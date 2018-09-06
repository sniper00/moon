#pragma once
#include "config.h"
#include "common/string.hpp"
#include "common/exception.hpp"
#include "rapidjson/document.h"
#include "rapidjson/rapidjson_helper.hpp"

namespace moon
{
    template<typename TService>
    class service_config
    {
        rapidjson::Document doc;
    public:
        bool parse(TService* s,const string_view_t& config)
        {
            doc.Parse(config.data(),config.size());
            if (doc.HasParseError() || !doc.IsObject())
            {
                CONSOLE_ERROR(s->logger(), "Lua service parse config %s failed,errorcode %d", std::string{ config.data(),config.size() }, doc.GetParseError());
                return false;
            }

            s->set_name(rapidjson::get_value<std::string>(&doc, "name"));

            //parse network config
            if (doc.HasMember("network"))
            {
                std::string compname = rapidjson::get_value<std::string>(&doc, "network.name");
                auto timeout = rapidjson::get_value<int32_t>(&doc, "network.timeout", 0);
                auto ip = rapidjson::get_value<std::string>(&doc, "network.ip");
                auto port = rapidjson::get_value<std::string>(&doc, "network.port");
                auto type = rapidjson::get_value<std::string>(&doc, "network.type","listen");
                auto protocol = rapidjson::get_value<std::string>(&doc, "network.protocol","default");

                if (ip.empty() || port.empty())
                {
                    CONSOLE_ERROR(s->logger(), "service %s add network component failed. ip or port is null", s->name());
                    return false;
                }

                auto n = s->template add_component<moon::tcp>(compname);
                n->setprotocol(protocol);
                n->settimeout(timeout);
                if (type == "listen")
                {
                    n->listen(ip, port);
                }
            }
            return true;
        }

        template<typename T>
        T get_value(moon::string_view_t name)
        {
            return rapidjson::get_value<T>(&doc, name);
        }
    };
}