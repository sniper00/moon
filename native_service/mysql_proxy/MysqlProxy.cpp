#include "MysqlProxy.h"
#include "common/string.hpp"
#include "log.h"
#include "common/buffer_reader.hpp"
#include "common/buffer_writer.hpp"

constexpr int16_t QUERY = 1;
constexpr int16_t EXECUTE = 2;

MysqlProxy::MysqlProxy()
{
}


MysqlProxy::~MysqlProxy()
{
}

bool MysqlProxy::init(const std::string & config)
{
	moon::service::init(config);

	set_name(get_config("name"));
	mysql_db_.Initialize();
	if (!check_config("host") ||
		!check_config("port") ||
		!check_config("username") ||
		!check_config("password") ||
		!check_config("database"))
	{
		CONSOLE_ERROR_TAG("mysql","mysql connect string must have :host,port,username,password,database");
		return false;
	}

	auto str = moon::format("%s;%s;%s;%s;%s"
		,get_config("host").data()
		, get_config("port").data()
		, get_config("username").data()
		, get_config("password").data()
		, get_config("database").data()
	);

	try
	{
		conn_.Initialize(str.data());
	}
	catch (const std::logic_error& e)
	{
		CONSOLE_ERROR_TAG("mysql", e.what());
		exit(true);
	}
	return true;
}

void MysqlProxy::start()
{
	moon::service::start();
	auto msg = moon::message::create();
	moon::buffer_writer<moon::buffer> bw(*msg);
	bw.write(uint16_t(60001));
	bw.write(name());
	bw.write(serviceid());
	msg->set_type(moon::message_type::service_send);
	broadcast_ex(msg);

	timer_.repeat(30 * 1000, -1, [this]() {
		conn_.Ping();
	});
}

void MysqlProxy::update()
{
	service::update();
	timer_.update();
}

void MysqlProxy::destory()
{
	moon::service::destory();
}

void MysqlProxy::on_message(moon::message * msg)
{
	if (msg->type() != moon::message_type::service_send)
		return;

	auto rpc = msg->rpc();
	auto sender = msg->sender();

	moon::buffer_reader br(msg->data(), msg->size());
	auto msgID = br.read<uint16_t>();
	if (msgID != 65001)
	{
		return;
	}

	auto op = br.read<int16_t>();
	auto sql = br.read<std::string>();
	if (op == QUERY)
	{	
		msg->reset();
		moon::buffer_writer<moon::buffer> bw(*msg);
		try
		{
			auto ret = conn_.QueryJson(sql.data());
			bw.write(int16_t(1));
			bw.write(ret);
		}
		catch (const std::logic_error& e)
		{
			bw.write(int16_t(0));
			bw.write(e.what());
		}
		msg->set_rpc(rpc);
		msg->set_type(moon::message_type::service_response);
		msg->set_receiver(sender);
	}
	else if (op == EXECUTE)
	{
		auto jsonparam = br.read<std::string>();
		auto orders = br.read<std::string>();
		msg->reset();

		moon::buffer_writer<moon::buffer> bw(*msg);
		try
		{
			conn_.ExecuteJson(sql, jsonparam, orders);
			bw.write(int16_t(1));
		}
		catch (const std::logic_error& e)
		{
			bw.write(int16_t(0));
			bw.write(e.what());
		}
		msg->set_rpc(rpc);
		msg->set_type(moon::message_type::service_response);
		msg->set_receiver(sender);
	}
}

moon::service_ptr_t MysqlProxy::clone()
{
	return moon::service_ptr_t();
}
