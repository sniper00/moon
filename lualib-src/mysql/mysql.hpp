#pragma once
#include <type_traits>
#include <cstdint>
#include <string>
#include <memory>
#include <unordered_map>
#include <sstream>
#include <string_view>
#include <cstring>
#include <cstdarg>
#include <errmsg.h>
#include "mysql.h"
#include "data_table.hpp"

namespace db
{
	class mysql
	{
		struct stmt
		{
		public:
			stmt()
				:nparams(0)
				, ncolumns(0)
				, mysql_res(nullptr)
				, mysql_stmt(nullptr)
			{

			}

			~stmt()
			{
				if (nullptr != mysql_res)
				{
					mysql_free_result(mysql_res);
				}

				if (nullptr != mysql_stmt)
				{
					mysql_stmt_close(mysql_stmt);
				}
			}

			uint32_t nparams;
			uint32_t ncolumns;
			MYSQL_RES* mysql_res;
			MYSQL_STMT*	 mysql_stmt;
			std::string sql;
		};

		using stmt_ptr = std::shared_ptr<stmt>;

		MYSQL* mysql_=nullptr;
		std::unordered_map<size_t, stmt_ptr> stmts_;
	public:
		mysql() = default;
		mysql(const mysql&) = delete;
		mysql& operator=(mysql&) = delete;

		~mysql()
		{
			if (nullptr != mysql_)
			{
				mysql_close(mysql_);
				mysql_ = nullptr;
			}
		}

		void connect(const std::string& host, int port, const std::string& user, const std::string& password, const std::string& database, int timeout = 0)
		{
			if (nullptr != mysql_)
			{
				mysql_close(mysql_);
			}

			auto init_conn = mysql_init(nullptr);
			if (nullptr == init_conn)
			{
				format_throw("mysql init failed");
			}

			if (timeout > 0)
			{
				if (mysql_options(init_conn, MYSQL_OPT_CONNECT_TIMEOUT, &timeout))
				{
					format_throw("mysql_options MYSQL_OPT_CONNECT_TIMEOUT failed");
				}
			}

			mysql_ = mysql_real_connect(init_conn, host.data(), user.data(), password.data(), database.data(), port, nullptr, CLIENT_MULTI_STATEMENTS);//CLIENT_MULTI_STATEMENTS
			if (nullptr == mysql_)
			{
				std::string err = mysql_error(init_conn);
				mysql_close(init_conn);
				format_throw("ERROR: %s",err.data());
			}

			char value = 1;
			mysql_options(mysql_, MYSQL_OPT_RECONNECT, &value);
		}

		bool connected() const noexcept
		{
			if (nullptr != mysql_)
			{
				auto ec = mysql_errno(mysql_);
				return (ec != CR_SERVER_LOST && ec != CR_SERVER_GONE_ERROR);
			}
			return false;
		}

		int error_code() const noexcept
		{
			return mysql_errno(mysql_);
		}

		int ping() const noexcept
		{
			if (nullptr != mysql_)
			{
				return mysql_ping(mysql_);
			}
			return 0;
		}

		void execute(const std::string& sql) const
		{
			do
			{
				if (nullptr == mysql_)
				{
					format_throw("SQL: mysql not init");
				}

				if (mysql_query(mysql_, sql.data()))
				{
					format_throw("Execute SQL:%s. ERROR:%s", sql.data(), mysql_error(mysql_));
				}

				while (!mysql_next_result(mysql_))
				{
					auto result = mysql_store_result(mysql_);
					if (nullptr != result)
					{
						printf("get more result, should use query.");
						mysql_free_result(result);
					}
				}
			} while (0);
		}

		template<typename... Args>
		constexpr void execute_stmt(size_t stmtid, Args&&... args)
		{
			std::vector<MYSQL_BIND> binds;
			((set_param_bind(binds, std::forward<Args>(args))), ...);
			execute_stmt_param(stmtid, binds);
		}

		template<typename TablePolicy,typename... PolicyArgs>
		std::shared_ptr<TablePolicy> query(const std::string& sql, PolicyArgs&&... args) const
		{
			if (nullptr == mysql_)
			{
				format_throw("SQL: mysql not init");
				return nullptr;
			}

			std::shared_ptr<TablePolicy> dt;

			MYSQL_RES* result = nullptr;
			MYSQL_FIELD* fields = nullptr;

			do
			{
				size_t nrow = 0;
				size_t nfield = 0;

				if (mysql_query(mysql_, sql.data()))
				{
					format_throw("Query Sql Failed:%s. ERROR: %s", sql.data(), mysql_error(mysql_));
				}

				result = mysql_store_result(mysql_);
				if (nullptr == result)
				{
					format_throw("Query Sql Failed:%s. store result ERROR: %s", sql.data(), mysql_error(mysql_));
				}

				nrow = mysql_affected_rows(mysql_);
				if (0 == nrow)
				{
					mysql_free_result(result);
					format_throw("Query Sql Failed:%s. result rowCount 0", sql.data());
				}

				nfield = mysql_field_count(mysql_);
				fields = mysql_fetch_fields(result);

				dt = std::make_shared<TablePolicy>(std::forward<PolicyArgs>(args)...);

				for (uint32_t i = 0; i < nfield; i++)
				{
					dt->add_column(fields[i].name, fields[i].type);
				}

				MYSQL_ROW row;
				row = mysql_fetch_row(result);
				while (row != nullptr)
				{
					dt->add_row(row);
					row = mysql_fetch_row(result);
				}
			} while (0);

			if (nullptr != result)
			{
				mysql_free_result(result);
			}

			while (!mysql_next_result(mysql_))
			{
				result = mysql_store_result(mysql_);
				if (nullptr != result)
				{
					printf("get more result");
					mysql_free_result(result);
				}
			}
			return dt;
		}

