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
#ifdef  MOON_LITTLEENDIAN
		detail::apply<T>(&val);
#endif //  MOON_LITTLEENDIAN	
	}

	template<typename T> 
	inline void net2host(T& val)
	{
#ifdef MOON_LITTLEENDIAN
		detail::apply<T>(&val);
#endif // MOON_LITTLEENDIAN
	}

	template<typename T> void net2host(T*);    // will generate link error
	template<typename T> void host2net(T*);  // will generate link error
	inline void net2host(uint8_t*) {}
	inline void net2host(int8_t*) {}
	inline void host2net(uint8_t*) { }
	inline void host2net(int8_t*) { }
}
