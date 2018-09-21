/****************************************************************************

Git <https://github.com/sniper00/MoonNetLua>
E-Mail <hanyongtao@live.com>
Copyright (c) 2015-2017 moon
Licensed under the MIT License <http://opensource.org/licenses/MIT>.

****************************************************************************/

#pragma once
#include <cstdint>
#include <vector>
#include <cassert>
#include <memory>
#include <type_traits>
#include <string>
#include <cstring>
#include <iostream>

namespace moon
{
    class buffer
    {
    public:
        //buffer default size
        constexpr static size_t    DEFAULT_CAPACITY = 120;

        using value_type = char;
        using const_reference = const value_type&;
        using reference = value_type & ;
        using data_type = std::vector<value_type>;
        using iterator = data_type::iterator;
        using const_iterator = data_type::const_iterator;
        using difference_type = data_type::difference_type;
        using size_type = data_type::size_type;
        using pointer = value_type * ;
        using const_pointer = const value_type*;

        enum seek_origin
        {
            Begin,
            Current,
            End
        };

        buffer(size_t capacity = DEFAULT_CAPACITY, size_t headreserved = 0)
            : flag_(0)
			, readpos_(headreserved)
			, writepos_(headreserved)
			, headreserved_(headreserved)
			, data_(capacity + headreserved)
        {

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

        template<typename T>
        void write_back(const T* Indata, size_t offset = 0, size_t count = 1)
        {
            static_assert(std::is_trivially_copyable<T>::value, "type T must be trivially copyable");
            if (nullptr == Indata || 0 == count)
                return;
            size_t n = sizeof(T)*count;

            check_space(n);
            auto* buff = reinterpret_cast<T*>(std::addressof(*end()));
            memcpy(buff, Indata + offset, n);
            writepos_ += n;
        }

        template<typename T>
        bool write_front(const T* Indata, size_t offset = 0, size_t count = 1) noexcept
        {
            static_assert(std::is_trivially_copyable<T>::value, "type T must be trivially copyable");
            if (nullptr == Indata || 0 == count || offset >= count)
                return false;

            size_t n = sizeof(T)*count;

            if (n > readpos_)
            {
                return false;
            }

            readpos_ -= n;
            auto* buff = reinterpret_cast<T*>(std::addressof(*begin()));
            memcpy(buff, Indata + offset, n);
            return true;
        }

        template<typename T>
        bool read(T* Outdata, size_t offset = 0, size_t count = 1) noexcept
        {
            static_assert(std::is_trivially_copyable<T>::value, "type T must be trivially copyable");
            if (nullptr == Outdata || 0 == count)
                return false;

            size_t n = sizeof(T)*count;

            if (readpos_ + n > writepos_)
            {
                return false;
            }

            auto* buff = std::addressof(*begin());
            memcpy(Outdata + offset, buff, n);
            readpos_ += n;
            return true;
        }

        size_t seek(int offset, seek_origin s = seek_origin::Current)  noexcept
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

        void clear() noexcept
        {
            readpos_ = writepos_ = headreserved_;
            flag_ = 0;
        }

        void set_flag(uint8_t v) noexcept
        {
            flag_ |=v;
        }

        bool has_flag(uint8_t v) const noexcept
        {
            return ((flag_&v) != 0);
        }

        void clear_flag(uint8_t v) noexcept
        {
            flag_ &= ~v;
        }

        //mark
        void offset_writepos(int offset) noexcept
        {
            writepos_ += offset;
            if (writepos_ >= data_.size())
            {
                writepos_ = data_.size();
            }
        }

        const_iterator begin() const noexcept
        {
            return data_.begin() + readpos_;
        }

        iterator begin() noexcept
        {
            return data_.begin() + readpos_;
        }

        const_iterator end() const noexcept
        {
            return data_.begin() + writepos_;
        }

        iterator end() noexcept
        {
            return data_.begin() + writepos_;
        }

        pointer data() noexcept
        {
            return  std::addressof(*begin());
        }

        const_pointer data() const noexcept
        {
            return  std::addressof(*begin());
        }

        //readable size
        size_type size() const noexcept
        {
            return writepos_ - readpos_;
        }

        size_type max_size() const noexcept
        {
            return data_.size();
        }

        void check_space(size_t len)
        {
            if (writeablesize() < len)
            {
                grow(len);
            }

            if (writeablesize() < len)
            {
                throw std::runtime_error("write_able_size() must >= len");
            }
        }

    protected:
        size_t writeablesize() const
        {
            assert(data_.size() >= writepos_);
            return data_.size() - writepos_;
        }

        void grow(size_t len)
        {
            if (writeablesize() + readpos_ < len + headreserved_)
            {
                data_.resize(writepos_ + len);
            }
            else
            {
                size_t readable = size();
                if (readable != 0)
                {
                    std::copy(begin(), end(), data_.begin() + headreserved_);
                }
                readpos_ = headreserved_;
                writepos_ = readpos_ + readable;
            }
        }

    protected:
        uint8_t								flag_;
        //read position
        size_t								readpos_;
        //write position
        size_t								writepos_;

        size_t								headreserved_;

        data_type							data_;
    };

};