		void execute_stmt_param(size_t id, std::vector<MYSQL_BIND>& params) const
		{
			stmt_ptr m = nullptr;
			if (auto iter = stmts_.find(id); iter == stmts_.end())
			{
				format_throw("SQL: can not found prepare stmt for '%zu'", id);
			}
			else
			{
				m = iter->second;
			}

			if (m->nparams != params.size())
			{
				format_throw("SQL: no enough params for '%s'", m->sql.data());
			}

			// bind input arguments
			if (mysql_stmt_bind_param(m->mysql_stmt, params.data()))
			{
				format_throw("SQL ERROR: mysql_stmt_bind_param() failed.SQL ERROR: %s", mysql_stmt_error(m->mysql_stmt));
			}

			if (mysql_stmt_execute(m->mysql_stmt))
			{
				format_throw("SQL: cannot execute '%s'. SQL ERROR: %s", m->sql.data(), mysql_stmt_error(m->mysql_stmt));
			}
		}

		size_t prepare(const std::string& sql)
		{
			if (nullptr == mysql_)
			{
				format_throw("SQL: mysql not init");
				return 0;
			}

			size_t id = std::hash<std::string>()(sql);

			if (auto iter = stmts_.find(id); iter != stmts_.end())
			{
				return id;
			}

			auto m = std::make_shared<stmt>();
			m->sql = std::move(sql);

			// create statement object
			m->mysql_stmt = mysql_stmt_init(mysql_);
			if (nullptr == m->mysql_stmt)
			{
				format_throw("SQL: mysql_stmt_init() failed. %s", sql.data());
			}

			// prepare statement
			if (mysql_stmt_prepare(m->mysql_stmt, m->sql.c_str(), (unsigned long)m->sql.size()))
			{
				format_throw("SQL: mysql_stmt_prepare() failed for '%s'. SQL ERROR: %s", sql.data(), mysql_stmt_error(m->mysql_stmt));
			}

			/* Get the parameter count from the statement */
			m->nparams = mysql_stmt_param_count(m->mysql_stmt);

			/* Fetch result set meta information */
			m->mysql_res = mysql_stmt_result_metadata(m->mysql_stmt);

			// check if we have a statement which returns result sets
			if (nullptr != m->mysql_res)
			{
				/* Get total columns in the query */
				m->ncolumns = mysql_num_fields(m->mysql_res);
			}
			stmts_.emplace(id, m);
			return id;
		}

	private:
		void format_throw(const char* fmt, ...) const
		{
			if (nullptr == fmt)
			{
				return;
			}
			va_list ap;
			char szBuffer[8192];
			va_start(ap, fmt);
			// win32
#if defined(_WIN32)
			vsnprintf_s(szBuffer, sizeof(szBuffer), fmt, ap);
#endif
			// linux
#if defined(__linux__) || defined(__linux) 
			vsnprintf(szBuffer, sizeof(szBuffer), fmt, ap);
#endif
			va_end(ap);
			throw std::logic_error(szBuffer);
		}

		
		template<typename T>
		constexpr void set_param_bind(std::vector<MYSQL_BIND>& param_binds, T&& value) {
			MYSQL_BIND param = {};

			using U = std::remove_const_t<std::remove_reference_t<T>>;
			if constexpr(std::is_arithmetic_v<U>) {
				param.buffer_type = (enum_field_types)type_id_map<U>::value;
				param.buffer = const_cast<void*>(static_cast<const void*>(&value));
			}
			else if constexpr(std::is_same_v<std::string, U>) {
				param.buffer_type = MYSQL_TYPE_STRING;
				param.buffer = (void*)(value.c_str());
				param.buffer_length = (unsigned long)value.size();
			}
			else if constexpr(std::is_same_v<const char*, U> || is_char_array_v<U>) {
				param.buffer_type = MYSQL_TYPE_STRING;
				param.buffer = (void*)(value);
				param.buffer_length = (unsigned long)strlen(value);
			}
			param_binds.push_back(param);
		}
	};
}