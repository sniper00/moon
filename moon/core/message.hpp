#pragma once
#include "config.hpp"
#include "common/buffer.hpp"

namespace moon
{
    class  message final
    {
    public:
        static buffer_ptr_t create_buffer(size_t capacity = 64, uint32_t headreserved = BUFFER_HEAD_RESERVED)
        {
            return std::make_shared<buffer>(capacity, headreserved);
        }

        static message_ptr_t create(size_t capacity = 64, uint32_t headreserved = BUFFER_HEAD_RESERVED)
        {
            return std::make_unique<message>(capacity, headreserved);
        }

        template<typename Buffer,std::enable_if_t<std::is_same_v<std::decay_t<Buffer>,buffer_ptr_t>,int> = 0>
        static message_ptr_t create(Buffer&& v)
        {
            return std::make_unique<message>(std::forward<Buffer>(v));
        }

        message(size_t capacity = 64, uint32_t headreserved = 0)
        {
            data_ = std::make_shared<buffer>(capacity, headreserved);
        }

        template<typename Buffer, std::enable_if_t<std::is_same_v<std::decay_t<Buffer>, buffer_ptr_t>, int> = 0>
        explicit message(Buffer&& v)
            :data_(std::forward<Buffer>(v))
        {
            
        }

        ~message()
        {
        }

        message(const message&) = delete;

        message& operator=(const message&) = delete;

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

        void set_header(string_view_t header)
        {
            if (header.size() != 0)
            {
                if (!header_)
                {
                    header_ = std::make_unique<std::string>(header.data(), header.size());
                }
                else
                {
                    header_->clear();
                    header_->assign(header.data(), header.size());
                }
            }
        }

        string_view_t header() const
        {
            if (nullptr == header_)
            {
                return string_view_t{};
            }
            else
            {
                return string_view_t{ header_->data(),header_->size() };
            }
        }

        void set_responseid(int32_t v)
        {
            responseid_ = v;
        }

        int32_t sessionid() const
        {
            return responseid_;
        }

        void set_type(uint8_t v)
        {
            type_ = v;
        }

        uint8_t type() const
        {
            return type_;
        }

        void set_subtype(uint8_t v)
        {
            subtype_ = v;
        }

        uint8_t subtype() const
        {
            return subtype_;
        }

        string_view_t bytes() const
        {
            if (!data_)
            {
                return string_view_t(nullptr, 0);
            }
            return string_view_t(reinterpret_cast<const char*>(data_->data()), data_->size());
        }

        string_view_t substr(int pos, size_t len = string_view_t::npos) const
        {
            if (!data_)
            {
                return string_view_t(nullptr, 0);
            }
            string_view_t sr(reinterpret_cast<const char*>(data_->data()), data_->size());
            return sr.substr(pos, len);
        }

        void write_data(string_view_t s)
        {
            assert(data_);
            data_->write_back(s.data(), 0, s.size());
        }

        void write_string(const std::string& s)
        {
            assert(data_);
            data_->write_back(s.data(), 0, s.size() + 1);
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

        bool broadcast() const
        {
            return data_?data_->has_flag(buffer_flag::broadcast):false;
        }

        void set_broadcast(bool v)
        {
            if (!data_)
            {
                return;
            }
            v ? data_->set_flag(buffer_flag::broadcast) : data_->clear_flag(buffer_flag::broadcast);
        }

        void reset()
        {
            type_ = 0;
            subtype_ = 0;
            sender_ = 0;
            receiver_ = 0;
            responseid_ = 0;

            if (header_)
            {
                header_->clear();
            }

            if (data_)
            {
                data_->clear();
            }
        }
    private:
        uint8_t type_ = 0;
        uint8_t subtype_ = 0;
        uint32_t sender_ = 0;
        uint32_t receiver_ = 0;
        int32_t responseid_ = 0;
        std::unique_ptr<std::string> header_;
        buffer_ptr_t data_;
    };
};


