#pragma once
#include "common/buffer.hpp"
#include "config.hpp"

namespace moon {
struct message {
private:
    uint8_t data_type_ = 0; //0:bytes, 1:object ptr or integer

public:
    uint8_t type = 0;
    uint32_t sender = 0;
    uint32_t receiver = 0;
    int64_t session = 0;

private:
    void* data_ = nullptr;

public:
    message(): data_(new buffer {}) {}

    message(size_t capacity): data_(capacity > 0 ? new buffer { capacity } : nullptr) {}

    message(uint8_t t, uint32_t s, uint32_t r, int64_t sid):
        type(t),
        sender(s),
        receiver(r),
        session(sid) {}

    message(uint8_t t, uint32_t s, uint32_t r, int64_t sid, ssize_t ptr):
        data_type_(1),
        type(t),
        sender(s),
        receiver(r),
        session(sid),
        data_((void*)ptr) {}

    message(uint8_t t, uint32_t s, uint32_t r, int64_t sid, buffer_ptr_t&& d):
        type(t),
        sender(s),
        receiver(r),
        session(sid),
        data_(d.release()) {}

    message(uint8_t t, uint32_t s, uint32_t r, int64_t sid, std::string_view d):
        type(t),
        sender(s),
        receiver(r),
        session(sid),
        data_(new buffer { d.size() }) {
        static_cast<buffer*>(data_)->write_back(d.data(), d.size());
    }

    message(const message&) = delete;
    message& operator=(const message&) = delete;

    message(message&& other) noexcept:
        data_type_(other.data_type_),
        type(other.type),
        sender(other.sender),
        receiver(other.receiver),
        session(other.session),
        data_(std::exchange(other.data_, nullptr)) {}

    message& operator=(message&& other) noexcept {
        if (this != &other) {
            data_type_ = other.data_type_;
            type = other.type;
            sender = other.sender;
            receiver = other.receiver;
            session = other.session;
            data_ = std::exchange(other.data_, nullptr);
        }
        return *this;
    }

    ~message() {
        if (data_ && (!data_type_)) {
            delete static_cast<buffer*>(data_);
        }
    }

    const char* data() const noexcept {
        return data_ && (!data_type_) ? static_cast<buffer*>(data_)->data() : nullptr;
    }

    size_t size() const noexcept {
        return data_ && (!data_type_) ? static_cast<buffer*>(data_)->size() : 0;
    }

    buffer_ptr_t into_buffer() {
        if (!data_ || data_type_) {
            return nullptr;
        }
        return buffer_ptr_t { static_cast<buffer*>(std::exchange(data_, nullptr)) };
    }

    buffer* as_buffer() const noexcept {
        return data_ && (!data_type_) ? static_cast<buffer*>(data_) : nullptr;
    }

    bool is_bytes() const noexcept {
        return data_type_ == 0;
    }

    ssize_t as_ptr() const noexcept {
        return data_ && data_type_ ? (ssize_t)data_ : 0;
    }
};

}; // namespace moon
