#include "MysqlProxy.h"
#include "log.h"
#include "common/buffer_reader.hpp"
#include "common/buffer_writer.hpp"
#include "rapidjson/document.h"

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
	rapidjson::Document doc;
	doc.Parse(config.data());
	if (doc.HasParseError())
	{
		auto r = doc.GetParseError();
		CONSOLE_ERROR("mysql service parse config %s failed,errorcode %d", config.data(), r);
		exit(true);
		return false;
	}

	if (!doc.HasMember("mysql"))
	{
		CONSOLE_ERROR("mysql service init failed, can not find mysql config. %s",config.data());
		exit(true);
		return false;
	}

	auto& conf = doc["mysql"];

	if (!conf.HasMember("host") ||
		!conf.HasMember("user") ||
		!conf.HasMember("password") ||
		!conf.HasMember("database"))
	{
		CONSOLE_ERROR_TAG("mysql", "mysql connect config must have :host,user,password,database");
		exit(true);
		return false;
	}

	std::string host,user,password,database;
	int port = 3306;

	{
		auto& v = conf["host"];
		if (v.IsString())
		{
			host = v.GetString();
		}
	}

	{
		auto& v = conf["port"];
		if (v.IsInt())
		{
			port = v.GetInt();
		}
	}

	{
		auto& v = conf["user"];
		if (v.IsString())
		{
			user = v.GetString();
		}
	}
	
	{
		auto& v = conf["password"];
		if (v.IsString())
		{
			password = v.GetString();
		}
	}

	{
		auto& v = conf["database"];
		if (v.IsString())
		{
			database = v.GetString();
		}
	}

	if (doc.HasMember("name"))
	{
		rapidjson::Value& v = doc["name"];
		if (v.IsString())
		{
			set_name(v.GetString());
		}
	}

	try
	{
		conn_.Initialize(host,port,user,password,database);
	}
	catch (const std::logic_error& e)
	{
		CONSOLE_ERROR_TAG("mysql", e.what());
		exit(true);
		return false;
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
	broadcast(msg);

	timer_.repeat(30 * 1000, -1, [this](uint64_t timerid) {
		conn_.Ping();
	});

	mysql_db_.ThreadStart();
}

void MysqlProxy::update()
{
	service::update();
	timer_.update();
}

void MysqlProxy::destory()
{
	moon::service::destory();
	mysql_db_.ThreadEnd();
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
			conn_.QueryJson(sql.data(),*msg);
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

