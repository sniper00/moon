#pragma once
#include "config.hpp"
#include "common/buffer.hpp"

namespace moon
{
    class message final
    {
    public:
        static buffer_ptr_t create_buffer(size_t capacity = 64, uint32_t headreserved = BUFFER_HEAD_RESERVED)
        {
            return std::make_shared<buffer>(capacity, headreserved);
        }

        message(size_t capacity = 64, uint32_t headreserved = 0)
            :data_(std::make_shared<buffer>(capacity, headreserved))
        {
        }

        template<typename Buffer, std::enable_if_t<std::is_same_v<std::decay_t<Buffer>, buffer_ptr_t>, int> = 0>
        explicit message(Buffer&& v)
            :data_(std::forward<Buffer>(v))
        {
            
        }

        ~message() = default;

        message(const message&) = delete;

        message& operator=(const message&) = delete;

        message(message&& other) noexcept
            :type_(std::exchange(other.type_, uint8_t{}))
            , sender_(std::exchange(other.sender_, 0))
            , receiver_(std::exchange(other.receiver_, 0))
            , sessionid_(std::exchange(other.sessionid_, 0))
            , data_(std::move(other.data_))
        {
        }

        message& operator=(message&& other) noexcept
        {
            if (this != std::addressof(other))
            {
                type_ = std::exchange(other.type_, uint8_t{});
                sender_ = std::exchange(other.sender_, 0);
                receiver_ = std::exchange(other.receiver_, 0);
                sessionid_ = std::exchange(other.sessionid_, 0);
                data_ = std::move(other.data_);
            }
            return *this;
        }

        void set_sender(uint32_t serviceid)
        {
            sender_ = serviceid;
        }

        uint32_t sender() const
        {
            return sender_;
        }

        void set_receiver(uint32_t serviceid)
        {
            receiver_ = serviceid;
        }

        uint32_t receiver() const
        {
            return receiver_;
        }

        void set_sessionid(int32_t v)
        {
            sessionid_ = v;
        }

        int32_t sessionid() const
        {
            return sessionid_;
        }

        void set_type(uint8_t v)
        {
            type_ = v;
        }

        uint8_t type() const
        {
            return type_;
        }

        bool broadcast() const
        {
            return (data_ && data_->has_flag(buffer_flag::broadcast));
        }

        void set_broadcast(bool v)
        {
            if (!data_)
            {
                return;
            }
            v ? data_->set_flag(buffer_flag::broadcast) : data_->clear_flag(buffer_flag::broadcast);
        }

        void write_data(std::string_view s)
        {
            assert(data_);
            data_->write_back(s.data(), s.size());
        }

        const char* data() const
        {
            return data_ ? data_->data() : nullptr;
        }

        size_t size() const
        {
            return data_ ? data_->size():0;
        }

        operator const buffer_ptr_t&() const
        {
            return data_;
        }

        buffer* get_buffer()
        {
            return data_ ? data_.get() : nullptr;
        }
    private:
        uint8_t type_ = 0;
        uint32_t sender_ = 0;
        uint32_t receiver_ = 0;
        int32_t sessionid_ = 0;
        buffer_ptr_t data_;
    };
};


