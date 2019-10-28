#pragma once
#include "config.hpp"
#include "common/string.hpp"
#include "common/exception.hpp"
#include "rapidjson/document.h"
#include "rapidjson/rapidjson_helper.hpp"

namespace moon
{
    template<typename TService>
    class service_config_parser
    {
        rapidjson::Document doc;
    public:
        bool parse(TService* s,  string_view_t config)
        {
            doc.Parse(config.data(), config.size());
            if (doc.HasParseError() || !doc.IsObject())
            {
                CONSOLE_ERROR(s->logger(), "Lua service parse config %s failed,errorcode %d", std::string{ config.data(),config.size() }.data(), doc.GetParseError());
                return false;
            }

            s->set_name(rapidjson::get_value<std::string>(&doc, "name"));
            return true;
        }

        template<typename T>
        T get_value(string_view_t name)
        {
            return rapidjson::get_value<T>(&doc, name);
        }
    };
}