#pragma once
#include "base_connection.hpp"
#include "common/http_utility.hpp"
#include "common/base64.hpp"
#include "common/byte_convert.hpp"
#include "common/sha1.hpp"

//https://developer.mozilla.org/en-US/docs/Web/API/WebSockets_API/Writing_WebSocket_servers

namespace moon
{
    namespace ws
    {
        enum close_code : std::uint16_t
        {
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

        enum class opcode : std::uint8_t
        {
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

        struct frame_header
        {
            bool fin;
            bool rsv1;
            bool rsv2;
            bool rsv3;
            bool mask;
            opcode op;//4bit
            uint8_t payload_len;//7 bit
            std::uint32_t key;
        };
    }

    class ws_connection : public base_connection
    {
    public:

        using base_connection_t = base_connection;

        static constexpr size_t HANDSHAKE_STREAMBUF_SIZE = 8192;

        static constexpr size_t DEFAULT_RECV_BUFFER_SIZE = 128;

        static constexpr size_t SEC_WEBSOCKET_KEY_LEN = 24;

        static constexpr size_t PAYLOAD_MIN_LEN = 125;
        static constexpr size_t PAYLOAD_MID_LEN = 126;
        static constexpr size_t PAYLOAD_MAX_LEN = 127;
        static constexpr size_t FIN_FRAME_FLAG = 0x80;// 1 0 0 0 0 0 0 0

        static constexpr const std::string_view WEBSOCKET = "websocket"sv;
        static constexpr const std::string_view UPGRADE = "upgrade"sv;
        static constexpr const std::string_view WS_MAGICKEY = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"sv;

        template <typename... Args, std::enable_if_t<!std::disjunction_v<std::is_same<std::decay_t<Args>, ws_connection>...>, int> = 0>
        explicit ws_connection(Args&&... args)
            :base_connection_t(std::forward<Args>(args)...)
        {
        }

        void start(role r) override
        {
            base_connection_t::start(r);
            if (role::server == r)
            {
                read_header();
            }
            else
            {
                read_some();
            }
        }

        bool send(buffer_shr_ptr_t&& data, socket_send_mask mask) override
        {
            auto payload = encode_frame(data, mask);
            base_connection_t::send(std::move(payload), mask);
            return base_connection_t::send(std::move(data), mask);
        }

    protected:
        asio::mutable_buffer prepare_buffer(size_t size)
        {
            if (nullptr == recv_buf_){
                recv_buf_ = buffer::make_unique(size + BUFFER_OPTION_CHEAP_PREPEND);
                recv_buf_->commit(BUFFER_OPTION_CHEAP_PREPEND);
            }
            auto space = recv_buf_->prepare(size);
            return asio::buffer(space.first, space.second);
        }

        void read_header()
        {
            auto sbuf = std::make_shared<asio::streambuf>(HANDSHAKE_STREAMBUF_SIZE);
            asio::async_read_until(socket_, *sbuf, STR_DCRLF,
                    [this, self = shared_from_this(), sbuf](const asio::error_code& e, std::size_t bytes_transferred)
            {
                if (e)
                {
                    error(e);
                    return;
                }

                size_t num_additional_bytes = sbuf->size() - bytes_transferred;
                auto ec = handshake(sbuf, bytes_transferred);
                sbuf->consume(bytes_transferred);
                if (!ec)
                {
                    prepare_buffer(std::max(DEFAULT_RECV_BUFFER_SIZE, num_additional_bytes));
                    if (num_additional_bytes > 0)
                    {
                        auto data = reinterpret_cast<const char*>(sbuf->data().data());
                        recv_buf_->write_back(data, num_additional_bytes);
                        if (!handle_frame())
                        {
                            return;
                        }
                    }
                    read_some();
                }
                else
                {
                    // client close, or server check read timeout.
                    send_response("HTTP/1.1 400 Bad Request\r\n\r\n", true);
                }
            });
        }


