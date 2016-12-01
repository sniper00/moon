/****************************************************************************

Git <https://github.com/sniper00/MoonNetLua>
E-Mail <hanyongtao@live.com>
Copyright (c) 2015-2016 moon
Licensed under the MIT License <http://opensource.org/licenses/MIT>.

****************************************************************************/
#pragma once

namespace moon
{
	template<typename TBuffer>
	class buffer_writer
	{
	public:
		using buffer_type = typename TBuffer::buffer_type;

		explicit buffer_writer(buffer_type& t)
			:stream_(t)
		{
		}

		buffer_writer(const buffer_writer& t) = delete;
		buffer_writer& operator=(const buffer_writer& t) = delete;

		template<typename T>
		void write_front(T& t)
		{
			stream_.write_front(&t, 0, 1);
		}

		template<typename T>
		void write_front(T* t,size_t count)
		{
			stream_.write_front(t, 0, count);
		}

		template<typename T>
		void write(const T& t)
		{
			write_imp(t);
		}

	
		template<typename T>
		void write_vector(std::vector<T> t)
		{
			static_assert(std::is_pod<T>::value, "type T must be pod.");
			write(t.size());
			for (auto& it : t)
			{
				write(it);
			}
		}

		template<typename T>
		void write_array(T* t, size_t count)
		{
			static_assert(std::is_pod<T>::value, "type T must be pod.");
			stream_.write_back(t, 0, count);
		}

		template<class T>
		buffer_writer& operator<<(T&& value)
		{
			write(value);
			return *this;
		}

		size_t Size()
		{
			return stream_.Size();
		}

	private:
		template<typename T>
		void write_imp(const T & t)
		{
			static_assert(std::is_pod<T>::value, "type T must be pod.");
			stream_.write_back(&t, 0, 1);
		}

		void write_imp(const bool& v)
		{
			uint8_t b = v ? 1 : 0;
			stream_.write_back(&b, 0, 1);
		}

		void write_imp(const char* str)
		{
			stream_.write_back(str, 0,std::strlen(str) + 1);
		}

		void write_imp(const std::string & str)
		{
			stream_.write_back(str.data(), 0, str.size() + 1);
		}
	private:
		buffer_type& stream_;
	};
};




