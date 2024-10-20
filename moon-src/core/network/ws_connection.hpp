#pragma once
#include "base_connection.hpp"
#include "common/base64.hpp"
#include "common/byte_convert.hpp"
#include "common/http_utility.hpp"
#include "common/sha1.hpp"
#include "streambuf.hpp"

//https://developer.mozilla.org/en-US/docs/Web/API/WebSockets_API/Writing_WebSocket_servers

namespace moon {
namespace ws {
    enum close_code : std::uint16_t {
        /// Normal closure; the connection successfully completed whatever purpose for which it was created.
        normal = 1000,
        /// The endpoint is going away, either because of a server failure or because the browser is navigating away from the page that opened the connection.
        going_away = 1001,
        /// The endpoint is terminating the connection due to a protocol error.
        protocol_error = 1002,
        /// The connection is being terminated because the endpoint received data of a type it cannot accept (for example, a text-only endpoint received binary data).
        unknown_data = 1003,
        /// The endpoint is terminating the connection because a message was received that contained inconsistent data (e.g., non-UTF-8 data within a text message).
        bad_payload = 1007,
        /// The endpoint is terminating the connection because it received a message that violates its policy. This is a generic status code, used when codes 1003 and 1009 are not suitable.
        policy_error = 1008,
        /// The endpoint is terminating the connection because a data frame was received that is too large.
        too_big = 1009,
        /// The client is terminating the connection because it expected the server to negotiate one or more extension, but the server didn't.
        needs_extension = 1010,
        /// The server is terminating the connection because it encountered an unexpected condition that prevented it from fulfilling the request.
        internal_error = 1011,
        /// The server is terminating the connection because it is restarting.
        service_restart = 1012,
        /// The server is terminating the connection due to a temporary condition, e.g. it is overloaded and is casting off some of its clients.
        try_again_later = 1013,
        //----
        //
        // The following are illegal on the wire
        //
        /** Used internally to mean "no error"
            This code is reserved and may not be sent.
            */
        none = 0,
        /** Reserved for future use by the WebSocket standard.
            This code is reserved and may not be sent.
            */
        reserved1 = 1004,
        /** No status code was provided even though one was expected.
            This code is reserved and may not be sent.
            */
        no_status = 1005,
        /** Connection was closed without receiving a close frame
            This code is reserved and may not be sent.
            */
        abnormal = 1006,
        /** Reserved for future use by the WebSocket standard.
            This code is reserved and may not be sent.
            */
        reserved2 = 1014,
        /** Reserved for future use by the WebSocket standard.
            This code is reserved and may not be sent.
            */
        reserved3 = 1015
        //
        //----
        //last = 5000 // satisfy warnings
    };

    enum class opcode : std::uint8_t {
        incomplete = 0,
        text = 1,
        binary = 2,
        rsv3 = 3,
        rsv4 = 4,
        rsv5 = 5,
        rsv6 = 6,
        rsv7 = 7,
        close = 8,
        ping = 9,
        pong = 10,
        crsvb = 11,
        crsvc = 12,
        crsvd = 13,
        crsve = 14,
        crsvf = 15
    };

    using mask_key_type = std::array<uint8_t, 4>;

    struct frame_header {
        bool fin;
        bool rsv1;
        bool rsv2;
        bool rsv3;
        bool mask;
        opcode op; //4bit
        uint8_t payload_len; //7 bit
        mask_key_type mask_key;
    };
} // namespace ws

class ws_connection: public base_connection {
public:
    using base_connection_t = base_connection;

    static constexpr size_t HANDSHAKE_STREAMBUF_SIZE = 2048;

    static constexpr size_t SEC_WEBSOCKET_KEY_LEN = 24;

    static constexpr size_t PAYLOAD_MIN_LEN = 125;
    static constexpr size_t PAYLOAD_MID_LEN = 126;
    static constexpr size_t PAYLOAD_MAX_LEN = 127;
    static constexpr size_t FIN_FRAME_FLAG = 0x80; // 1 0 0 0 0 0 0 0

    static constexpr const std::string_view WEBSOCKET = "websocket"sv;
    static constexpr const std::string_view UPGRADE = "upgrade"sv;
    static constexpr const std::string_view WS_MAGICKEY = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"sv;

    template<
        typename... Args,
        std::enable_if_t<
            !std::disjunction_v<std::is_same<std::decay_t<Args>, ws_connection>...>,
            int> = 0>
    explicit ws_connection(Args&&... args):
        base_connection_t(std::forward<Args>(args)...),
        cache_(HANDSHAKE_STREAMBUF_SIZE) {}

