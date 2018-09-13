/****************************************************************************

Git <https://github.com/sniper00/MoonNetLua>
E-Mail <hanyongtao@live.com>
Copyright (c) 2015-2017 moon
Licensed under the MIT License <http://opensource.org/licenses/MIT>.

****************************************************************************/
#pragma once
#include "config.h"
#include "router.h"
#include "common/log.hpp"

namespace moon
{
    class  server final
    {
    public:
        server();

        ~server();

        server(const server&) = delete;

        server& operator=(const server&) = delete;

        server(server&&) = delete;

        void init(uint8_t worker_num, const std::string& logpath);

        void run();

        void stop();

		log* logger();

		router* get_router();

		size_t workernum() const;
	private:
		void wait();
    private:
		std::atomic<state> state_;
		std::vector<std::shared_ptr<worker>> workers_;
		log default_log_;
		router router_;
    };
};


