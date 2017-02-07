#include "DatabaseMysql.h"
#include "config.h"
#include "mysql.h"
#include "common/string.hpp"
#include "common/buffer.hpp"
#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"
#include "log.h"

DataType ToValueType(enum_field_types mysqlType);

using namespace rapidjson;

class MySqlStmt
{
public:
	MySqlStmt()
		:ResultMetadata(nullptr),
		Stmt(nullptr)
	{

	}

	~MySqlStmt()
	{
		if (Stmt != nullptr)
		{
			mysql_free_result(ResultMetadata);
			mysql_stmt_close(Stmt);
		}
	}

	MYSQL_RES*		ResultMetadata;
	MYSQL_STMT*		Stmt;
	uint32_t				Params;
	uint32_t				Columns;
	bool						IsQuery;
	bool						Prepared;
	std::string				Sql;
};

size_t DatabaseMysql::m_dbCount = 0; 

DatabaseMysql::DatabaseMysql()
{
}

DatabaseMysql::~DatabaseMysql()
{
	// Free Mysql library pointers for last ~DB
	if (--m_dbCount == 0)
		mysql_library_end();
}

void DatabaseMysql::Initialize()
{
	// before first connection
	if (m_dbCount++ == 0)
	{
		// Mysql Library Init
		mysql_library_init(-1, nullptr, nullptr);
		if (!mysql_thread_safe())
		{
			throw std::logic_error("FATAL ERROR: Used MySQL library isn't thread-safe.");
		}
	}
}

void DatabaseMysql::ThreadStart()
{
	mysql_thread_init();
}

void DatabaseMysql::ThreadEnd()
{
	mysql_thread_end();
}

struct MySQLConnection::Imp
{
	Imp()
		:MySql(nullptr)
	{

	}
	MYSQL* MySql;
	std::unordered_map<std::string, MySqlStmtPtr> PreparedStmts;
};

MySQLConnection::MySQLConnection()
	:m_Imp(std::make_shared<Imp>())
{

}

MySQLConnection::~MySQLConnection()
{
	Close();
}

void MySQLConnection::Initialize(const std::string& host, int port, const std::string& user, const std::string& password, const std::string& database)
{
	auto mySqlInit = mysql_init(nullptr);
	if (nullptr == mySqlInit)
	{
		throw std::logic_error("Could not initialize Mysql connection");
	}

	m_Imp->MySql =  mysql_real_connect(mySqlInit, host.data(), user.data(),password.data(), database.data(), port, nullptr, 0);//CLIENT_MULTI_STATEMENTS

	if (nullptr == m_Imp->MySql)
	{
		mysql_close(mySqlInit);
		throw std::logic_error(moon::format("Could not connect to MySQL database at %s: %s",
			host.c_str(), mysql_error(mySqlInit)));
	}

	CONSOLE_TRACE_TAG("mysql","Connected to MySQL database %s@%s:%d/%s", user.c_str(), host.c_str(), port, database.c_str());
	CONSOLE_TRACE_TAG("mysql", "MySQL client library: %s", mysql_get_client_info());
	CONSOLE_TRACE_TAG("mysql", "MySQL server version: %s", mysql_get_server_info(m_Imp->MySql));


	if (!mysql_autocommit(m_Imp->MySql, 1))
	{
		CONSOLE_TRACE_TAG("mysql", "AUTOCOMMIT SUCCESSFULLY SET TO 1");
	}
	else
	{
		CONSOLE_WARN_TAG("mysql","AUTOCOMMIT NOT SET TO 1");
	}

	char value = 1;
	mysql_options(m_Imp->MySql, MYSQL_OPT_RECONNECT, &value);
		
	Execute("SET NAMES `utf8`\n");
	Execute("SET CHARACTER SET `utf8`\n");
}

void MySQLConnection::Close()
{
	m_Imp->PreparedStmts.clear();
	if (nullptr != m_Imp->MySql)
	{
		mysql_close(m_Imp->MySql);
		m_Imp->MySql = nullptr;
	}
}