        void read_some()
        {
            socket_.async_read_some(prepare_buffer(DEFAULT_RECV_BUFFER_SIZE),
                    [this, self = shared_from_this()](const asio::error_code& e, std::size_t bytes_transferred)
            {
                if (e)
                {
                    error(e);
                    return;
                }

                if (bytes_transferred == 0)
                {
                    read_some();
                    return;
                }

                recv_buf_->commit(static_cast<int>(bytes_transferred));
 
                if (!handle_frame())
                {
                    return;
                }

                read_some();
            });
        }

        std::error_code handshake(const std::shared_ptr<asio::streambuf>& buf, size_t n)
        {
            std::string_view method;
            std::string_view ignore;
            std::string_view version;
            http::case_insensitive_multimap_view header;
            if(!http::request_parser::parse(std::string_view{ reinterpret_cast<const char*>(buf->data().data()), n }
                , method
                , ignore
                , ignore
                , version
                , header))
            {
                return make_error_code(moon::error::ws_bad_http_header);
            }

            if (version<"1.0" || version>"1.1")
            {
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

            if (base64_decode(std::string{ sec_ws_key.data(), sec_ws_key.size() }).size() != 16)
                return make_error_code(moon::error::ws_bad_sec_key);

            std::string_view sec_ws_version;
            if (!moon::try_get_value(header, "sec-websocket-version"sv, sec_ws_version))
                return make_error_code(moon::error::ws_no_sec_version);

            if (sec_ws_version != "13"sv)
            {
                return make_error_code(moon::error::ws_bad_sec_version);
            }

            std::string_view protocol;
            moon::try_get_value(header, "sec-websocket-protocol"sv, protocol);

            auto answer = upgrade_response(sec_ws_key, protocol);
            send_response(answer);
            auto msg = message{n};
            msg.write_data(std::string_view{ reinterpret_cast<const char*>(buf->data().data()), n});
            msg.set_receiver(static_cast<uint8_t>(socket_data_type::socket_accept));
            handle_message(std::move(msg));
            return std::error_code();
        }

        void send_response(const std::string& s, bool will_close = false)
        {
            auto buf = std::make_shared<buffer>(s.size());
            buf->write_back(s.data(), s.size());
            base_connection::send(std::move(buf), will_close?socket_send_mask::close:socket_send_mask::none);
        }

        bool handle_frame()
        {
            auto ec = decode_frame();
            if (ec)
            {
                if (ec == moon::error::ws_closed && recv_buf_->size()>1)
                {
                    uint16_t code = *(const uint16_t*)recv_buf_->data();
                    std::string reason;
                    reason.append(std::to_string(code));
                    recv_buf_->consume(2);
                    if (recv_buf_->size() != 0)
                    {
                        reason.append(":");
                        reason.append(recv_buf_->data(), recv_buf_->size());
                    }
                    error(ec, reason);
                }
                else
                {
                    error(ec);
                }
                return false;
            }
            return true;
        }

        std::error_code decode_frame()
        {
            const uint8_t* tmp = (const uint8_t*)(recv_buf_->data());
            size_t size = recv_buf_->size();

            if (size < 3)
            {
                return std::error_code();
            }

            size_t need = 2;
            ws::frame_header fh{};

            fh.payload_len = tmp[1] & 0x7F;
            switch (fh.payload_len)
            {
            case PAYLOAD_MID_LEN: need += 2; break;
            case PAYLOAD_MAX_LEN: need += 8; break;
            default:
                break;
            }

            fh.mask = (tmp[1] & 0x80) != 0;
            //message client to server must mask.
            if (!fh.mask && role_ == role::server)
            {
                return make_error_code(moon::error::ws_bad_unmasked_frame);
            }

            if (fh.mask)
            {
                need += 4;
            }

            //need more data
            if (size < need)
            {
                prepare_buffer(need);
                return std::error_code();
            }

            fh.op = static_cast<ws::opcode>(tmp[0] & 0x0F);
            fh.fin = (tmp[0] & 0x80) != 0;
            fh.rsv1 = (tmp[0] & 0x40) != 0;
            fh.rsv2 = (tmp[0] & 0x20) != 0;
            fh.rsv3 = (tmp[0] & 0x10) != 0;

            switch (fh.op)
            {
            case ws::opcode::text:
            case ws::opcode::binary:
                if (fh.rsv1 || fh.rsv2 || fh.rsv3)
                {
                    // reserved bits not cleared
                    return make_error_code(moon::error::ws_bad_reserved_bits);
                }
                break;
            case ws::opcode::incomplete:
                {
                    //not support continuation frame
                    return make_error_code(moon::error::ws_bad_continuation);
                }
            default:
                if (!fh.fin)
                {
                    //not support fragmented control message
                    return make_error_code(moon::error::ws_bad_control_fragment);
                }
                if (fh.payload_len > PAYLOAD_MIN_LEN)
                {
                    // invalid length for control message
                    return make_error_code(moon::error::ws_bad_control_size);
                }
                if (fh.rsv1 || fh.rsv2 || fh.rsv3)
                {
                    // reserved bits not cleared
                    return make_error_code(moon::error::ws_bad_reserved_bits);
                }
                break;
            }

            uint64_t reallen = 0;
            switch (fh.payload_len)
            {
            case PAYLOAD_MID_LEN:
            {
                auto n = *(uint16_t*)(&tmp[2]);
                moon::net2host(n);
                reallen = n;
                if (reallen < PAYLOAD_MID_LEN)
                {
                    // length not canonical
                    return make_error_code(moon::error::ws_bad_size);
                }
                break;
            }
            case PAYLOAD_MAX_LEN:
            {
                if (role_ == role::server)
                {
                    //server not support PAYLOAD_MAX_LEN frame.
                    return make_error_code(moon::error::ws_bad_size);
                }
                else
                {
                    reallen = *(uint64_t*)(&tmp[2]);
                    moon::net2host(reallen);
                    if (reallen < 65536)
                    {
                        // length not canonical
                        return make_error_code(moon::error::ws_bad_size);
                    }
                }
                break;
            }
            default:
                reallen = fh.payload_len;
                break;
            }

            if (size < need + reallen)
            {
                //need more data
                prepare_buffer(static_cast<size_t>(need + reallen - size));
                return std::error_code();
            }

            if (fh.mask)
            {
                fh.key = *((uint32_t*)(tmp + (need - sizeof(fh.key))));
                // unmask data:
                uint8_t* d = (uint8_t*)(tmp + need);
                for (uint64_t i = 0; i < reallen; i++)
                {
                    d[i] = d[i] ^ ((uint8_t*)(&fh.key))[i % 4];
                }
            }

            recv_buf_->consume(need);
            if (fh.op == ws::opcode::close)
            {
                return make_error_code(moon::error::ws_closed);
            }
            message msg = message::with_empty();
            if (recv_buf_->size()==reallen)
            {
                recv_buf_->seek(BUFFER_OPTION_CHEAP_PREPEND);
                msg = message{ std::move(recv_buf_) };
            }
            else
            {
                msg = message{ reallen + BUFFER_OPTION_CHEAP_PREPEND };
                auto b = msg.as_buffer();
                b->commit(BUFFER_OPTION_CHEAP_PREPEND);
                b->write_back(recv_buf_->data(), reallen);
                b->seek(BUFFER_OPTION_CHEAP_PREPEND);
                recv_buf_->consume(reallen);
            }

            switch (fh.op)
            {
            case ws::opcode::ping:
            {
                msg.set_receiver(static_cast<uint8_t>(socket_data_type::socket_ping));
                break;
            }
            case ws::opcode::pong:
            {
                msg.set_receiver(static_cast<uint8_t>(socket_data_type::socket_pong));
                break;
            }
            default:
                msg.set_receiver(static_cast<uint8_t>(socket_data_type::socket_recv));
                break;
            }

            handle_message(std::move(msg));

            if (nullptr == recv_buf_) {
                return std::error_code();
            }

            return decode_frame();
        }

        static const std::array<uint8_t, 4> randkey()
        {
            std::array<uint8_t, 4> tmp;
            char x = 0;
            for (auto& c : tmp) {
                c = static_cast<uint8_t>(std::rand() & 0xFF);
                x ^= c;
            }

            if (x == 0)
            {
                tmp[0] |= 1;// avoid 0
            }

            return tmp;
        }

        buffer_shr_ptr_t encode_frame(const buffer_shr_ptr_t& data, socket_send_mask send_mask) const
        {
            buffer_shr_ptr_t payload = buffer::make_shared(16);
            payload->commit(16);
            payload->seek(16);

            uint64_t size = data->size();

            if (role_ == role::client)
            {
                auto d = reinterpret_cast<uint8_t*>(data->data());
                auto mask = randkey();
                for (uint64_t i = 0; i < size; i++)
                {
                    d[i] = d[i] ^ mask[i % mask.size()];
                }
                payload->write_front(mask.data(), mask.size());
            }

            uint8_t payload_len = 0;
            if (size <= PAYLOAD_MIN_LEN)
            {
                payload_len = static_cast<uint8_t>(size);
            }
            else if (size <= UINT16_MAX)
            {
                payload_len = static_cast<uint8_t>(PAYLOAD_MID_LEN);
                uint16_t n = (uint16_t)size;
                moon::host2net(n);
                payload->write_front(&n, 1);
            }
            else
            {
                payload_len = static_cast<uint8_t>(PAYLOAD_MAX_LEN);
                moon::host2net(size);
                payload->write_front(&size, 1);
            }

            //messages from the client must be masked
            if (role_ == role::client)
            {
                payload_len |= 0x80;
            }

            payload->write_front(&payload_len, 1);

            uint8_t opcode = FIN_FRAME_FLAG | static_cast<uint8_t>(ws::opcode::binary);

            if (enum_has_any_bitmask(send_mask, socket_send_mask::ws_text))
            {
                opcode = FIN_FRAME_FLAG | static_cast<uint8_t>(ws::opcode::text);
            }
            else if (enum_has_any_bitmask(send_mask, socket_send_mask::ws_ping))
            {
                opcode = FIN_FRAME_FLAG | static_cast<uint8_t>(ws::opcode::ping);
            }
            else if (enum_has_any_bitmask(send_mask, socket_send_mask::ws_pong))
            {
                opcode = FIN_FRAME_FLAG | static_cast<uint8_t>(ws::opcode::pong);
            }

            payload->write_front(&opcode, 1);
            return payload;
        }

        static std::string hash_key(std::string_view seckey)
        {
            uint8_t keybuf[SEC_WEBSOCKET_KEY_LEN+ WS_MAGICKEY.size()];
            std::memcpy(keybuf, seckey.data(), seckey.size());
            std::memcpy(keybuf + seckey.size(), WS_MAGICKEY.data(), WS_MAGICKEY.size());

            uint8_t shakey[sha1::sha1_context::digest_size] = { 0 };
            sha1::sha1_context ctx;
            sha1::init(ctx);
            sha1::update(ctx, keybuf, sizeof(keybuf));
            sha1::finish(ctx, shakey);
            return base64_encode(shakey, sizeof(shakey));
        }

        static std::string upgrade_response(std::string_view seckey, std::string_view wsprotocol)
        {
            std::string response;
            response.append("HTTP/1.1 101 Switching Protocols\r\n");
            response.append("Upgrade: WebSocket\r\n");
            response.append("Connection: Upgrade\r\n");
            response.append("Sec-WebSocket-Accept: ");
            response.append(hash_key(seckey));
            response.append(STR_CRLF);
            if (!wsprotocol.empty())
            {
                response.append("Sec-WebSocket-Protocol: ");
                response.append(wsprotocol);
                response.append(STR_CRLF);
            }
            response.append(STR_CRLF);
            return response;
        }

    protected:
        buffer_ptr_t recv_buf_;
    };
}