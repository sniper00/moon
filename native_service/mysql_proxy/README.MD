# Mysql库使用示例
```cpp
#include "DatabaseMysql.h"

int main()
{
	{
		DatabaseMysql dm;
		dm.Initialize();

		MySQLConnection mc;
		mc.Initialize("localhost;3306;root;4321;test");

		
		mc.Execute("CREATE TABLE test_table(col1 INT,\
		col2 VARCHAR(40),\
		col3 SMALLINT,\
		col4 TIMESTAMP)");

		const char* sql = "INSERT INTO test_table(col1,col2,col3) VALUES(?,?,?)";

		//执行 json数据
		if (mc.ExecuteJson(sql, R"(
		[{"ID":1,"name":"ww1","num":123},
		{"ID":2,"name":"ww2","num":123},
		{"ID":3,"name":"ww3","num":123}])"))
		{
			printf("success\r\n");
		}

		//可变参数
		mc.Execute(sql, 100, "sdada", 500);

		//元组
		auto tup = std::make_tuple(100, "sdada", 500);
		mc.ExecuteTuple(sql, tup);
	
		//查询返回
		{
			auto Ret = mc.Query("select * from test_table");
			if (Ret != nullptr)
			{
				for (size_t i = 0; i < Ret->Rows.Size(); i++)
				{
					for (size_t j = 0; j < Ret->Columns.Size(); j++)
					{
						printf("%s ", Ret->Rows[i][j].ToString().c_str());
					}
					printf("\r\n");
				}
			}
		}
	}
	return 0;
}
```