void MySQLConnection::Execute(const std::string& sql)
{
	do 
	{
		if (nullptr == m_Imp->MySql)
			break;

		if (mysql_query(m_Imp->MySql, sql.data()))
		{
			throw std::logic_error(moon::format("Execute SQL:%s. EEEROR", sql.c_str(), mysql_error(m_Imp->MySql)));
		}
	} while (0);
}

void MySQLConnection::Prepare(const std::string& sql, MySqlStmtPtr& stmt)
{
	auto iter = m_Imp->PreparedStmts.find(sql);
	if (iter != m_Imp->PreparedStmts.end())
	{
		stmt = iter->second;
		return;
	}

	do 
	{
		if (nullptr == m_Imp->MySql)
			break;

		stmt = std::make_shared<MySqlStmt>();
		stmt->Sql = sql;
		// create statement object
		stmt->Stmt = mysql_stmt_init(m_Imp->MySql);
		if (nullptr == stmt->Stmt)
		{
			throw std::logic_error("SQL: mysql_stmt_init() failed");
			break;
		}

		// prepare statement
		if (mysql_stmt_prepare(stmt->Stmt, sql.c_str(), (unsigned long)sql.size()))
		{
			throw std::logic_error(moon::format("SQL: mysql_stmt_prepare() failed for '%s'. SQL ERROR: %s", sql.c_str(), mysql_stmt_error(stmt->Stmt)));
			break;
		}

		/* Get the parameter count from the statement */
		stmt->Params = mysql_stmt_param_count(stmt->Stmt);

		/* Fetch result set meta information */

		stmt->ResultMetadata = mysql_stmt_result_metadata(stmt->Stmt);
		// if we do not have result metadata
		std::string sqlop = sql.substr(0, 6);
		if (nullptr == stmt->ResultMetadata && moon::iequals(sqlop,"SELECT"))
		{
			throw std::logic_error(moon::format("SQL: no meta information for '%s'. SQL ERROR: %s", sql.c_str(), mysql_stmt_error(stmt->Stmt)));
			break;
		}

		// check if we have a statement which returns result sets
		if (stmt->ResultMetadata)
		{
			// our statement is query
			stmt->IsQuery = true;
			/* Get total columns in the query */
			stmt->Columns = mysql_num_fields(stmt->ResultMetadata);
			// bind output buffers
		}
		m_Imp->PreparedStmts.emplace(sql, stmt);
		return;
	} while (0);
	stmt.reset();
}

std::pair<enum_field_types, char> ToMySqlType(NativeType cppType);
void MySQLConnection::ExecuteStmt(const MySqlStmtPtr& stmt, std::vector<BindData>& datas)
{
	if (stmt->Params != datas.size())
	{
		throw std::logic_error(moon::format("SQL: no enough params for '%s'", stmt->Sql.c_str()));
		return;
	}

	std::vector<MYSQL_BIND> mysqlBinds;
	for (auto& d : datas)
	{
		auto type = ToMySqlType(d.Type);
		mysqlBinds.emplace_back(MYSQL_BIND());
		auto& Param = mysqlBinds.back();
		memset(&Param, 0, sizeof(MYSQL_BIND));
		Param.buffer_type = type.first;
		Param.is_null = 0;
		Param.is_unsigned = type.second;	
		Param.length = 0;
		Param.buffer = d.Data;
		Param.buffer_length =(unsigned long)d.Size;
	}

	// bind input arguments
	if (mysql_stmt_bind_param(stmt->Stmt, mysqlBinds.data()))
	{
		throw std::logic_error(moon::format("SQL ERROR: mysql_stmt_bind_param() failed.SQL ERROR: %s", mysql_stmt_error(stmt->Stmt)));
		return;
	}

	if (mysql_stmt_execute(stmt->Stmt))
	{
		throw std::logic_error(moon::format("SQL: cannot execute '%s'. SQL ERROR: %s", stmt->Sql.c_str(), mysql_stmt_error(stmt->Stmt)));
		return;
	}
}

