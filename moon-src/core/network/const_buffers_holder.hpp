#pragma once
#include "config.hpp"
#include "common/buffer.hpp"
#include "asio.hpp"

namespace moon
{
    class const_buffers_holder
    {
    public:
        static constexpr size_t max_count = 64;

        const_buffers_holder() = default;

        void push_back(const char* data, size_t len)
        {
            buffers_.emplace_back(data, len);
            ++count_;
        }

        void push()
        {
            ++count_;
        }

        void push_slice(message_size_t header, const char* data, size_t len)
        {
            headers_.emplace_front(header);
            message_size_t& value = headers_.front();
            buffers_.emplace_back(reinterpret_cast<const char*>(&value), sizeof(value));
            if(len>0)
                buffers_.emplace_back(data, len);
        }

        const auto& buffers() const
        {
            return buffers_;
        }

        size_t size() const
        {
            return buffers_.size();
        }

        //hold buffer's count
        size_t count() const
        {
            return count_;
        }

        void clear()
        {
            count_ = 0;
            buffers_.clear();
            headers_.clear();
        }
    private:
        size_t count_ = 0;
        std::vector<asio::const_buffer> buffers_;
        std::forward_list<message_size_t> headers_;
    };

    /*
    Don't pass heavy-weight buffer sequences to async IO operations
    https://github.com/chriskohlhoff/asio/issues/203
     */
    template<class BufferSequence>
    class buffers_ref
    {
        BufferSequence const& buffers_;

    public:
        using value_type = typename BufferSequence::value_type;

        using const_iterator = typename BufferSequence::const_iterator;

        buffers_ref(buffers_ref const&) = default;

        explicit
            buffers_ref(BufferSequence const& buffers)
            : buffers_(buffers)
        {
        }

        const_iterator
            begin() const
        {
            return buffers_.begin();
        }

        const_iterator
            end() const
        {
            return buffers_.end();
        }
    };

    // Return a reference to a buffer sequence
    template<class BufferSequence>
    buffers_ref<BufferSequence>
        make_buffers_ref(BufferSequence const& buffers)
    {
        return buffers_ref<BufferSequence>(buffers);
    }
}