    void start(bool server) override {
        base_connection_t::start(server);
        if (server) {
            read_handshake();
        } else {
            read_payload(2);
        }
    }

private:
    void read_handshake() {
        asio::async_read_until(
            socket_,
            moon::streambuf(&cache_, cache_.capacity()),
            STR_DCRLF,
            [this, self = shared_from_this()](const asio::error_code& e, std::size_t size) {
                if (e) {
                    error(e);
                    return;
                }

                auto ec = handshake(size);
                if (ec) {
                    send_response("HTTP/1.1 400 Bad Request\r\n\r\n", true);
                }
            }
        );
    }

    std::error_code handshake(size_t n) {
        std::string_view method;
        std::string_view ignore;
        std::string_view version;
        http::case_insensitive_multimap_view header;
        if (!http::request_parser::parse(
                std::string_view { cache_.data(), n },
                method,
                ignore,
                ignore,
                version,
                header
            ))
        {
            return make_error_code(moon::error::ws_bad_http_header);
        }

        if (version < "1.0" || version > "1.1") {
            return make_error_code(moon::error::ws_bad_http_version);
        }

        if (method != "GET"sv)
            return make_error_code(moon::error::ws_bad_method);

        std::string_view connection;
        if (!moon::try_get_value(header, "connection"sv, connection))
            return make_error_code(moon::error::ws_no_connection);

        std::string_view upgrade;
        if (!moon::try_get_value(header, "upgrade"sv, upgrade))
            return make_error_code(moon::error::ws_no_upgrade);

        if (!iequal_string(connection, UPGRADE))
            return make_error_code(moon::error::ws_no_connection_upgrade);

        if (!iequal_string(upgrade, WEBSOCKET))
            return make_error_code(moon::error::ws_no_upgrade_websocket);

        std::string_view sec_ws_key;
        if (!moon::try_get_value(header, "sec-websocket-key"sv, sec_ws_key))
            return make_error_code(moon::error::ws_no_sec_key);

        if (base64_decode(std::string { sec_ws_key.data(), sec_ws_key.size() }).size() != 16)
            return make_error_code(moon::error::ws_bad_sec_key);

        std::string_view sec_ws_version;
        if (!moon::try_get_value(header, "sec-websocket-version"sv, sec_ws_version))
            return make_error_code(moon::error::ws_no_sec_version);

        if (sec_ws_version != "13"sv) {
            return make_error_code(moon::error::ws_bad_sec_version);
        }

        std::string_view protocol;
        moon::try_get_value(header, "sec-websocket-protocol"sv, protocol);

        auto answer = upgrade_response(sec_ws_key, protocol);
        send_response(answer);
        auto msg = message { n };
        msg.set_type(type_);
        msg.write_data(std::string_view { cache_.data(), n });
        msg.set_receiver(static_cast<uint8_t>(socket_data_type::socket_accept));
        handle_message(std::move(msg));

        cache_.consume(n);

        handle_frame();

        return std::error_code {};
    }

    void read_payload(size_t size) {
        asio::async_read(
            socket_,
            moon::streambuf(&cache_, cache_.capacity()),
            asio::transfer_at_least(size),
            [this, self = shared_from_this()](const asio::error_code& ec, std::size_t) {
                if (ec) {
                    error(ec);
                    return;
                }

                handle_frame();
            }
        );
    }

    void send_response(std::string_view s, bool will_close = false) {
        auto buf = std::make_shared<buffer>(s.size());
        buf->write_back(s.data(), s.size());
        buf->add_bitmask(
            socket_send_mask::raw | (will_close ? socket_send_mask::close : socket_send_mask::none)
        );
        base_connection::send(std::move(buf));
    }

    template<typename T>
    static T read_be_uint(const uint8_t* data) {
        T result = 0;
        for (size_t i = 0; i < sizeof(T); ++i) {
            result = (result << 8) | static_cast<T>(data[i]);
        }
        return result;
    }

