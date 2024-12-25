#pragma once
#include "common/buffer.hpp"
#include "config.hpp"

namespace moon {
struct message {
    uint8_t type = 0;
    uint32_t sender = 0;
    uint32_t receiver = 0;
    int64_t session = 0;
    ssize_t stub = 0;//Save pointer or integer value

private:
    buffer_ptr_t data_ = nullptr;

public:
    message(): data_(buffer::make_unique()) {}

    message(size_t capacity): data_(capacity > 0 ? buffer::make_unique(capacity) : nullptr) {}

    message(uint8_t t, uint32_t s, uint32_t r, int64_t sid):
        type(t),
        sender(s),
        receiver(r),
        session(sid) {}

    message(uint8_t t, uint32_t s, uint32_t r, int64_t sid, ssize_t p):
        type(t),
        sender(s),
        receiver(r),
        session(sid),
        stub(p) {}

    message(uint8_t t, uint32_t s, uint32_t r, int64_t sid, buffer_ptr_t&& d):
        type(t),
        sender(s),
        receiver(r),
        session(sid),
        data_(std::move(d)) {}

    message(uint8_t t, uint32_t s, uint32_t r, int64_t sid, std::string_view d):
        type(t),
        sender(s),
        receiver(r),
        session(sid),
        data_(buffer::make_unique(d.size())) {
        data_->write_back(d.data(), d.size());
    }

    const char* data() const {
        return data_ ? data_->data() : nullptr;
    }

    size_t size() const {
        return data_ ? data_->size() : 0;
    }

    buffer_ptr_t into_buffer() {
        return std::move(data_);
    }

    buffer* as_buffer() {
        return data_ ? data_.get() : nullptr;
    }
};

}; // namespace moon
