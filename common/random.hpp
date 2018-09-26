/****************************************************************************

Git <https://github.com/sniper00/MoonNetLua>
E-Mail <hanyongtao@live.com>
Copyright (c) 2015-2017 moon
Licensed under the MIT License <http://opensource.org/licenses/MIT>.

****************************************************************************/

#pragma once
#include <random>
#include <functional>
#include <ctime>
#include <map>

namespace moon
{
    ///[min.max]
    inline int rand(int min, int max)
    {
        std::default_random_engine generator(time(NULL));
        std::uniform_int_distribution<int> dis(min, max);
        auto dice = std::bind(dis, generator);
        return dice();
    }

    ///[min,max)
    inline double randf(double min, double max)
    {
        std::default_random_engine generator(time(NULL));
        std::uniform_real_distribution<double> dis(min, max);
        auto dice = std::bind(dis, generator);
        return dice();
    }

    template<typename TKey, typename TWeight>
    inline TKey rand_weight(const std::map<TKey, TWeight>& data)
    {
        TWeight total = 0;
        for (auto& it : data)
        {
            total += it.second;
        }
        if (total == 0)
        {
            return 0;
        }
        TWeight rd = rand(1, total);

        TWeight tmp = 0;
        for (auto& it : data)
        {
            tmp += it.second;
            if (rd <= tmp)
            {
                return it.first;
            }
        }
        return 0;
    }
};

