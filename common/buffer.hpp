#pragma once
#include <cstdint>
#include <vector>
#include <cassert>
#include <memory>
#include <type_traits>
#include <string>
#include <cstring>
#include <iostream>
#include <charconv>
#include <utility>

namespace moon
{
    template<typename ValueType>
    class buffer_iterator
    {
    public:
        using iterator_category = std::random_access_iterator_tag;
        using self_type = buffer_iterator;
        using value_type = ValueType;
        using pointer = value_type*;
        using reference = value_type&;
        using difference_type = std::ptrdiff_t;

        explicit buffer_iterator(pointer p)
            :_Ptr(p)
        {
        }

        reference operator*() const
        {
            return  *_Ptr;
        }

        buffer_iterator& operator++()
        {
            ++_Ptr;
            return *this;
        }

        buffer_iterator operator++(int)
        {	// postincrement
            buffer_iterator _Tmp = *this;
            ++* this;
            return (_Tmp);
        }

        buffer_iterator& operator--()
        {
            --_Ptr;
            return *this;
        }

        buffer_iterator operator--(int)
        {	// postdecrement
            buffer_iterator _Tmp = *this;
            --* this;
            return (_Tmp);
        }

        buffer_iterator& operator+=(const difference_type _Off)
        {	// increment by integer
            _Ptr += _Off;
            return (*this);
        }

        buffer_iterator operator+(const difference_type _Off) const
        {	// return this + integer
            buffer_iterator _Tmp = *this;
            return (_Tmp += _Off);
        }

        buffer_iterator& operator-=(const difference_type _Off)
        {	// decrement by integer
            return (*this += -_Off);
        }

        buffer_iterator operator-(const difference_type _Off) const
        {	// return this - integer
            buffer_iterator _Tmp = *this;
            return (_Tmp -= _Off);
        }

        reference operator[](const difference_type _Off) const
        {	// subscript
            return (*(*this + _Off));
        }

        difference_type operator-(const buffer_iterator& _Right) const
        {	// return difference of iterators
            return (_Ptr - _Right._Ptr);
        }

        bool operator!=(const buffer_iterator& other) const
        {
            return _Ptr != other._Ptr;
        }

        bool operator==(const buffer_iterator& _Right) const
        {	// test for iterator equality
            return (_Ptr == _Right._Ptr);
        }

        bool operator<(const buffer_iterator& _Right) const
        {	// test if this < _Right
            return (_Ptr < _Right._Ptr);
        }

        bool operator>(const buffer_iterator& _Right) const
        {	// test if this > _Right
            return (_Right < *this);
        }

        bool operator<=(const buffer_iterator& _Right) const
        {	// test if this <= _Right
            return (!(_Right < *this));
        }

        bool operator>=(const buffer_iterator& _Right) const
        {	// test if this >= _Right
            return (!(*this < _Right));
        }
    private:
        pointer _Ptr;
    };

    template<class Alloc>
    class base_buffer
    {
    public:
        using allocator_type = Alloc;
        using value_type = typename allocator_type::value_type;
        using iterator = buffer_iterator<value_type>;
        using const_iterator = buffer_iterator<const value_type>;
        using pointer = typename iterator::pointer;
        using const_pointer = typename const_iterator::pointer;

        //buffer default size
        constexpr static size_t   DEFAULT_CAPACITY = 128;

        enum class seek_origin
        {
            Begin,
            Current,
        };

        base_buffer(size_t capacity = DEFAULT_CAPACITY, uint32_t headreserved = 0, const allocator_type& al = allocator_type())
            : allocator_(al)
            , flag_(0)
            , headreserved_(headreserved)
            , capacity_(0)
            , readpos_(0)
            , writepos_(0)
        {
            capacity = (capacity > 0 ? capacity : DEFAULT_CAPACITY);
            prepare(capacity + headreserved);
            readpos_ = writepos_ = headreserved_;
        }

        base_buffer(const base_buffer&) = delete;

        base_buffer& operator=(const base_buffer&) = delete;

        base_buffer(base_buffer&& other) noexcept
            : allocator_(std::move(other.allocator_))
            , flag_(std::exchange(other.flag_, 0))
            , headreserved_(std::exchange(other.headreserved_, 0))
            , capacity_(std::exchange(other.capacity_, 0))
            , readpos_(std::exchange(other.readpos_, 0))
            , writepos_(std::exchange(other.writepos_, 0))
            , data_(std::exchange(other.data_, nullptr))
        {
        }

        base_buffer& operator=(base_buffer&& other) noexcept
        {
            if (this != std::addressof(*other))
            {
                allocator_.deallocate(data_, capacity_);
                flag_ = std::exchange(other.flag_, 0);
                headreserved_ = std::exchange(other.headreserved_, 0);
                capacity_ = std::exchange(other.capacity_, 0);
                readpos_ = std::exchange(other.readpos_, 0);
                writepos_ = std::exchange(other.writepos_, 0);
                data_ = std::exchange(other.data_, nullptr);
                allocator_ = std::move(other.allocator_);
            }
            return *this;
        }

        ~base_buffer()
        {
            allocator_.deallocate(data_, capacity_);
        }

        template<typename T>
        void write_back(const T* Indata, size_t count)
        {
            static_assert(std::is_trivially_copyable<T>::value, "type T must be trivially copyable");
            if (nullptr == Indata || 0 == count)
                return;
            size_t n = sizeof(T) * count;
            auto* buff = reinterpret_cast<T*>(prepare(n));
            memcpy(buff, Indata, n);
            writepos_ += n;
        }

        void write_back(char c)
        {
            *prepare(1) = c;
            ++writepos_;
        }

        void unsafe_write_back(char c)
        {
            *(data_ + (writepos_++)) = c;
        }

