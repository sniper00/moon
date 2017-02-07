/****************************************************************************

Git <https://github.com/sniper00/MoonNetLua>
E-Mail <hanyongtao@live.com>
Copyright (c) 2015-2016 moon
Licensed under the MIT License <http://opensource.org/licenses/MIT>.

****************************************************************************/

#pragma once
#include <functional>
#include "sync_queue.hpp"

namespace moon
{
	using event_handle_t = std::function<void()>;

	class async_event
	{
	public:
		void post(event_handle_t&& f)
		{
			auto p(std::make_shared<event_handle_t>(std::forward<event_handle_t>(f)));
			events_.push_back(p);
		}

		void exit()
		{
			events_.exit();
		}

		void set_max_size(size_t size)
		{
			events_.set_max_size(size);
		}

	protected:
		void update_event()
		{
			if (events_.size() == 0)
				return;
			auto evts = events_.move();
			for (auto& e : evts)
			{
				(*e)();
			}
		}

		size_t size()
		{
			return events_.size();
		}
	private:
		sync_queue<std::shared_ptr<event_handle_t>>  events_;
	};
};