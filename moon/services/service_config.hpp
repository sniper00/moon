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
        bool parse(TService* s,  string_view_t config)
        {
            doc.Parse(config.data(), config.size());
            if (doc.HasParseError() || !doc.IsObject())
            {
                CONSOLE_ERROR(s->logger(), "Lua service parse config %s failed,errorcode %d", std::string{ config.data(),config.size() }, doc.GetParseError());
                return false;
            }

            s->set_name(rapidjson::get_value<std::string>(&doc, "name"));

            //parse network config
            for (auto& v : doc.GetObject())
            {
                auto name = rapidjson::detail::get_value<std::string_view>(&v.name);
                if (name.find("network") != std::string_view::npos)
                {
                    auto timeout = rapidjson::get_value<int32_t>(&v.value, "timeout", 0);
                    auto ip = rapidjson::get_value<std::string>(&v.value, "ip");
                    auto port = rapidjson::get_value<std::string>(&v.value, "port");
                    auto type = rapidjson::get_value<std::string>(&v.value, "type", "listen");
                    auto protocol = rapidjson::get_value<std::string>(&v.value, "protocol", "default");
                    auto frame_flag = rapidjson::get_value<std::string>(&v.value, "frame_flag", "none");

                    if (ip.empty() || port.empty())
                    {
                        CONSOLE_ERROR(s->logger(), "service %s add network component failed. ip or port is null", s->name().data());
                        return false;
                    }

                    auto n = s->get_tcp(protocol);
                    n->settimeout(timeout);
                    n->set_enable_frame(frame_flag);
                    if (type == "listen")
                    {
                        if (!n->listen(ip, port))
                        {
                            return false;
                        }
                    }
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