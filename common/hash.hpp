#pragma once
#include <cassert>
#include <chrono>
#include <cstdint>
#include <ctime>
#include <functional>
#include <string>
#include <string_view>

#if defined(_MSC_VER)
    #define HASH_ROTL32(x, r) _rotl(x, r)
#else
    #define HASH_ROTL32(x, r) (x << r) | (x >> (32 - r))
#endif

namespace moon {
namespace hash_detail {
    template<typename SizeT>
    inline void hash_combine_impl(SizeT& seed, SizeT value) {
        seed ^= value + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    }

    inline void hash_combine_impl(std::uint32_t& h1, std::uint32_t k1) {
        const uint32_t c1 = 0xcc9e2d51;
        const uint32_t c2 = 0x1b873593;

        k1 *= c1;
        k1 = HASH_ROTL32(k1, 15);
        k1 *= c2;

        h1 ^= k1;
        h1 = HASH_ROTL32(h1, 13);
        h1 = h1 * 5 + 0xe6546b64;
    }

#if defined(_WIN64) || defined(__x86_64__) || defined(__amd64)
    inline void hash_combine_impl(std::uint64_t& h, std::uint64_t k) {
        static_assert(sizeof(void*) == 8, "only support 64bit system");

        const uint64_t m = UINT64_C(0xc6a4a7935bd1e995);
        const int r = 47;

        k *= m;
        k ^= k >> r;
        k *= m;

        h ^= k;
        h *= m;

        // Completely arbitrary number, to prevent 0's
        // from hashing to 0.
        h += 0xe6546b64;
    }
#endif
} // namespace hash_detail

template<class T>
inline std::size_t hash_value_signed(T val) {
    const int size_t_bits = std::numeric_limits<std::size_t>::digits;
    // ceiling(std::numeric_limits<T>::digits / size_t_bits) - 1
    const int length = (std::numeric_limits<T>::digits - 1) / size_t_bits;

    std::size_t seed = 0;
    T positive = val < 0 ? -1 - val : val;

    // Hopefully, this loop can be unrolled.
    for (unsigned int i = length * size_t_bits; i > 0; i -= size_t_bits) {
        seed ^= (std::size_t)(positive >> i) + (seed << 6) + (seed >> 2);
    }
    seed ^= (std::size_t)val + (seed << 6) + (seed >> 2);

    return seed;
}

template<class T>
inline std::size_t hash_value_unsigned(T val) {
    const int size_t_bits = std::numeric_limits<std::size_t>::digits;
    // ceiling(std::numeric_limits<T>::digits / size_t_bits) - 1
    const int length = (std::numeric_limits<T>::digits - 1) / size_t_bits;

    std::size_t seed = 0;

    // Hopefully, this loop can be unrolled.
    for (unsigned int i = length * size_t_bits; i > 0; i -= size_t_bits) {
        seed ^= (std::size_t)(val >> i) + (seed << 6) + (seed >> 2);
    }
    seed ^= (std::size_t)val + (seed << 6) + (seed >> 2);

    return seed;
}

template<class T>
inline void hash_combine(std::size_t& seed, T const& v) {
    std::hash<T> hasher;
    return moon::hash_detail::hash_combine_impl(seed, hasher(v));
}

template<class It>
inline std::size_t hash_range(It first, It last) {
    std::size_t seed = 0;

    for (; first != last; ++first) {
        hash_combine(seed, *first);
    }

    return seed;
}

template<class It>
inline void hash_range(std::size_t& seed, It first, It last) {
    for (; first != last; ++first) {
        hash_combine(seed, *first);
    }
}

inline constexpr uint64_t chash_string(const char* str, size_t len, uint64_t seed = 0) {
    return 0 == len
        ? seed
        : chash_string(
              str + 1,
              len - 1,
              seed ^ (static_cast<uint64_t>(*str) + 0x9e3779b9 + (seed << 6) + (seed >> 2))
          );
}

inline constexpr uint64_t chash_string(const std::string_view& s, uint64_t seed = 0) {
    return chash_string(s.data(), s.size(), seed);
}

inline uint64_t chash_string(const std::string& s, uint64_t seed = 0) {
    return chash_string(s.data(), s.size(), seed);
}

inline constexpr uint64_t operator""_csh(const char* string, size_t len) {
    return chash_string(string, len);
}
} // namespace moon