void MySQLConnection::ExecuteJson(const std::string& sql, const std::string& jsonData, const std::string& order)
{
	MySqlStmtPtr stmt;

	Prepare(sql, stmt);

	auto field_count = stmt->Stmt->param_count;

	std::vector<std::string> orders;
	if (order.size() > 0)
	{
		orders = moon::split<std::string>(order, ",");
		if (orders.size() != field_count)
		{
			throw std::logic_error(moon::format("ExecuteJson order size != fields_count Sql:%s. Order:%s", sql.data(), order.data()));
		}
	}

	Document doc;
	doc.Parse(jsonData.data());
	if (!doc.IsArray())
	{
		throw std::logic_error(moon::format("ExecuteJson Parse Failed for Sql:%s. Data:%s", sql.data(),jsonData.data()));
	}

	auto beginret = Begin();
	if (!beginret.first)
	{
		throw std::logic_error(beginret.second);
	}
	
	try
	{
		std::vector<int> orderIndex;
		std::vector<BindData> bindDatas(field_count);
		for (auto& arrItem : doc.GetArray())
		{
			auto& obj = arrItem.GetObjectW();
			unsigned int i = 0;
			for (auto& it : orders)
			{
				if (i >= field_count)
				{
					throw std::logic_error("json field count does not equal sql field count");
				}

				auto& bd = bindDatas[i];
				i++;

				auto jv = obj.FindMember(it.data());
				if (jv == obj.end())
				{
					throw std::logic_error(moon::format("json data key dose not have named: %s",it.data()));
				}

				auto& value = jv->value;

				if (value.IsInt64())
				{
					bd.Type = NativeType::INT64;
					bd.Value = value.GetInt64();
					bd.Data = bd.Value.CastPointer<int64_t>();
				}
				else if (value.IsUint64())
				{
					bd.Type = NativeType::UINT64;
					bd.Value = value.GetUint64();
					bd.Data = bd.Value.CastPointer<uint64_t>();
				}
				else if (value.IsDouble())
				{
					bd.Type = NativeType::DOUBLE;
					bd.Value = value.GetDouble();
					bd.Data = bd.Value.CastPointer<double>();
				}
				else if(value.IsBool())
				{
					bd.Type = NativeType::UINT8;
					bd.Value = (uint8_t)value.GetBool();
					bd.Data = bd.Value.CastPointer<uint8_t>();
				}
				else if (value.IsString())
				{
					bd.Type = NativeType::STRING;
					bd.Data = (char*)value.GetString();
					bd.Size = value.GetStringLength();
				}
			}
			ExecuteStmt(stmt, bindDatas);
		}
		auto commitret = Commit();
		if (!commitret.first)
		{
			throw std::logic_error(beginret.second);
		}
	}
	catch (const std::logic_error& e)
	{
		auto rbret=  Rollback();
		if (!rbret.first)
		{
			CONSOLE_ERROR("Rollback Failed %s", rbret.second.data());
		}
		throw e;
	}
}

std::pair<bool, std::string> MySQLConnection::Begin()
{
	return TransactionSql("START TRANSACTION");
}

std::pair<bool, std::string> MySQLConnection::Rollback()
{
	return TransactionSql("ROLLBACK");
}

std::pair<bool, std::string> MySQLConnection::Commit()
{
	return TransactionSql("COMMIT");
}

