#pragma once
#include <cstdint>
#include <cstdlib>
#include <type_traits>

//define system endian
#define MOON_LITTLEENDIAN
//#define MOON_BIGENDIAN

namespace moon
{
    template<typename T>
    void byte_swap(T& v)
    {
        static_assert(std::is_integral_v<T>, "need integer type");
#ifdef _MSC_VER
        if constexpr(sizeof(T) == 2)
            v = static_cast<T>(_byteswap_ushort(static_cast<uint16_t>(v)));
        else if constexpr(sizeof(T) == 4)
            v = static_cast<T>(_byteswap_ulong(static_cast<uint32_t>(v)));
        else if constexpr(sizeof(T) == 8)
            v = static_cast<T>(_byteswap_uint64(static_cast<uint64_t>(v)));
#else
        if constexpr(sizeof(T) == 2)
            v = static_cast<T>(__builtin_bswap16(static_cast<uint16_t>(v)));
        else if constexpr(sizeof(T) == 4)
            v = static_cast<T>(__builtin_bswap32(static_cast<uint32_t>(v)));
        else if constexpr(sizeof(T) == 8)
            v = static_cast<T>(__builtin_bswap64(static_cast<uint64_t>(v)));
#endif
        else
            static_assert(sizeof(T)<=8&&(sizeof(T)%2==0), "unsupported");
    }

    template<typename T>
    inline void host2net(T& val)
    {
#ifdef  MOON_LITTLEENDIAN
        byte_swap(val);
#endif //  MOON_LITTLEENDIAN	
    }

    template<typename T>
    inline void net2host(T& val)
    {
#ifdef MOON_LITTLEENDIAN
        byte_swap(val);
#endif // MOON_LITTLEENDIAN
    }
}
