/****************************************************************************

Git <https://github.com/sniper00/moon_net>
E-Mail <hanyongtao@live.com>
Copyright (c) 2015-2016 moon
Licensed under the MIT License <http://opensource.org/licenses/MIT>.

****************************************************************************/

#pragma once
#include <cstdint>
#include <vector>
#include <cassert>
#include <memory>
#include <type_traits>
#include <string>

namespace moon
{
	class MemoryStream;
	using MemoryStreamPtr = std::shared_ptr<MemoryStream>;

	class MemoryStream
	{
	public:
		using StreamType = MemoryStream;
		///buffer default size
		constexpr static size_t    DEFAULT_CAPACITY = 64;

		enum seek_origin
		{
			Begin,
			Current,
			End
		};

		MemoryStream(size_t capacity = DEFAULT_CAPACITY, size_t headreserved = 0)
			:m_data(capacity + headreserved), m_readpos(headreserved), m_writepos(headreserved)
		{
			assert(Size() == 0);
			assert(WriteableSize() == capacity);
		}

		MemoryStream(const MemoryStream& other) = default;
	

		MemoryStream(MemoryStream&& other) = default;


		~MemoryStream(void)
		{
			
		}

		void Init(size_t capacity = DEFAULT_CAPACITY, size_t headreserved = 0)
		{
			m_data.resize(capacity + headreserved);

			m_readpos = headreserved;
			m_writepos = headreserved;
		}

		template<typename T, typename _T = typename std::remove_cv<T>::type>
		void WriteBack(T* Indata, size_t offset, size_t count)
		{
			static_assert(!std::is_pointer<_T>::value, "type T must be not pointer");
			if (nullptr == Indata || 0 == count)
				return;
			CheckWriteableSize(sizeof(_T)*count);
			auto* buff = (_T*)&(*Writeable());

			for (size_t i = 0; i < count; i++)
			{
				buff[i] = Indata[offset + i];
			}

			m_writepos += sizeof(_T)*count;
		}

		template<typename T, typename _T = typename std::remove_cv<T>::type>
		void WriteFront(T* Indata, size_t offset, size_t count)
		{
			static_assert(!std::is_pointer<_T>::value, "type T must be not pointer");
			if (nullptr == Indata || 0 == count)
				return;

			if (sizeof(_T)*count > m_readpos)
			{
				count = m_readpos / sizeof(_T);
				throw std::runtime_error("write_front:write data out of size\r\n");
			}

			m_readpos -= sizeof(_T)*count;

			auto* buff = (_T*)&m_data[m_readpos];
			for (size_t i = 0; i < count; i++)
			{
				buff[i] = Indata[offset + i];
			}
		}

		template<typename T>
		void Read(T* Outdata, size_t offset, size_t count)
		{
			static_assert(!std::is_pointer<T>::value, "type T must be not pointer.");
			static_assert(!std::is_const<T>::value, "Outdata must be not const.");
			if (nullptr == Outdata || 0 == count)
				return;

			if (m_readpos + sizeof(T)*count > m_writepos)
			{
				count = (m_writepos - m_readpos) / sizeof(T);
				throw std::runtime_error("mempry_stream:read data out of size\r\n");
			}

			auto* buff = (T*)&m_data[m_readpos];

			for (size_t i = 0; i < count; i++)
			{
				Outdata[i + offset] = buff[i];
			}
			m_readpos += sizeof(T)*count;
		}

		const uint8_t* Data() const
		{
			return m_data.data() + m_readpos;
		}

		//readable size
		uint32_t Size() const
		{
			return uint32_t(m_writepos - m_readpos);
		}

		size_t Seek(int offset, seek_origin s)
		{
			switch (s)
			{
			case Begin:
				m_readpos = offset;
				break;
			case Current:
				m_readpos += offset;
				if (m_readpos >= m_writepos)
				{
					m_readpos = m_writepos;
				}
				break;
			case End:
				m_readpos = m_writepos + offset;
				if (m_readpos >= m_writepos)
				{
					m_readpos = m_writepos;
				}
				break;
			default:
				break;
			}

			return m_readpos;
		}

		void Clear()
		{
			m_readpos = m_writepos;
		}

		MemoryStream& ToStream()
		{
			return *this;
		}

	protected:
		std::vector<uint8_t>::iterator Writeable()
		{
			return begin() + m_writepos;
		}

		size_t WriteableSize()
		{
			return m_data.size() - m_writepos;
		}

		void CheckWriteableSize(size_t len)
		{
			if (WriteableSize() < len)
			{
				MakeSpace(len);
			}

			if (WriteableSize() < len)
			{
				throw std::runtime_error("write_able_size() must >= len");
			}
		}

		void MakeSpace(size_t len)
		{
			if (WriteableSize() + m_readpos < len)
			{
				size_t s = DEFAULT_CAPACITY;
				while (s < m_writepos + len)
				{
					s *= 2;
				}
				m_data.resize(s);
			}
			else
			{
				size_t readable = Size();
				std::copy(begin() + m_readpos, begin() + m_writepos, begin());
				m_readpos = 0;
				m_writepos = m_readpos + readable;
			}
		}

		std::vector<uint8_t>::iterator begin()
		{
			return m_data.begin();
		}

	protected:
		std::vector<uint8_t>		m_data;
		//read position
		size_t								m_readpos;
		//write position
		size_t								m_writepos;
	};

};