std::shared_ptr<DataTable> MySQLConnection::Query(const char* sql)
{
	std::shared_ptr<DataTable> dt;

	MYSQL_RES* result = nullptr;
	MYSQL_FIELD* fields = nullptr;

	do 
	{
		uint64_t rowCount = 0;
		uint32_t fieldCount = 0;

		if (mysql_query(m_Imp->MySql, sql))
		{
			throw std::logic_error(moon::format("Query Sql Failed:%s. ERROR: %s", sql, mysql_error(m_Imp->MySql)));
			break;
		}
		
		result = mysql_store_result(m_Imp->MySql);
		if (nullptr == result)
		{
			throw std::logic_error(moon::format("Query Sql Failed:%s. store result ERROR: %s", sql, mysql_error(m_Imp->MySql)));
			break;
		}

		rowCount = mysql_affected_rows(m_Imp->MySql);
		if (0 == rowCount)
		{
			mysql_free_result(result);
			throw std::logic_error(moon::format("Query Sql Failed:%s. result rowCount 0", sql));
			break;
		}

		fieldCount = mysql_field_count(m_Imp->MySql);
		fields = mysql_fetch_fields(result);

		dt = std::make_shared<DataTable>();

		for (uint32_t i = 0; i < fieldCount; i++)
		{
			dt->Columns.Add(fields[i].name, ToValueType(fields[i].type));
		}

		MYSQL_ROW row;
		row = mysql_fetch_row(result);
		size_t  col = 0;
		while (row != nullptr)
		{
			auto& r = dt->Rows.Add();
			r.Resize(fieldCount);

			for (size_t col = 0; col < fieldCount; col++)
			{
				switch (dt->Columns[col].type())
				{
				case DataType::number_integer:
					r[col] = (row[col] != nullptr) ? std::stoull(row[col]) : 0;
					break;
				case DataType::number_double:
					r[col] = (row[col] != nullptr) ? std::stod(row[col]) : 0.0;
					break;
				case DataType::string:
					r[col] = (row[col] != nullptr) ? row[col]: "";
					break;
				case DataType::boolean:
					r[col] = (row[col] != nullptr) ? (std::stoul(row[col]) !=0):false;
					break;
				default:
					break;
				}	
			}
			row = mysql_fetch_row(result);
		}
	} while (0);

	if (nullptr != result)
	{
		mysql_free_result(result);
	}
	
	return dt;
}

void MySQLConnection::QueryJson(const char * sql,moon::buffer& buf)
{

	Document  doc;
	doc.SetArray();
	auto& allocator = doc.GetAllocator();
	DataColumns dtCol;
	MYSQL_RES* result = nullptr;
	MYSQL_FIELD* fields = nullptr;
	do
	{
		uint64_t rowCount = 0;
		uint32_t fieldCount = 0;

		if (mysql_query(m_Imp->MySql, sql))
		{
			throw std::logic_error(moon::format("Query Sql Failed:%s. ERROR: %s", sql, mysql_error(m_Imp->MySql)));
			break;
		}

		result = mysql_store_result(m_Imp->MySql);
		if (nullptr == result)
		{
			throw std::logic_error(moon::format("Query Sql Failed:%s. store result ERROR: %s", sql, mysql_error(m_Imp->MySql)));
			break;
		}

		rowCount = mysql_affected_rows(m_Imp->MySql);
		if (0 == rowCount)
		{
			mysql_free_result(result);
			throw std::logic_error(moon::format("Query Sql Failed:%s. result rowCount 0", sql));
			break;
		}

		fieldCount = mysql_field_count(m_Imp->MySql);
		fields = mysql_fetch_fields(result);
		if (nullptr == fields || fieldCount == 0)
		{
			mysql_free_result(result);
			throw std::logic_error(moon::format("Query Sql Failed:%s. result fields 0", sql));
			break;
		}

		for (uint32_t i = 0; i < fieldCount; i++)
		{
			dtCol.Add(fields[i].name, ToValueType(fields[i].type));
		}
	
		MYSQL_ROW row;
		row = mysql_fetch_row(result);
	
		while (row != nullptr)
		{
			Value obj(kObjectType);
			for (size_t col = 0; col < fieldCount; col++)
			{
				auto name = dtCol[col].name().data();
				auto name_len = dtCol[col].name().size();
				auto v = row[col];

				switch (dtCol[col].type())
				{
				case DataType::number_integer:
					if (v != nullptr && std::strlen(v)!= 0)
					{
						if (v[0] == '-')
						{
							obj.AddMember(StringRef(name,name_len) , std::stoll(v), allocator);
						}
						else
						{
							obj.AddMember(StringRef(name, name_len),std::stoull(v), allocator);
						}		
					}
					else
					{
						obj.AddMember(StringRef(name, name_len),0, allocator);
					}
					break;
				case DataType::number_double:
					obj.AddMember(StringRef(name, name_len),(row[col] != nullptr) ? std::stod(v) : 0.0,allocator);
					break;
				case DataType::string:
					obj.AddMember(StringRef(name, name_len),StringRef(v,strlen(v)), allocator);
					break;
				case DataType::boolean:
					obj.AddMember(StringRef(name, name_len), (row[col] != nullptr) ? (std::stoul(row[col]) != 0) : false, allocator);
					break;
				default:
					break;
				}
			}
			doc.PushBack(obj,allocator);
			row = mysql_fetch_row(result);
		}
	} while (0);

	StringBuffer buffer;
	rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
	doc.Accept(writer);
	auto out = buffer.GetString();
	auto ret = int16_t(1);
	buf.write_back(&ret, 0, 1);
	buf.write_back(buffer.GetString(), 0, buffer.GetSize());

	if (nullptr != result)
	{
		mysql_free_result(result);
	}
}

