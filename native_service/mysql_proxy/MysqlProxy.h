#pragma once
#include "message.h"
#include "service.h"
#include "DatabaseMysql.h"
#include "components/timer/timer.h"

class MysqlProxy:public moon::service
{
public:
	MysqlProxy();
	~MysqlProxy();

	bool init(const std::string& config) override;

	void start() override;

	void update() override;

	void destory() override;

	void on_message(moon::message* msg)  override;

private:
	DatabaseMysql mysql_db_;
	MySQLConnection conn_;
	moon::timer timer_;
};

