/****************************************************************************

Git <https://github.com/sniper00/MoonNetLua>
E-Mail <hanyongtao@live.com>
Copyright (c) 2015-2016 moon
Licensed under the MIT License <http://opensource.org/licenses/MIT>.

****************************************************************************/
#pragma once
#include "MacroDefine.h"
#include "Common/string_utils.hpp"

namespace moon
{
	namespace detail
	{
		struct actor_manager_config
		{
			int			worker_num;
			uint8_t		machine_id;
		};

		template<typename T>
		struct parse_config;

		template<>
		struct parse_config<actor_manager_config>
		{
			using config_type = actor_manager_config;

			config_type parse(const std::string& config)
			{
				config_type tmp;

				auto kv_config = string_utils::parse_key_value_string(config);

				CHECKWARNING(contains_key(kv_config, "worker_num"), "config does not have [worker_num],will initialize worker_num = 1");
				CHECKWARNING(contains_key(kv_config, "machine_id"), "config does not have [machine_id],will initialize machine_id = 1");

				std::string worker_num;
				std::string machine_id;

				if (contains_key(kv_config, "worker_num"))
				{
					tmp.worker_num = string_utils::string_convert<int>(kv_config["worker_num"]);
				}
				else
				{
					tmp.worker_num = 1;
				}

				if (contains_key(kv_config, "machine_id"))
				{
					tmp.machine_id = string_utils::string_convert<uint8_t>(kv_config["machine_id"]);
				}
				else
				{
					tmp.machine_id = 0;
				}
				return tmp;
			}
		};
	}
}
