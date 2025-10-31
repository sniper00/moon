#pragma once
#include "asio.hpp"
#include "common/buffer.hpp"
#include "config.hpp"
#include <deque>

namespace moon {
class write_queue {
public:
    write_queue() = default;

    // Enqueue data into the send queue
    size_t enqueue(buffer_shr_ptr_t data) {
        send_queue_.emplace_back(std::move(data));
        return send_queue_.size();
    }

    // Get the number of writable buffers
    size_t writeable() const noexcept {
        return send_queue_.size();
    }

    // Commit the written data
    void commit_written() {
        while (consume_size_-- > 0) {
            send_queue_.pop_front();
        }
        consume_size_ = 0;
        buffer_sequences_.clear();
        padding_.clear();
    }

    // Prepare each buffer and call the handler
    template<typename Handler>
    void prepare_buffers(const Handler& handler, size_t max_bytes = 0) {
        size_t bytes = 0;
        for (size_t i = 0, n = send_queue_.size(); i < n; ++i) {
            const auto& elm = send_queue_[i];
            handler(elm);
            if (max_bytes && (bytes += elm->size()) >= max_bytes) {
                break;
            }
        }
    }

    void consume(const char* data = nullptr, size_t len = 0) {
        if (data && len > 0) {
            buffer_sequences_.emplace_back(data, len);
        }
        ++consume_size_;
    }

    // Prepare data with padding
    void prepare_with_padding(
        const void* padding_data,
        size_t padding_size,
        const char* data,
        size_t len
    ) {
        auto& space = padding_.emplace_front();
        size_t n = std::min(space.size(), padding_size);
        memcpy(space.data(), padding_data, n);
        buffer_sequences_.emplace_back(space.data(), n);
        if (len > 0) {
            buffer_sequences_.emplace_back(data, len);
        }
    }

    const std::vector<asio::const_buffer>& buffer_sequence() const noexcept {
        return buffer_sequences_;
    }

private:
    // Number of buffers to be sent
    size_t consume_size_ = 0;
    // Stores padding data for specific protocols
    std::deque<std::array<char, 16>> padding_;
    // Data structure needed for ASIO write operations
    std::vector<asio::const_buffer> buffer_sequences_;
    // Queue for buffers waiting to be sent
    VecDeque<buffer_shr_ptr_t> send_queue_;
};

/*
    Don't pass heavy-weight buffer sequences to async IO operations
    https://github.com/chriskohlhoff/asio/issues/203
     */
template<class BufferSequence>
class buffers_ref {
    BufferSequence const& buffers_;

public:
    using value_type = typename BufferSequence::value_type;

    using const_iterator = typename BufferSequence::const_iterator;

    buffers_ref(buffers_ref const&) = default;

    explicit buffers_ref(BufferSequence const& buffers): buffers_(buffers) {}

    const_iterator begin() const {
        return buffers_.begin();
    }

    const_iterator end() const {
        return buffers_.end();
    }
};

// Return a reference to a buffer sequence
template<class BufferSequence>
buffers_ref<BufferSequence> make_buffers_ref(BufferSequence const& buffers) {
    return buffers_ref<BufferSequence>(buffers);
}
} // namespace moon
