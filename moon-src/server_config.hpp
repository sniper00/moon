#pragma once
#include "config.hpp"
#include "common/string.hpp"
#include "common/exception.hpp"
#include "rapidjson/cursorstreamwrapper.h"
#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/error/en.h"
#include "common/rapidjson_helper.hpp"

namespace moon
{
    struct server_config
    {
        int32_t sid = 0;
        int32_t thread = 0;
        std::string loglevel;
        std::string name;
        std::string bootstrap;
        std::string params;
        std::string log;
    };

    class server_config_manger
    {
    public:
        server_config_manger() = default;

        const std::string config()
        {
            return config_;
        }

        bool parse(const std::string& config, int32_t sid)
        {
            try
            {
                config_ = config;
                rapidjson::StringStream ss(config_.data());
                rapidjson::CursorStreamWrapper<rapidjson::StringStream> csw(ss);
                rapidjson::Document doc;
                doc.ParseStream<rapidjson::kParseCommentsFlag>(csw);
                MOON_CHECK(!doc.HasParseError(), moon::format("Parse server config failed:%s(%d).line %d col %d", rapidjson::GetParseError_En(doc.GetParseError()), doc.GetParseError(), csw.GetLine(), csw.GetColumn()));
                MOON_CHECK(doc.IsArray(), "Server config format error: must be json array.");
                for (auto&c : doc.GetArray())
                {
                    server_config scfg;
                    scfg.sid = rapidjson::get_value<int32_t>(&c, "sid", -1);//server id
                    MOON_CHECK(-1 != scfg.sid, "Server config format error:must has sid");
                    scfg.name = rapidjson::get_value<std::string>(&c, "name");//server name
                    MOON_CHECK(!scfg.name.empty(), "Server config format error:must has name");
                    scfg.thread = rapidjson::get_value<int32_t>(&c, "thread", std::thread::hardware_concurrency());
                    scfg.log = rapidjson::get_value<std::string>(&c, "log");
                    scfg.bootstrap = rapidjson::get_value<std::string>(&c, "bootstrap");
                    MOON_CHECK(!scfg.bootstrap.empty(), "Server config format error:must has bootstrap file");
                    scfg.loglevel = rapidjson::get_value<std::string>(&c, "loglevel", "DEBUG");
                    auto params = rapidjson::get_value<rapidjson::Value*>(&c, "params");
                    if (nullptr != params)
                    {
                        rapidjson::StringBuffer buffer;
                        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
                        params->Accept(writer);
                        scfg.params = std::string(buffer.GetString(), buffer.GetSize());
                    }

                    moon::replace(scfg.name, "#sid", std::to_string(scfg.sid));
                    moon::replace(scfg.log, "#sid", std::to_string(scfg.sid));

                    if (scfg.log.find("#date") != std::string::npos)
                    {
                        time_t now = std::time(nullptr);
                        std::tm m;
                        moon::time::localtime(&now, &m);
                        char buff[50 + 1] = { 0 };
                        std::strftime(buff, 50, "%Y-%m-%d-%H-%M-%S", &m);
                        moon::replace(scfg.log, "#date", buff);
                    }

                    MOON_CHECK(data_.emplace(scfg.sid, scfg).second, moon::format("Server config format error:sid %d already exist.", scfg.sid));
                }
                sid_ = sid;
                return true;
            }
            catch (std::exception& e)
            {
                std::cerr << e.what() << std::endl;
                return false;
            }
        }

        template<typename Handler>
        void for_all(Handler&& handler)
        {
            for (auto& c : data_)
            {
                handler(c.second);
            }
        }

        server_config* find(int32_t sid)
        {
            auto iter = data_.find(sid);
            if (iter != data_.end())
            {
                return &iter->second;
            }
            return nullptr;
        }

        server_config* get_server_config()
        {
            auto iter = data_.find(sid_);
            if (iter != data_.end())
            {
                return &iter->second;
            }
            return nullptr;
        }
    private:
        int sid_ = 0;
        std::string config_;
        std::unordered_map<int32_t, server_config> data_;
    };
}
