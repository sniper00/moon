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
	template<typename T>
	class BinaryReader
	{
	public:
		using TPointer = T*;

		explicit BinaryReader(TPointer t)
			:m_t(t),m_data(t->Data()), m_readpos(0), m_size(t->Size())
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

		template<typename TData>
		typename std::enable_if<
			!std::is_same<TData, bool>::value &&
			!std::is_same<TData, std::string>::value, TData>::type
			Read()
		{
			static_assert(std::is_pod<TData>::value, "type T must be pod.");
			return _read<TData>();
		}

		bool ReadBoolean()
		{
			return (_read<uint8_t>() != 0) ? true : false;
		}

		std::string Bytes()
		{
			return std::string((const char*)Data(), Size());
		}

		std::string ReadString()
		{
			std::string tmp;
			while (m_readpos < m_size)
			{
				char c = _read<char>();
				if (c == 0)
					break;
				tmp += c;
			}
			return std::move(tmp);
		}

		template<class TData>
		std::vector<TData> ReadVector()
		{
			std::vector<TData> vec;
			size_t vec_size = 0;
			(*this) >> vec_size;
			for (size_t i = 0; i < vec_size; i++)
			{
				TData tmp;
				(*this) >> tmp;
				vec.push_back(tmp);
			}
			return std::move(vec);
		}


		template<class TData>
		BinaryReader& operator >> (TData& value)
		{
			value = Read<TData>();
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
		template <typename TData>
		TData _read()
		{
			TData r = _read<TData>(m_readpos);
			m_readpos += sizeof(TData);
			return r;
		}

		template <typename TData>
		TData _read(size_t pos) const
		{
			if ((pos + sizeof(TData)) > m_size)
				throw std::runtime_error("reading out of size");
			TData val = *((TData const*)&m_data[pos]);
			return val;
		}
	protected:
		TPointer							m_t;

		const uint8_t*					m_data;
		//read position
		size_t								m_readpos;
		//write position
		size_t								m_size;
	};

};



