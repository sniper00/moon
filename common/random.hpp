#pragma once
#include <random>
#include <functional>
#include <map>
#include <string>
#include <cassert>

namespace moon
{
    inline std::mt19937& random_generator()
    {
        static thread_local std::random_device rd;
        static thread_local std::mt19937 gen(rd());
        return gen;
    }

    inline unsigned int rand()
    {
        std::uniform_int_distribution<unsigned int> dis(1, std::numeric_limits<unsigned int>::max());
        return dis(random_generator());
    }

    ///[min.max]
    template<typename IntType>
    inline IntType rand_range(IntType min, IntType max)
    {
        std::uniform_int_distribution<IntType> dis(min, max);
        return dis(random_generator());
    }

    inline const unsigned char* randkey(size_t len = 8)
    {
        static thread_local unsigned char tmp[256];
        assert(len < sizeof(tmp));
        char x = 0;
        for (size_t i = 0; i < len; ++i)
        {
            tmp[i] = static_cast<unsigned char>(rand() & 0xFF);
            x ^= tmp[i];
        }

        if (x == 0)
        {
            tmp[0] |= 1;// avoid 0
        }
        
        return tmp;
    }

    ///[min,max)
    template< class RealType = double >
    inline RealType randf_range(RealType min, RealType max)
    {
        std::uniform_real_distribution<RealType> dis(min, max);
        return dis(random_generator());
    }

    template< class RealType = double >
    inline bool randf_percent(RealType percent)
    {
        if (percent <= 0.0)
        {
            return false;
        }
        if (randf_range(0.0f,1.0f) >= percent)
        {
            return false;
        }
        return true;
    }

    template<typename Values, typename Weights>
    inline auto  rand_weight(const Values& v, const Weights& w)
    {
        if (v.empty() || v.size() != w.size())
        {
            return -1;
        }

        auto dist = std::discrete_distribution<int>(w.begin(), w.end());
        int index = dist(random_generator());
        return v[index];
    }
};

