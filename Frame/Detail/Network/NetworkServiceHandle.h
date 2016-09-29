/****************************************************************************

Git <https://github.com/sniper00/MoonNetLua>
E-Mail <hanyongtao@live.com>
Copyright (c) 2015-2016 moon
Licensed under the MIT License <http://opensource.org/licenses/MIT>.

****************************************************************************/

#pragma once
#include <unordered_map>
#include "Common/Handle.hpp"

namespace moon
{
	struct NetworkServiceHandle:public THandle<uint32_t, NetworkServiceHandle>
	{	
	};
};

namespace std
{
	template<>
	struct hash<moon::NetworkServiceHandle>
	{
		std::size_t operator()(const moon::NetworkServiceHandle& k) const
		{
			return k.value;
		}
	};
};

