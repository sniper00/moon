/****************************************************************************

Git <https://github.com/sniper00/MoonNetLua>
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
#include <iostream>
namespace moon
{
	class buffer
	{
	public:
		using buffer_type = buffer;
		///buffer default size
		constexpr static size_t    DEFAULT_CAPACITY = 64;

		enum seek_origin
		{
			Begin,
			Current,
			End
		};

		buffer(size_t capacity = DEFAULT_CAPACITY, size_t headreserved = 0)
			:data_(capacity + headreserved), readpos_(headreserved), writepos_(headreserved), headreserved_(headreserved),flag_(0)
		{
			assert(size() == 0);
			assert(writeablesize() == capacity);
			//assert(readpos_ != 0);
		}

		buffer(const buffer& other) = default;
		buffer(buffer&& other) = default;

		void init(size_t capacity = DEFAULT_CAPACITY, size_t headreserved = 0)
		{
			data_.resize(capacity + headreserved);
			readpos_ = headreserved;
			writepos_ = headreserved;
			headreserved_ = headreserved;
			flag_ = 0;
		}

		template<typename T, typename _T = typename std::remove_cv<T>::type>
		void write_back(T* Indata, size_t offset, size_t count)
		{
			static_assert(!std::is_pointer<_T>::value, "type T must be not pointer");
			if (nullptr == Indata || 0 == count)
				return;
			checkwriteablesize(sizeof(_T)*count);
			auto* buff = (_T*)&(*writeable());

			for (size_t i = 0; i < count; i++)
			{
				buff[i] = Indata[offset + i];
			}

			writepos_ += sizeof(_T)*count;
		}

		template<typename T, typename _T = typename std::remove_cv<T>::type>
		void write_front(T* Indata, size_t offset, size_t count)
		{
			static_assert(!std::is_pointer<_T>::value, "type T must be not pointer");
			if (nullptr == Indata || 0 == count)
				return;

			if (sizeof(_T)*count > readpos_)
			{
				count = readpos_ / sizeof(_T);
				std::cerr << "buffer:write_front:write data out of size \r\n";
				throw std::runtime_error("write_front:write data out of size");
			}

			readpos_ -= sizeof(_T)*count;

			auto* buff = (_T*)&data_[readpos_];
			for (size_t i = 0; i < count; i++)
			{
				buff[i] = Indata[offset + i];
			}
		}

		template<typename T>
		void read(T* Outdata, size_t offset, size_t count)
		{
			static_assert(!std::is_pointer<T>::value, "type T must be not pointer.");
			static_assert(!std::is_const<T>::value, "Outdata must be not const.");
			if (nullptr == Outdata || 0 == count)
				return;

			if (readpos_ + sizeof(T)*count > writepos_)
			{
				count = (writepos_ - readpos_) / sizeof(T);
				std::cerr << "buffer:read data out of size \r\n";
				throw std::runtime_error("buffer:read data out of size");
			}

			auto* buff = (T*)&data_[readpos_];

			for (size_t i = 0; i < count; i++)
			{
				Outdata[i + offset] = buff[i];
			}
			readpos_ += sizeof(T)*count;
		}

		const uint8_t* data() const
		{
			return data_.data() + readpos_;
		}

		//readable size
		uint32_t size() const
		{
			return uint32_t(writepos_ - readpos_);
		}

		size_t seek(int offset, seek_origin s)
		{
			switch (s)
			{
			case Begin:
				readpos_ = offset;
				break;
			case Current:
				readpos_ += offset;
				if (readpos_ > writepos_)
				{
					readpos_ = writepos_;
				}
				break;
			case End:
				readpos_ = writepos_ + offset;
				if (readpos_ >= writepos_)
				{
					readpos_ = writepos_;
				}
				break;
			default:
				break;
			}

			return readpos_;
		}

		void clear()
		{
			readpos_ = writepos_ = headreserved_;
			flag_ = 0;
		}

		buffer& stream()
		{
			return *this;
		}

		void set_flag(uint8_t v)
		{
			flag_ |= v;
		}

		bool check_flag(uint8_t v)
		{
			return ((flag_&v) != 0);
		}

		void offset_writepos(size_t offset)
		{
			writepos_ += offset;
		}

	protected:
		std::vector<uint8_t>::iterator writeable()
		{
			return begin() + writepos_;
		}

		size_t writeablesize()
		{
			return data_.size() - writepos_;
		}

		void checkwriteablesize(size_t len)
		{
			if (writeablesize() < len)
			{
				makespace(len);
			}

			if (writeablesize() <len)
			{
				throw std::runtime_error("write_able_size() must >= len");
			}
		}

		void makespace(size_t len)
		{
			if (writeablesize() + readpos_ < len+headreserved_)
			{
				size_t s = DEFAULT_CAPACITY;
				while (s < writepos_ + len)
				{
					s *= 2;
				}
				data_.resize(s);
			}
			else
			{
				size_t readable = size();
				std::copy(begin() + readpos_, begin() + writepos_,begin()+ headreserved_);
				readpos_ = headreserved_;
				writepos_ = readpos_ + readable;
			}
		}

		std::vector<uint8_t>::iterator begin()
		{
			return data_.begin();
		}

	protected:
		std::vector<uint8_t>		data_;
		//read position
		size_t								readpos_;
		//write position
		size_t								writepos_;

		size_t								headreserved_;

		uint8_t								flag_;
	};

};



