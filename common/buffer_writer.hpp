/****************************************************************************

Git <https://github.com/sniper00/MoonNetLua>
E-Mail <hanyongtao@live.com>
Copyright (c) 2015-2017 moon
Licensed under the MIT License <http://opensource.org/licenses/MIT>.

****************************************************************************/
#pragma once

namespace moon
{
    template<typename TBuffer>
    class buffer_writer
    {
    public:
        using buffer_type = TBuffer;

        explicit buffer_writer(buffer_type& t)
            :buffer_(t)
        {
        }

        buffer_writer(const buffer_writer& t) = delete;
        buffer_writer& operator=(const buffer_writer& t) = delete;

        template<typename T>
        bool write_front(const T& t)
        {
            static_assert(std::is_trivially_copyable<T>::value && !std::is_pointer<T>::value && !std::is_array<T>::value, "type T must be trivially copyable");
            return buffer_.write_front(&t, 0, 1);
        }

        template<typename T>
        bool write_front(const T* t, size_t count)
        {
            static_assert(std::is_trivially_copyable<T>::value && !std::is_pointer<T>::value && !std::is_array<T>::value, "type T must be trivially copyable");
            return buffer_.write_front(t, 0, count);
        }

        template<typename T>
        void write(const T& t)
        {
            write_imp(t);
        }

        template<typename T>
        void write_vector(const std::vector<T>& t)
        {
            static_assert(std::is_trivially_copyable<T>::value && !std::is_pointer<T>::value && !std::is_array<T>::value, "type T must be trivially copyable");
            write(t.size());
            for (auto& it : t)
            {
                write(it);
            }
        }

        template<typename T>
        void write_array(const T* t, size_t count)
        {
            static_assert(std::is_trivially_copyable<T>::value && !std::is_pointer<T>::value && !std::is_array<T>::value, "type T must be trivially copyable");
            buffer_.write_back(t, 0, count);
        }

        template<class T>
        buffer_writer& operator<<(T&& value)
        {
            write(value);
            return *this;
        }

        size_t size()
        {
            return buffer_.size();
        }

    private:
        template<typename T>
        void write_imp(const T & t)
        {
            static_assert(std::is_trivially_copyable<T>::value && !std::is_pointer<T>::value && !std::is_array<T>::value, "type T must be trivially copyable");
            buffer_.write_back(&t, 0, 1);
        }

        void write_imp(const bool& v)
        {
            uint8_t b = v ? 1 : 0;
            buffer_.write_back(&b, 0, 1);
        }

        void write_imp(const char* str)
        {
            buffer_.write_back(str, 0, std::strlen(str) + 1);
        }

        void write_imp(const std::string & str)
        {
            buffer_.write_back(str.data(), 0, str.size() + 1);
        }
    private:
        buffer_type& buffer_;
    };
};