        template<typename T>
        bool write_front(const T* Indata, size_t count) noexcept
        {
            static_assert(std::is_trivially_copyable<T>::value, "type T must be trivially copyable");
            if (nullptr == Indata || 0 == count)
                return false;

            size_t n = sizeof(T) * count;

            if (n > readpos_)
            {
                return false;
            }

            readpos_ -= n;
            auto* buff = reinterpret_cast<T*>(std::addressof(*begin()));
            memcpy(buff, Indata, n);
            return true;
        }

        template<typename T, typename =  std::enable_if_t<std::is_arithmetic_v<T>>>
        bool write_chars(T value)
        {
            static constexpr size_t MAX_NUMBER_2_STR = 44;
            auto* b = prepare(MAX_NUMBER_2_STR);
#ifndef _MSC_VER//std::to_chars in C++17: gcc and clang only integral types supported
            if constexpr (std::is_floating_point_v<T>)
            {
                int len = std::snprintf(b, MAX_NUMBER_2_STR, "%.16g", value);
                if (len < 0)
                    return false;
                commit(len);
                return true;
            }
            else
#endif
            {
                auto* e = b + MAX_NUMBER_2_STR;
                auto res = std::to_chars(b, e, value);
                if (res.ec != std::errc())
                    return false;
                commit(res.ptr - b);
                return true;
            }
        }

        template<typename T>
        bool read(T* Outdata, size_t count) noexcept
        {
            static_assert(std::is_trivially_copyable<T>::value, "type T must be trivially copyable");
            if (nullptr == Outdata || 0 == count)
                return false;

            size_t n = sizeof(T) * count;

            if (readpos_ + n > writepos_)
            {
                return false;
            }

            auto* buff = std::addressof(*begin());
            memcpy(Outdata, buff, n);
            readpos_ += n;
            return true;
        }

        void consume(std::size_t n) noexcept
        {
            seek(n);
        }

        //set read or forward read pos
        bool seek(size_t offset, seek_origin s = seek_origin::Current) noexcept
        {
            switch (s)
            {
            case seek_origin::Begin:
                if (offset > writepos_)
                    return false;
                readpos_ = offset;
                break;
            case seek_origin::Current:
                if (readpos_ + offset > writepos_)
                    return false;
                readpos_ += offset;
                break;
            default:
                assert(false);
				return false;
            }
            return true;
        }

        void clear() noexcept
        {
            flag_ = 0;
            writepos_ = readpos_ = headreserved_;
        }

        template<typename ValueType>
        void set_flag(ValueType v) noexcept
        {
            flag_ |= static_cast<uint32_t>(v);
        }

        template<typename ValueType>
        bool has_flag(ValueType v) const noexcept
        {
            return ((flag_ & static_cast<uint32_t>(v)) != 0);
        }

        template<typename ValueType>
        void clear_flag(ValueType v) noexcept
        {
            flag_ &= ~static_cast<uint32_t>(v);
        }

        void commit(std::size_t n) noexcept
        {
            writepos_ += n;
            assert(writepos_ <= capacity_);
            if (writepos_ >= capacity_)
            {
                writepos_ = capacity_;
            }
        }

        pointer revert(size_t n) noexcept
        {
            assert(writepos_ >= (readpos_+n));
            if (writepos_ >= n)
            {
                writepos_ -= n;
            }
            return (data_ + writepos_);
        }

        const_iterator begin() const noexcept
        {
            return const_iterator{ data_ + readpos_ };
        }

        iterator begin() noexcept
        {
            return iterator{ data_ + readpos_ };
        }

        const_iterator end() const noexcept
        {
            return const_iterator{ data_ + writepos_ };
        }

        iterator end() noexcept
        {
            return iterator{ data_ + writepos_ };
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
        size_t size() const noexcept
        {
            return writepos_ - readpos_;
        }

        size_t capacity() const noexcept
        {
            return capacity_;
        }

        size_t reserved() const noexcept
        {
            return headreserved_;
        }

        pointer prepare(size_t need)
        {
            if (writeablesize() >= need)
            {
                return (data_ + writepos_);
            }

            if (writeablesize() + readpos_ < need + headreserved_)
            {
                auto required_size = writepos_ + need;
                required_size = next_pow2(required_size);
                auto tmp = allocator_.allocate(required_size);
                if (nullptr != data_)
                {
                    std::memcpy(tmp, data_, writepos_);
                    allocator_.deallocate(data_, capacity_);
                }
                data_ = tmp;
                capacity_ = required_size;
            }
            else
            {
                size_t readable = size();
                if (readable != 0)
                {
                    assert(readpos_ >= headreserved_);
                    std::memmove(data_ + headreserved_, data_ + readpos_, readable);
                }
                readpos_ = headreserved_;
                writepos_ = readpos_ + readable;
            }
            return (data_ + writepos_);
        }

        size_t writeablesize() const noexcept
        {
            assert(capacity_ >= writepos_);
            return capacity_ - writepos_;
        }

    private:
        size_t next_pow2(size_t x)
        {
            if (!(x & (x - 1)))
            {
                return x;
            }
            x |= x >> 1;
            x |= x >> 2;
            x |= x >> 4;
            x |= x >> 8;
            x |= x >> 16;
            return x + 1;
        }
    private:
        allocator_type allocator_;

        uint32_t flag_;

        uint32_t headreserved_;

        size_t capacity_;
        //read position
        size_t readpos_;
        //write position
        size_t writepos_;
        value_type* data_ = nullptr;
    };
};

#ifdef MOON_ENABLE_MIMALLOC
#include "mimalloc.h"
namespace moon
{
    using buffer = base_buffer<mi_stl_allocator<char>>;
}
#else
namespace moon
{
    using buffer = base_buffer<std::allocator<char>>;
}
#endif



