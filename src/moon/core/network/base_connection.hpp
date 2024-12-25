#pragma once
#include "asio.hpp"
#include "common/string.hpp"
#include "common/vec_deque.hpp"
#include "config.hpp"
#include "error.hpp"
#include "message.hpp"
#include "write_queue.hpp"

namespace moon {
enum class connection_mask : uint8_t {
    none = 0,
    server = 1 << 0,
    would_close = 1 << 1,
    reading = 1 << 2,
    chunked_recv = 1 << 3,
    chunked_send = 1 << 4,
    chunked_both = 1 << 5,
};

template<>
struct enum_enable_bitmask_operators<connection_mask> {
    static constexpr bool enable = true;
};

class base_connection: public std::enable_shared_from_this<base_connection> {
public:
    using socket_t = asio::ip::tcp::socket;

    template<typename... Args>
    explicit base_connection(
        uint32_t serviceid,
        uint8_t type,
        moon::socket_server* s,
        Args&&... args
    ):
        type_(type),
        serviceid_(serviceid),
        parent_(s),
        socket_(std::forward<Args>(args)...) {}

    base_connection(const base_connection&) = delete;

    base_connection& operator=(const base_connection&) = delete;

    virtual ~base_connection() = default;

    virtual void start(bool server) {
        if (!server) {
            mask_ = enum_unset_bitmask(mask_, connection_mask::server);
        }
        recvtime_ = now();
    }

    virtual direct_read_result read(size_t, std::string_view, int64_t) {
        CONSOLE_ERROR("Unsupported read operation for PTYPE %d", (int)type_);
        parent_->close(fd_);
        parent_ = nullptr;
        return direct_read_result { false, { "Unsupported read operation" } };
    };

    virtual bool send(buffer_shr_ptr_t data) {
        if (!socket_.is_open()) {
            return false;
        }

        if (wq_warn_size_ != 0 && wqueue_.writeable() >= wq_warn_size_) {
            CONSOLE_WARN("network send queue too long. size:%zu", wqueue_.writeable());
            if (wq_error_size_ != 0 && wqueue_.writeable() >= wq_error_size_) {
                asio::post(socket_.get_executor(), [this, self = shared_from_this()]() {
                    error(make_error_code(moon::error::send_queue_too_big));
                });
                return false;
            }
        }

        if (data->has_bitmask(socket_send_mask::close)) {
            mask_ = mask_ | connection_mask::would_close;
        }

        if (wqueue_.enqueue(std::move(data)) == 1) {
            post_send();
        }

        return true;
    }

    void close() {
        if (socket_.is_open()) {
            asio::error_code ignore_ec;
            socket_.shutdown(asio::ip::tcp::socket::shutdown_both, ignore_ec);
            socket_.close(ignore_ec);
        }
    }

    socket_t& socket() {
        return socket_;
    }

    bool is_open() const {
        return socket_.is_open();
    }

    void fd(uint32_t fd) {
        fd_ = fd;
    }

    uint32_t fd() const {
        return fd_;
    }

    uint8_t type() const {
        return type_;
    }

    uint32_t owner() const {
        return serviceid_;
    }

    bool is_server() const {
        return enum_has_any_bitmask(mask_, connection_mask::server);
    }

    void timeout(time_t now) {
        if ((0 != timeout_) && (0 != recvtime_) && (now - recvtime_ > timeout_)) {
            asio::post(socket_.get_executor(), [this, self = shared_from_this()]() {
                error(make_error_code(moon::error::read_timeout));
            });
            return;
        }
        return;
    }

    bool set_no_delay() {
        asio::ip::tcp::no_delay option(true);
        asio::error_code ec;
        socket_.set_option(option, ec);
        return !ec;
    }

    void settimeout(uint32_t v) {
        timeout_ = v;
        recvtime_ = now();
    }

    void set_send_queue_limit(uint16_t warnsize, uint16_t errorsize) {
        wq_warn_size_ = warnsize;
        wq_error_size_ = errorsize;
    }

    static time_t now() {
        return std::time(nullptr);
    }

    std::string address() {
        std::string address;
        asio::error_code ec;
        auto endpoint = socket_.remote_endpoint(ec);
        if (!ec) {
            address.append(endpoint.address().to_string());
            address.append(":");
            address.append(std::to_string(endpoint.port()));
        }
        return address;
    }

protected:
    virtual void prepare_send(size_t default_once_send_bytes) {
        wqueue_.prepare_buffers(
            [this](const buffer_shr_ptr_t& elm) { wqueue_.consume(elm->data(), elm->size()); },
            default_once_send_bytes
        );
    }

    void post_send() {
        prepare_send(262144);

        asio::async_write(
            socket_,
            make_buffers_ref(wqueue_.buffer_sequence()),
            [this, self = shared_from_this()](const asio::error_code& e, std::size_t) {
                if (e) {
                    error(e);
                    return;
                }

                wqueue_.commit_written();

                if (wqueue_.writeable() > 0) {
                    post_send();
                } else if (enum_has_any_bitmask(mask_, connection_mask::would_close)
                           && parent_ != nullptr)
                {
                    parent_->close(fd_);
                    parent_ = nullptr;
                }
            }
        );
    }

    virtual void error(const asio::error_code& e, const std::string& additional = "") {
        if (nullptr == parent_) {
            return;
        }
        std::string str = e.message();
        if (!additional.empty()) {
            str.append("(");
            str.append(additional);
            str.append(")");
        }
        std::string content = moon::format(
            R"({"addr":"%s","code":%d,"message":"%s"})",
            address().data(),
            e.value(),
            str.data()
        );

        parent_->close(fd_);
        handle_message(message{
            type_, 0, static_cast<uint8_t>(socket_data_type::socket_close), 0, content
        });
        parent_ = nullptr;
    }

    template<typename Message>
    void handle_message(Message&& m) {
        recvtime_ = now();
        if (nullptr != parent_) {
            m.sender = fd_;
            parent_->handle_message(serviceid_, std::forward<Message>(m));
        }
    }

protected:
    connection_mask mask_ = connection_mask::server;
    uint8_t type_ = 0;
    uint16_t wq_warn_size_ = 0;
    uint16_t wq_error_size_ = 0;
    uint32_t fd_ = 0;
    uint32_t timeout_ = 0;
    uint32_t serviceid_ = 0;
    time_t recvtime_ = 0;
    moon::socket_server* parent_;
    write_queue wqueue_;
    socket_t socket_;
};
} // namespace moon