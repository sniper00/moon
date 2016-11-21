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
	class buffer_reader
	{
	public:
		buffer_reader(const uint8_t* data, size_t size)
			:data_(data)
			,readpos_(0)
			,size_(size)
		{
		}

		buffer_reader(const buffer_reader&) = delete;
		buffer_reader& operator=(const buffer_reader&) = delete;

		~buffer_reader(void)
		{
		}

		template<typename T>
		typename std::enable_if<
			!std::is_same<T, bool>::value &&
			!std::is_same<T, std::string>::value, T>::type
			read()
		{
			static_assert(std::is_pod<T>::value, "type T must be pod.");
			return _read<T>();
		}

		template<typename T>
		typename std::enable_if<
			std::is_same<T, bool>::value, T>::type read()
		{
			return (_read<uint8_t>() != 0) ? true : false;
		}

		template<typename T>
		typename std::enable_if<
			std::is_same<T, std::string>::value, T>::type read()
		{
			std::string tmp;
			while (readpos_ < size_)
			{
				char c = _read<char>();
				if (c == 0)
					break;
				tmp += c;
			}
			return std::move(tmp);
		}

		std::string bytes()
		{
			return std::string((const char*)data(), size());
		}

		template<class T>
		std::vector<T> read_vector()
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
		buffer_reader& operator >> (T& value)
		{
			value = read<T>();
			return *this;
		}

		const uint8_t* data() const
		{
			return data_ + readpos_;
		}

		//readable size
		uint32_t size() const
		{
			return uint32_t(size_ - readpos_);
		}

		void skip(size_t len)
		{
			if (len < size())
			{
				readpos_ += len;
			}
			else
			{
				readpos_ = size_ = 0;
			}
		}

	private:
		template <typename T>
		T _read()
		{
			if ((readpos_ + sizeof(T)) > size_)
				throw std::runtime_error("reading out of size");
			T val = *((T const*)&data_[readpos_]);
			readpos_ += sizeof(T);
			return val;
		}
	protected:
		const uint8_t*					data_;
		//read position
		size_t								readpos_;
		//size
		size_t								size_;
	};
};



