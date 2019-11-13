#pragma once
#include <cstdint>
#include <cassert>
#include <memory>
#include <type_traits>
#include <string>
#include <string_view>

namespace moon
{
    class buffer_view
    {
    public:
        buffer_view(const char* data, size_t size)
            :data_(data)
            , readpos_(0)
            , size_(size)
        {
        }

        buffer_view(const buffer_view&) = delete;
        buffer_view& operator=(const buffer_view&) = delete;

        ~buffer_view(void)
        {
        }

        template<typename T>
        bool read(T* Outdata, size_t count = 1) noexcept
        {
            static_assert(std::is_trivially_copyable<T>::value, "type T must be trivially copyable");
            if (nullptr == Outdata || 0 == count)
                return false;

            size_t n = sizeof(T)*count;

            if (readpos_ + n > size_)
            {
                return false;
            }

            auto* buff = data();
            memcpy(Outdata, buff, n);
            readpos_ += n;
            return true;
        }

        template<typename T>
        typename std::enable_if<
            !std::is_same<T, bool>::value &&
            !std::is_same<T, std::string>::value, T>::type
            read()
        {
            static_assert(std::is_trivially_copyable<T>::value, "type T must be trivially copyable.");
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
                if (c == '\0')
                    break;
                tmp += c;
            }
            return tmp;
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
            return vec;
        }

        template<class T>
        buffer_view& operator >> (T& value)
        {
            value = read<T>();
            return *this;
        }

        const char* data() const
        {
            return data_ + readpos_;
        }

        //readable size
        size_t size() const
        {
            return (size_ - readpos_);
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

        std::string_view readline()
        {
            std::string_view strref(data_ + readpos_, size());
            size_t pos = strref.find("\r\n");
            if (pos != std::string_view::npos)
            {
                readpos_ += (pos + 2);
                return std::string_view(strref.data(), pos);
            }
            pos = size();
            readpos_ += pos;
            return std::string_view(strref.data(), pos);
        }

        std::string_view read_delim(char c)
        {
            std::string_view strref(data_ + readpos_, size());
            size_t pos = strref.find(c);
            if (pos != std::string_view::npos)
            {
                readpos_ += (pos + sizeof(c));
                return std::string_view(strref.data(), pos);
            }
            return std::string_view();
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
        const char*	data_;
        //read position
        size_t	readpos_;
        //size
        size_t   size_;
    };
};



