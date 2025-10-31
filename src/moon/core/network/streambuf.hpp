#pragma once
#include "asio.hpp"
#include "common/buffer.hpp"
#include <cassert>

namespace moon {
using buffer_t = buffer;

/// @brief ASIO-compatible buffer wrapper for moon::buffer
/// This class adapts moon::buffer to work with ASIO's async read/write operations
class streambuf {
public:
    using const_buffers_type = asio::const_buffer;
    using mutable_buffers_type = asio::mutable_buffer;

    /// @brief Construct a streambuf with a buffer pointer and optional max size
    /// @param buf Pointer to the underlying buffer (must not be null)
    /// @param maxsize Maximum size limit for read operations
    explicit streambuf(buffer_t* buf, std::size_t maxsize = std::numeric_limits<std::size_t>::max()) noexcept:
        buffer_(buf),
        max_size_(maxsize) {
        assert(buf != nullptr && "streambuf requires a non-null buffer pointer");
    }

    // Non-copyable for safety
    streambuf(const streambuf&) = delete;
    streambuf& operator=(const streambuf&) = delete;
    
    // Movable
    streambuf(streambuf&&) noexcept = default;
    streambuf& operator=(streambuf&&) noexcept = default;

    /// @brief Get the current size of data in the buffer
    std::size_t size() const noexcept {
        assert(buffer_ != nullptr);
        return buffer_->size();
    }

    /// @brief Get the maximum allowed size for this streambuf
    std::size_t max_size() const noexcept {
        return max_size_;
    }

    /// @brief Get the total capacity of the underlying buffer
    std::size_t capacity() const noexcept {
        assert(buffer_ != nullptr);
        return buffer_->capacity();
    }

    /// @brief Get a const buffer view of the current data
    const_buffers_type data() const noexcept {
        assert(buffer_ != nullptr);
        return asio::const_buffer { buffer_->data(), buffer_->size() };
    }

    /// @brief Prepare a mutable buffer for writing up to n bytes
    /// @param n Number of bytes to prepare
    /// @return Mutable buffer ready for writing
    /// @note If n exceeds max_size_, the actual prepared size may be limited
    mutable_buffers_type prepare(std::size_t n) {
        assert(buffer_ != nullptr);
        
        // Respect max_size limit by clamping the requested size
        // This prevents excessive memory allocation while allowing ASIO operations to proceed
        n = std::min(n, max_size_);
        
        auto [ptr, size] = buffer_->prepare(n);
        return asio::mutable_buffer { ptr, size };
    }

    /// @brief Commit n bytes of data that have been written to the buffer
    /// @param n Number of bytes to commit
    void commit(std::size_t n) noexcept {
        assert(buffer_ != nullptr);
        assert(n <= buffer_->capacity() - buffer_->size() && "commit size exceeds prepared space");
        buffer_->commit_unchecked(n);
    }

    /// @brief Consume (remove) n bytes from the front of the buffer
    /// @param n Number of bytes to consume
    void consume(std::size_t n) noexcept {
        assert(buffer_ != nullptr);
        assert(n <= buffer_->size() && "consume size exceeds buffer size");
        buffer_->consume_unchecked(n);
    }

private:
    buffer_t* buffer_;
    std::size_t max_size_;
};
} // namespace moon