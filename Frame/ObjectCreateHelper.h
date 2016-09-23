/****************************************************************************

Git <https://github.com/sniper00/moon_net>
E-Mail <hanyongtao@live.com>
Copyright (c) 2015-2016 moon
Licensed under the MIT License <http://opensource.org/licenses/MIT>.

****************************************************************************/

#pragma once
#include <memory>

template<typename TObject>
class ObjectCreateHelper
{
public:
	using TObjectPtr = std::shared_ptr<TObject>;

	template<typename... Args>
	static TObjectPtr Create(Args&&... args)
	{
		return std::make_shared<TObject>(std::forward<Args>(args)...);
	}
};

