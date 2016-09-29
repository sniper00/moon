/****************************************************************************

Git <https://github.com/sniper00/MoonNetLua>
E-Mail <hanyongtao@live.com>
Copyright (c) 2015-2016 moon
Licensed under the MIT License <http://opensource.org/licenses/MIT>.

****************************************************************************/

#pragma once
#include <cstdint>
#include <cassert>
#include <memory>
#include <type_traits>
#include <string>

namespace moon
{
	template<typename TStream>
	class BinaryReader
	{
	public:
		using TStreamPointer = TStream*;

		explicit BinaryReader(TStreamPointer t)
			:m_data(t->Data()), m_readpos(0), m_size(t->Size())
		{
		}

		BinaryReader(const uint8_t* data, size_t size)
			:m_data(data), m_readpos(0), m_size(size)
		{
		}

		BinaryReader(const BinaryReader&) = delete;
		BinaryReader& operator=(const BinaryReader&) = delete;

		~BinaryReader(void)
		{
		}

		template<typename T>
		typename std::enable_if<
			!std::is_same<T, bool>::value &&
			!std::is_same<T, std::string>::value, T>::type
			Read()
		{
			static_assert(std::is_pod<T>::value, "type T must be pod.");
			return _Read<T>();
		}

		template<typename T>
		typename std::enable_if<
			std::is_same<T, bool>::value,T>::type Read()
		{
			return (_Read<uint8_t>() != 0) ? true : false;
		}

		template<typename T>
		typename std::enable_if<
			std::is_same<T, std::string>::value, T>::type Read()
		{
			std::string tmp;
			while (m_readpos < m_size)
			{
				char c = _Read<char>();
				if (c == 0)
					break;
				tmp += c;
			}
			return std::move(tmp);
		}

		std::string Bytes()
		{
			return std::string((const char*)Data(), Size());
		}

		template<class T>
		std::vector<T> ReadVector()
		{
			std::vector<T> vec;
			size_t vec_size = 0;
			(*this) >> vec_size;
			for (size_t i = 0; i < vec_size; i++)
			{
				T tmp;
				(*this) >> tmp;
				vec.push_back(tmp);
			}
			return std::move(vec);
		}


		template<class T>
		BinaryReader& operator >> (T& value)
		{
			value = Read<T>();
			return *this;
		}

		const uint8_t* Data() const
		{
			return m_data + m_readpos;
		}

		//readable size
		uint32_t Size() const
		{
			return uint32_t(m_size - m_readpos);
		}

		void Skip(size_t len)
		{
			if (len < Size())
			{
				m_readpos += len;
			}
			else
			{
				m_readpos = m_size = 0;
			}
		}

	private:
		template <typename T>
		T _Read()
		{
			if ((m_readpos + sizeof(T)) > m_size)
				throw std::runtime_error("reading out of size");
			T val = *((T const*)&m_data[m_readpos]);
			m_readpos += sizeof(T);
			return val;
		}
	protected:
		const uint8_t*					m_data;
		//read position
		size_t								m_readpos;
		//size
		size_t								m_size;
	};

};