    void handle_frame() {
        size_t size = cache_.size();

        // Check if the size of the data is less than 2
        // According to the WebSocket protocol, a frame must have at least 2 bytes
        // The first byte contains FIN bit and opcode
        // The second byte contains the mask bit and payload length
        if (size < 2) {
            return read_payload(2);
        }

        auto frame_data = (const uint8_t*)(cache_.data());

        size_t header_size = 2;
        ws::frame_header fh {};

        fh.payload_len = frame_data[1] & 0x7F;
        switch (fh.payload_len) {
            case PAYLOAD_MID_LEN:
                header_size += 2;
                break;
            case PAYLOAD_MAX_LEN:
                header_size += 8;
                break;
            default:
                break;
        }

        fh.mask = (frame_data[1] & 0x80) != 0;
        //message client to server must mask.
        if (!fh.mask && is_server()) {
            return error(make_error_code(moon::error::ws_bad_unmasked_frame));
        }

        if (fh.mask) {
            header_size += 4;
        }

        // Check if we have enough data for the entire WebSocket frame header
        if (size < header_size) {
            return read_payload(header_size - size);
        }

        fh.op = static_cast<ws::opcode>(frame_data[0] & 0x0F);
        fh.fin = (frame_data[0] & 0x80) != 0;
        fh.rsv1 = (frame_data[0] & 0x40) != 0;
        fh.rsv2 = (frame_data[0] & 0x20) != 0;
        fh.rsv3 = (frame_data[0] & 0x10) != 0;

        switch (fh.op) {
            case ws::opcode::text:
            case ws::opcode::binary:
                if (fh.rsv1 || fh.rsv2 || fh.rsv3) {
                    // reserved bits not cleared
                    return error(make_error_code(moon::error::ws_bad_reserved_bits));
                }
                break;
            case ws::opcode::incomplete: {
                //not support continuation frame
                return error(make_error_code(moon::error::ws_bad_continuation));
            }
            default:
                if (!fh.fin) {
                    //not support fragmented control message
                    return error(make_error_code(moon::error::ws_bad_control_fragment));
                }
                if (fh.payload_len > PAYLOAD_MIN_LEN) {
                    // invalid length for control message
                    return error(make_error_code(moon::error::ws_bad_control_size));
                }
                if (fh.rsv1 || fh.rsv2 || fh.rsv3) {
                    // reserved bits not cleared
                    return error(make_error_code(moon::error::ws_bad_reserved_bits));
                }
                break;
        }

        uint64_t reallen = 0;
        switch (fh.payload_len) {
            case PAYLOAD_MID_LEN: {
                reallen = read_be_uint<uint16_t>(frame_data + 2);
                if (reallen < PAYLOAD_MID_LEN) {
                    // length not canonical
                    return error(make_error_code(moon::error::ws_bad_size));
                }
                break;
            }
            case PAYLOAD_MAX_LEN: {
                if (is_server()) {
                    //server not support PAYLOAD_MAX_LEN frame.
                    return error(make_error_code(moon::error::ws_bad_size));
                } else {
                    reallen = read_be_uint<uint64_t>(frame_data + 2);
                    if (reallen < 65536) {
                        // length not canonical
                        return error(make_error_code(moon::error::ws_bad_size));
                    }
                }
                break;
            }
            default:
                reallen = fh.payload_len;
                break;
        }

        if (fh.mask) {
            memcpy(
                fh.mask_key.data(),
                frame_data + (header_size - fh.mask_key.size()),
                fh.mask_key.size()
            );
        }

        cache_.consume(header_size);

        if (nullptr == data_) {
            data_ = buffer::make_unique(reallen + BUFFER_OPTION_CHEAP_PREPEND);
            data_->commit(BUFFER_OPTION_CHEAP_PREPEND);
        }

        // Calculate the difference between the cache size and the expected size
        ssize_t diff = static_cast<ssize_t>(cache_.size()) - static_cast<ssize_t>(reallen);
        // Determine the amount of data to consume from the cache
        // If the cache size is greater than or equal to the expected size, consume the expected size
        // Otherwise, consume the entire cache
        size_t consume_size = (diff >= 0 ? reallen : cache_.size());
        data_->write_back(cache_.data(), consume_size);
        cache_.consume(consume_size);

        if (diff >= 0) {
            return on_message(fh);
        }

        cache_.clear();

        asio::async_read(
            socket_,
            moon::streambuf(data_.get()),
            asio::transfer_exactly(static_cast<size_t>(-diff)),
            [this, self = shared_from_this(), fh](const asio::error_code& e, std::size_t) {
                if (!e) {
                    on_message(fh);
                    return;
                }
                error(e);
            }
        );
    }

    void on_message(ws::frame_header fh) {
        data_->seek(BUFFER_OPTION_CHEAP_PREPEND);

        if (fh.mask) {
            // unmask data:
            auto d = (uint8_t*)data_->data();
            size_t size = data_->size();
            for (size_t i = 0; i < size; ++i) {
                d[i] = d[i] ^ fh.mask_key[i % 4];
            }
        }

        if (fh.op == ws::opcode::close) {
            return error(
                make_error_code(moon::error::ws_closed),
                std::string { data_->data(), data_->size() }
            );
        }

        auto msg = message { std::move(data_) };
        msg.set_type(type_);

        switch (fh.op) {
            case ws::opcode::ping: {
                msg.set_receiver(static_cast<uint8_t>(socket_data_type::socket_ping));
                break;
            }
            case ws::opcode::pong: {
                msg.set_receiver(static_cast<uint8_t>(socket_data_type::socket_pong));
                break;
            }
            default:
                msg.set_receiver(static_cast<uint8_t>(socket_data_type::socket_recv));
                break;
        }

        handle_message(std::move(msg));

        handle_frame();
    }

