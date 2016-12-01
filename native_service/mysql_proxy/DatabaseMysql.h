#pragma once
#include <memory>
#include <tuple>
#include <unordered_map>
#include "StmpBind.hpp"
#include "DataTable.hpp"

template<int...>
struct IndexTuple {};

template<int N, int... Indexes>
struct MakeIndexes :MakeIndexes<N - 1, N - 1, Indexes... > {};

template<int... Indexes>
struct MakeIndexes<0, Indexes...>
{
	typedef IndexTuple<Indexes...> type;
};

class MySqlStmt;
using MySqlStmtPtr = std::shared_ptr<MySqlStmt>;

class DatabaseMysql
{
public:
	DatabaseMysql();
	~DatabaseMysql();
	
	//初始化数据库
	void Initialize();

	// must be call before first query in thread
	void ThreadStart();

	// must be call before finish thread run
	void ThreadEnd();

private:
	static size_t m_dbCount;
};

class MySQLConnection
{
public:
	MySQLConnection();
	~MySQLConnection();
	MySQLConnection(const MySQLConnection&) = delete;
	MySQLConnection& operator=(const MySQLConnection&) = delete;

	//连接字符串 host;port;username;password;database
	void Initialize(const char* infoString);

	/*Execute
	*
	*
	*/
	void Execute(const std::string& sql);

	template<typename... Args>
	void Execute(const std::string& sql, Args&&... args)
	{
		std::vector<BindData> datas;
		BindParams(datas, std::forward<Args>(args)...);

		MySqlStmtPtr stmt;
		if (!Prepare(sql,stmt))
			return false;

		return ExecuteStmt(stmt, datas);
	}

	void Prepare(const std::string& sql, MySqlStmtPtr& stmt);

	void ExecuteStmt(const MySqlStmtPtr& stmt, std::vector<BindData>& datas);

	template<typename Tuple>
	void ExecuteTuple(const std::string& sql,Tuple&& t)
	{
		MySqlStmtPtr stmt;
		if (!Prepare(sql, stmt))
		{
			return false;
		}
		return ExecuteTuple(stmt, MakeIndexes<std::tuple_size<std::remove_reference<Tuple>::type>::value>::type(), std::forward<Tuple>(t));
	}

	void ExecuteJson(const std::string& sql, const std::string& jsonData,const std::string& order);

	/*
	*Transaction
	*
	*/
	std::pair<bool, std::string> Begin();

	std::pair<bool, std::string> Rollback();

	std::pair<bool, std::string> Commit();

	/*
	*query
	*
	*/
	std::shared_ptr<DataTable> Query(const char* sql);

	std::string QueryJson(const char* sql);

	bool Ping();
protected:
	std::pair<bool,std::string> TransactionSql(const char* sql);

	template<int... Indexes, class Tuple>
	void ExecuteTuple(const MySqlStmtPtr& stmt, IndexTuple<Indexes... >&& in, Tuple&& t)
	{
		std::vector<BindData> datas;
		BindParams(datas, std::get<Indexes>(std::forward<Tuple>(t))...);
		return ExecuteStmt(stmt, datas);
	}
private:
	struct  Imp;
	std::shared_ptr<Imp> m_Imp;
};
