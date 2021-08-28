#pragma once
#include <cstdint>

namespace moon
{
    //define system endian
#define MOON_LITTLEENDIAN
//#define MOON_BIGENDIAN

    namespace detail
    {
        template<size_t T>
        inline void convert(char* val)
        {
            std::swap(*val, *(val + T - 1));
            convert < T - 2 >(val + 1);
        }

        template<> inline void convert<0>(char*) {}
        template<> inline void convert<1>(char*) {}             // ignore central byte

        template<typename T>
        inline void apply(T* val)
        {
            convert<sizeof(T)>(reinterpret_cast<char*>(val));
        }
    }

    template<typename T>
    inline void host2net(T& val)
    {
        static_assert(!std::is_pointer_v<T>, "need value type");
#ifdef  MOON_LITTLEENDIAN
        detail::apply<T>(&val);
#endif //  MOON_LITTLEENDIAN	
    }

    template<typename T>
    inline void net2host(T& val)
    {
        static_assert(!std::is_pointer_v<T>, "need value type");
#ifdef MOON_LITTLEENDIAN
        detail::apply<T>(&val);
#endif // MOON_LITTLEENDIAN
    }
}