    static ws::mask_key_type randkey() {
        ws::mask_key_type tmp {};
        char x = 0;
        for (auto& c: tmp) {
            c = static_cast<uint8_t>(std::rand() & 0xFF);
            x ^= c;
        }

        if (x == 0) {
            tmp[0] |= 1; // avoid 0
        }

        return tmp;
    }

    void prepare_send(size_t default_once_send_bytes) override {
        wqueue_.prepare_buffers(
            [this](const buffer_shr_ptr_t& elm) {
                if (!elm->has_bitmask(socket_send_mask::raw)) {
                    buffer payload = encode_frame(elm);
                    wqueue_.consume();
                    wqueue_.prepare_with_padding(
                        payload.data(),
                        payload.size(),
                        elm->data(),
                        elm->size()
                    );
                } else {
                    wqueue_.consume(elm->data(), elm->size());
                }
            },
            default_once_send_bytes
        );
    }

    buffer encode_frame(const buffer_shr_ptr_t& data) const {
        buffer payload { 16 };
        payload.commit(16);
        payload.seek(16);

        uint64_t size = data->size();

        if (!is_server()) {
            auto d = reinterpret_cast<uint8_t*>(data->data());
            auto mask = randkey();
            for (uint64_t i = 0; i < size; i++) {
                d[i] = d[i] ^ mask[i % mask.size()];
            }
            payload.write_front(mask.data(), mask.size());
        }

        uint8_t payload_len = 0;
        if (size <= PAYLOAD_MIN_LEN) {
            payload_len = static_cast<uint8_t>(size);
        } else if (size <= UINT16_MAX) {
            payload_len = static_cast<uint8_t>(PAYLOAD_MID_LEN);
            uint16_t n = (uint16_t)size;
            moon::host2net(n);
            payload.write_front(&n, 1);
        } else {
            payload_len = static_cast<uint8_t>(PAYLOAD_MAX_LEN);
            moon::host2net(size);
            payload.write_front(&size, 1);
        }

        //messages from the client must be masked
        if (!is_server()) {
            payload_len |= 0x80;
        }

        payload.write_front(&payload_len, 1);

        uint8_t opcode = FIN_FRAME_FLAG | static_cast<uint8_t>(ws::opcode::binary);

        if (data->has_bitmask(socket_send_mask::ws_text)) {
            opcode = FIN_FRAME_FLAG | static_cast<uint8_t>(ws::opcode::text);
        } else if (data->has_bitmask(socket_send_mask::ws_ping)) {
            opcode = FIN_FRAME_FLAG | static_cast<uint8_t>(ws::opcode::ping);
        } else if (data->has_bitmask(socket_send_mask::ws_pong)) {
            opcode = FIN_FRAME_FLAG | static_cast<uint8_t>(ws::opcode::pong);
        }

        payload.write_front(&opcode, 1);
        return payload;
    }

    static std::string hash_key(std::string_view seckey) {
        uint8_t keybuf[SEC_WEBSOCKET_KEY_LEN + WS_MAGICKEY.size()];
        std::memcpy(keybuf, seckey.data(), seckey.size());
        std::memcpy(keybuf + seckey.size(), WS_MAGICKEY.data(), WS_MAGICKEY.size());

        uint8_t shakey[sha1::sha1_context::digest_size] = { 0 };
        sha1::sha1_context ctx;
        sha1::init(ctx);
        sha1::update(ctx, keybuf, sizeof(keybuf));
        sha1::finish(ctx, shakey);
        return base64_encode(shakey, sizeof(shakey));
    }

    static std::string upgrade_response(std::string_view seckey, std::string_view wsprotocol) {
        std::string response;
        response.append("HTTP/1.1 101 Switching Protocols\r\n");
        response.append("Upgrade: WebSocket\r\n");
        response.append("Connection: Upgrade\r\n");
        response.append("Sec-WebSocket-Accept: ");
        response.append(hash_key(seckey));
        response.append(STR_CRLF);
        if (!wsprotocol.empty()) {
            response.append("Sec-WebSocket-Protocol: ");
            response.append(wsprotocol);
            response.append(STR_CRLF);
        }
        response.append(STR_CRLF);
        return response;
    }

private:
    buffer cache_;
    buffer_ptr_t data_;
};
} // namespace moon