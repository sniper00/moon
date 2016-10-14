/****************************************************************************

Git <https://github.com/sniper00/MoonNetLua>
E-Mail <hanyongtao@live.com>
Copyright (c) 2015-2016 moon
Licensed under the MIT License <http://opensource.org/licenses/MIT>.

****************************************************************************/

#pragma once
#include <functional>
#include "SyncQueue.hpp"

namespace moon
{
	using event_handle = std::function<void()>;

	class AsyncEvent
	{
	public:
		void Post(event_handle&& evt)
		{
			_events.EmplaceBack(std::forward<event_handle>(evt));
		}

		void exit()
		{
			_events.Exit();
		}
	protected:
		void UpdateEvents()
		{
			if (_events.Size() == 0)
				return;
			auto evts = _events.Move();
			for (auto& e : evts)
			{
				e();
			}
		}

		size_t GetEventsSize()
		{
			return _events.Size();
		}
	private:
		SyncQueue<event_handle,1000> _events;
	};
};