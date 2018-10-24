#pragma once
#include "config.h"
#include "common/string.hpp"
#include "common/exception.hpp"
#include "rapidjson/cursorstreamwrapper.h"
#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/error/en.h"
#include "rapidjson/rapidjson_helper.hpp"


namespace moon
{
    struct service_config
    {
        bool unique;
        bool shared;
        int32_t threadid;
        std::string type;
        std::string name;
        std::string config;
    };

    struct server_config
    {
        int32_t sid;
        int32_t thread;
        std::string loglevel;
        std::string name;
        std::string outer_host;
        std::string inner_host;
        std::string startup;
        std::string log;
        std::string path;
        std::vector<service_config> services;
    };

    class server_config_manger
    {
        void prepare(const std::string& config)
        {
            config_.append("[");

            rapidjson::StringStream ss(config.data());
            rapidjson::CursorStreamWrapper<rapidjson::StringStream> csw(ss);
            rapidjson::Document doc;
            doc.ParseStream<rapidjson::kParseCommentsFlag>(csw);
            MOON_CHECK(!doc.HasParseError(), moon::format("Parse server config failed:%s(%d).line %d col %d", rapidjson::GetParseError_En(doc.GetParseError()), doc.GetParseError(), csw.GetLine(), csw.GetColumn()));
            MOON_CHECK(doc.IsArray(), "Server config format error: must be json array.");
            for (auto&c : doc.GetArray())
            {
                if (config_.size() > 1)
                {
                    config_.append(",");
                }

                server_config scfg;
                scfg.sid = rapidjson::get_value<int32_t>(&c, "sid", -1);//server id
                MOON_CHECK(-1 != scfg.sid, "Server config format error:must has sid");
                scfg.outer_host = rapidjson::get_value<std::string>(&c, "outer_host", "*");
                scfg.inner_host = rapidjson::get_value<std::string>(&c, "inner_host", "127.0.0.1");
                rapidjson::StringBuffer buffer;
                rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
                c.Accept(writer);
                std::string s(buffer.GetString(), buffer.GetSize());
                moon::replace(s, "#sid", std::to_string(scfg.sid));
                moon::replace(s, "#outer_host", scfg.outer_host);
                moon::replace(s, "#inner_host", scfg.inner_host);
                config_.append(s);
            }
            config_.append("]");
        }
    public:
        const std::string config()
        {
            return config_;
        }

        bool parse(const std::string& config, int32_t sid)
        {
            try
            {
                prepare(config);
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
                    scfg.outer_host = rapidjson::get_value<std::string>(&c, "outer_host", "*");
                    scfg.inner_host = rapidjson::get_value<std::string>(&c, "inner_host", "127.0.0.1");
                    scfg.thread = rapidjson::get_value<int32_t>(&c, "thread", std::thread::hardware_concurrency());
                    scfg.startup = rapidjson::get_value<std::string>(&c, "startup");
                    scfg.log = rapidjson::get_value<std::string>(&c, "log");
                    scfg.loglevel = rapidjson::get_value<std::string>(&c, "loglevel", "DEBUG");
                    scfg.path = rapidjson::get_value<std::string>(&c, "path", "");
                    if (scfg.log.find("#date") != std::string::npos)
                    {
                        time_t now = std::time(nullptr);
                        std::tm m;
                        moon::time::localtime(&now, &m);
                        char buff[50 + 1] = { 0 };
                        std::strftime(buff, 50, "%Y%m%d%H%M%S", &m);
                        moon::replace(scfg.log, "#date", buff);
                    }

                    if (scfg.sid == sid)
                    {
                        auto services = rapidjson::get_value<rapidjson::Value*>(&c, "services", nullptr);
                        MOON_CHECK(services->IsArray(), "Server config format error: services must be array");
                        for (auto& s : services->GetArray())
                        {
                            MOON_CHECK(s.IsObject(), "Server config format error: service must be object");
                            service_config sc;
                            sc.type = rapidjson::get_value<std::string>(&s, "type", "lua");
                            sc.unique = rapidjson::get_value<bool>(&s, "unique", false);
                            sc.shared = rapidjson::get_value<bool>(&s, "shared", true);
                            sc.threadid = rapidjson::get_value<int32_t>(&s, "threadid", 0);
                            sc.name = rapidjson::get_value<std::string>(&s, "name");

                            s.AddMember("path", rapidjson::Value::StringRefType(scfg.path.data()), doc.GetAllocator());

                            rapidjson::StringBuffer buffer;
                            rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
                            s.Accept(writer);
                            sc.config = std::string(buffer.GetString(), buffer.GetSize());
                            MOON_CHECK(!sc.config.empty(), "Server config format error: service config must not be empty");
                            scfg.services.emplace_back(sc);
                        }
                    }
                    MOON_CHECK(data_.emplace(scfg.sid, scfg).second, moon::format("Server config format error:sid %d already exist.", scfg.sid));
                }
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
    private:
        std::string config_;
        std::unordered_map<int32_t, server_config> data_;
    };
}