bool MySQLConnection::Ping()
{
	if (m_Imp->MySql != nullptr)
		return 0 !=mysql_ping(m_Imp->MySql);
	return true;
}

std::pair<bool, std::string> MySQLConnection::TransactionSql(const char* sql)
{
	std::pair<bool, std::string> ret;
	if (mysql_query(m_Imp->MySql, sql))
	{
		ret.first = false;
		ret.second = moon::format("TransactionSql Failed:%s. SQL ERROR: %s", sql, mysql_error(m_Imp->MySql));
	}
	ret.first = true;
	return ret;
}

std::pair<enum_field_types, char> ToMySqlType(NativeType cppType)
{
	std::pair<enum_field_types, char> ret(MYSQL_TYPE_NULL, 0);

	switch (cppType)
	{
	case NativeType::NONE:
		ret.first = MYSQL_TYPE_NULL;
		ret.second = 0;
		break;
	case NativeType::UINT8:
		ret.first = MYSQL_TYPE_TINY;
		ret.second = 1;
		break;
	case NativeType::INT8:
		ret.first = MYSQL_TYPE_TINY;
		ret.second = 0;
		break;
	case NativeType::UINT16:
		ret.first = MYSQL_TYPE_SHORT;
		ret.second = 1;
		break;
	case NativeType::INT16:
		ret.first = MYSQL_TYPE_SHORT;
		ret.second = 0;
		break;
	case NativeType::UINT32:
		ret.first = MYSQL_TYPE_LONG;
		ret.second = 1;
		break;
	case NativeType::INT32:
		ret.first = MYSQL_TYPE_LONG;
		ret.second = 0;
		break;
	case NativeType::UINT64:
		ret.first = MYSQL_TYPE_LONGLONG;
		ret.second = 1;
		break;
	case NativeType::INT64:
		ret.first = MYSQL_TYPE_LONGLONG;
		ret.second = 0;
		break;
	case NativeType::FLOAT:
		ret.first = MYSQL_TYPE_FLOAT;
		ret.second = 0;
		break;
	case NativeType::DOUBLE:
		ret.first = MYSQL_TYPE_DOUBLE;
		ret.second = 0;
		break;
	case NativeType::STRING:
		ret.first = MYSQL_TYPE_STRING;
		ret.second = 0;
		break;
	}
	return ret;
}

DataType ToValueType(enum_field_types mysqlType)
{
	switch (mysqlType)
	{
	case FIELD_TYPE_TIMESTAMP:
	case FIELD_TYPE_DATE:
	case FIELD_TYPE_TIME:
	case FIELD_TYPE_DATETIME:
	case FIELD_TYPE_YEAR:
	case FIELD_TYPE_STRING:
	case FIELD_TYPE_VAR_STRING:
	case FIELD_TYPE_BLOB:
	case FIELD_TYPE_SET:
	case FIELD_TYPE_NULL:
		return DataType::string;
	case FIELD_TYPE_TINY:
	case FIELD_TYPE_SHORT:
	case FIELD_TYPE_LONG:
	case FIELD_TYPE_INT24:
	case FIELD_TYPE_LONGLONG:
	case FIELD_TYPE_ENUM:
		return DataType::number_integer;
	case FIELD_TYPE_DECIMAL:
	case FIELD_TYPE_FLOAT:
	case FIELD_TYPE_DOUBLE:
		return DataType::number_double;
	default:
		return DataType::null;
	}
}