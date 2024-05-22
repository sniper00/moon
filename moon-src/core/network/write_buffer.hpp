#pragma once
#include "config.hpp"
#include <deque>
#include "common/buffer.hpp"
#include "asio.hpp"

namespace moon
{
    class write_buffer
    {
    public:
        write_buffer() = default;

        void write(const char* data, size_t len)
        {
            buffers_.emplace_back(data, len);
            ++size_;
        }

        void begin_write_slice()
        {
            ++size_;
        }

        void write_slice(void* padding_data, size_t padding_size, const char* data, size_t len)
        {
            auto& space = padding_.emplace_front();
            size_t n = std::min(space.size(), padding_size);
            memcpy(space.data(), padding_data, n);
            buffers_.emplace_back(space.data(), n);
            if(len>0)
                buffers_.emplace_back(data, len);
        }

        const auto& buffers() const
        {
            return buffers_;
        }

        size_t size() const
        {
            return size_;
        }

        void clear()
        {
            size_ = 0;
            buffers_.clear();
            padding_.clear();
        }
    private:
        size_t size_ = 0;
        std::vector<asio::const_buffer> buffers_;
        std::deque<std::array<char, 16>> padding_;
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

