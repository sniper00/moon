#pragma once
#include "base_connection.hpp"
#include "common/http_util.hpp"
#include "common/base64.hpp"
#include "common/byte_convert.hpp"
#include "common/sha1.hpp"
#include "common/random.hpp"

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
         enum class  role
        {
            none,
            client,
            server
        };

        using base_connection_t = base_connection;

        static constexpr size_t HANDSHAKE_STREAMBUF_SIZE = 8192;

        static constexpr size_t DEFAULT_RECV_BUFFER_SIZE = buffer::STACK_CAPACITY - BUFFER_HEAD_RESERVED;

        static constexpr size_t SEC_WEBSOCKET_KEY_LEN = 24;

        static constexpr size_t PAYLOAD_MIN_LEN = 125;
        static constexpr size_t PAYLOAD_MID_LEN = 126;
        static constexpr size_t PAYLOAD_MAX_LEN = 127;
        static constexpr size_t FIN_FRAME_FLAG = 0x80;// 1 0 0 0 0 0 0 0

        static constexpr const string_view_t WEBSOCKET = "websocket"sv;
        static constexpr const string_view_t UPGRADE = "upgrade"sv;
        static constexpr const string_view_t WS_MAGICKEY = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"sv;

        template <typename... Args>
        explicit ws_connection(Args&&... args)
            :base_connection_t(std::forward<Args>(args)...)
        {
        }

        void start(bool accepted) override
        {
            role_ = accepted ? role::server : role::client;
            base_connection_t::start(accepted);
            if (accepted)
            {
                read_header();
            }
            else
            {
                request_handshake();
            }
        }

        bool send(const buffer_ptr_t & data) override
        {
            if (!handshaked_) return false;
            encode_frame(data);
            return base_connection_t::send(data);
        }

    protected:
        void check_recv_buffer(size_t size)
        {
            if (nullptr == recv_buf_)
            {
                recv_buf_ = message::create_buffer(size);
            }
            else
            {
                recv_buf_->check_space(size);
            }
        }

        void read_header()
        {
            auto sbuf = std::make_shared<asio::streambuf>(HANDSHAKE_STREAMBUF_SIZE);
            asio::async_read_until(socket_, *sbuf, STR_DCRLF,
                make_custom_alloc_handler(rallocator_,
                    [this, self = shared_from_this(), sbuf](const asio::error_code& e, std::size_t bytes_transferred)
            {
                if (e)
                {
                    error(e, int(logic_error_));
                    return;
                }

                if (bytes_transferred == 0)
                {
                    read_header();
                    return;
                }

                recvtime_ = now();

                size_t num_additional_bytes = sbuf->size() - bytes_transferred;
                if (handshake(sbuf))
                {
                    check_recv_buffer(DEFAULT_RECV_BUFFER_SIZE);
                    if (num_additional_bytes > 0)
                    {
                        auto data = reinterpret_cast<const char*>(sbuf->data().data());
                        recv_buf_->write_back(data, 0, num_additional_bytes);
                        if (!handle_frame())
                        {
                            return;
                        }
                    }
                    read_some();
                }
                else
                {
                    send_response("HTTP/1.1 400 Bad Request\r\n\r\n", true);
                }
            }));
        }

        void request_handshake()
        {
            std::string key(base64_encode((moon::randkey(16)), 16));

            std::shared_ptr<std::string> str = std::make_shared<std::string>();
            str->append("GET / HTTP/1.1\r\n");
            str->append("Upgrade: WebSocket\r\n");
            str->append("Connection: Upgrade\r\n");
            str->append("Sec-WebSocket-Version: 13\r\n");
            str->append(moon::format("Sec-WebSocket-Key: %s\r\n\r\n",key.data()));

            asio::async_write(socket_, asio::buffer(str->data(), str->size()), [this, self = shared_from_this(), str, key = std::move(key)](const asio::error_code& e, std::size_t) {
                if (!e)
                {
                    auto sbuf = std::make_shared<asio::streambuf>(HANDSHAKE_STREAMBUF_SIZE);
                    asio::async_read_until(socket_, *sbuf, STR_DCRLF,
                        [this, self = shared_from_this(), sbuf, key = std::move(key)](const asio::error_code& e, std::size_t bytes_transferred)
                    {
                        if (e || bytes_transferred == 0)
                        {
                            error(e, int(logic_error_));
                            return;
                        }

                        std::string_view status_code;
                        std::string_view ignore;
                        http::case_insensitive_multimap_view header;
                        if (!http::response_parser::parse(string_view_t{ reinterpret_cast<const char*>(sbuf->data().data()), sbuf->size() }
                            , ignore
                            , status_code
                            , header))
                        {
                            error(e, int(logic_error_));
                            return;
                        }

                        if (status_code.substr(0, 3) != "101"sv)
                        {
                            error(e, int(logic_error_));
                            return;
                        }

                        std::string_view accept_key;
                        if (!moon::try_get_value(header, "sec-websocket-accept"sv, accept_key))
                        {
                            error(e, int(logic_error_));
                            return;
                        }

                        if (accept_key != hash_key(key))
                        {
                            error(e, int(logic_error_));
                            return;
                        }

                        handshaked_ = true;
                        auto msg = message::create();
                        msg->write_string(address());
                        msg->set_subtype(static_cast<uint8_t>(socket_data_type::socket_connect));
                        handle_message(std::move(msg));
                        check_recv_buffer(DEFAULT_RECV_BUFFER_SIZE);
                        read_some();
                    });
                }
                else
                {
                    error(e, int(logic_error_));
                }
            });
        }

        void read_some()
        {
            socket_.async_read_some(asio::buffer(recv_buf_->data() + recv_buf_->size(), recv_buf_->writeablesize()),
                make_custom_alloc_handler(rallocator_,
                    [this, self = shared_from_this()](const asio::error_code& e, std::size_t bytes_transferred)
            {
                if (e)
                {
                    error(e, int(logic_error_));
                    return;
                }

                if (bytes_transferred == 0)
                {
                    read_some();
                    return;
                }

                recvtime_ = now();
                recv_buf_->offset_writepos(static_cast<int>(bytes_transferred));
 
                if (!handle_frame())
                {
                    return;
                }

                read_some();
            }));
        }

        bool handshake(const std::shared_ptr<asio::streambuf>& buf)
        {
            if (handshaked_)
            {
                return false;
            }

            std::string_view method;
            std::string_view ignore;
            http::case_insensitive_multimap_view header;
            int consumed = http::request_parser::parse(string_view_t{ reinterpret_cast<const char*>(buf->data().data()), buf->size() }
                , method
                , ignore
                , ignore
                , ignore
                , header);
            if (-1 == consumed)
            {
                return false;
            }
            buf->consume(consumed);

            if (method != "GET"sv)
                return false;

            std::string_view h;
            if (!moon::try_get_value(header, "connection"sv, h))
                return false;

            std::string_view u;
            if (!moon::try_get_value(header, "upgrade"sv, u))
                return false;

            if (!iequal_string(h, UPGRADE))
                return false;

            if (!iequal_string(u, WEBSOCKET))
                return false;

            std::string_view sec_ws_key_;
            if (!moon::try_get_value(header, "sec-websocket-key"sv, sec_ws_key_))
                return false;

            if (sec_ws_key_.size() != SEC_WEBSOCKET_KEY_LEN)
                return false;

            std::string_view protocol;
            moon::try_get_value(header, "sec-websocket-protocol"sv, protocol);

            handshaked_ = true;
            auto answer = upgrade_response(sec_ws_key_, protocol);
            send_response(answer);
            auto msg = message::create();
            msg->write_string(address());
            msg->set_subtype(static_cast<uint8_t>(socket_data_type::socket_accept));
            handle_message(std::move(msg));
            return true;
        }

        void send_response(const std::string& s, bool bclose = false)
        {
            auto buf = message::create_buffer();
            buf->write_back(s.data(), 0, s.size());
            if (bclose)
            {
                buf->set_flag(buffer_flag::close);
            }
            base_connection::send(buf);
        }

        bool handle_frame()
        {
            auto close_code = decode_frame();
            if (ws::close_code::none != close_code)
            {
                error(asio::error_code(), int(close_code), std::string(recv_buf_->data(), recv_buf_->size()).data());
                base_connection::close();
                return false;
            }
            return true;
        }

        ws::close_code decode_frame()
        {
            const uint8_t* tmp = (const uint8_t*)(recv_buf_->data());
            size_t size = recv_buf_->size();

            if (size < 3)
            {
                check_recv_buffer(10);
                return ws::close_code::none;
            }

            size_t need = 2;
            ws::frame_header fh;

            fh.payload_len = tmp[1] & 0x7F;
            switch (fh.payload_len)
            {
            case PAYLOAD_MID_LEN: need += 2; break;
            case PAYLOAD_MAX_LEN: need += 8; break;
            default:
                break;
            }

            fh.mask = (tmp[1] & 0x80) != 0;
            //message clinet to server must masked.
            if (!fh.mask && role_ == role::server)
            {
                return ws::close_code::protocol_error;
            }

            if (fh.mask)
            {
                need += 4;
            }

            //need more data
            if (size < need)
            {
                check_recv_buffer(need);
                return ws::close_code::none;
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
                    return ws::close_code::protocol_error;
                }
                break;
            case ws::opcode::incomplete:
                {
                    //not support continuation frame
                    return ws::close_code::protocol_error;
                    break;
                }
            default:
                if (!fh.fin)
                {
                    // fragmented control message
                    return ws::close_code::protocol_error;
                }
                if (fh.payload_len > PAYLOAD_MIN_LEN)
                {
                    // invalid length for control message
                    return ws::close_code::protocol_error;
                }
                if (fh.rsv1 || fh.rsv2 || fh.rsv3)
                {
                    // reserved bits not cleared
                    return ws::close_code::protocol_error;
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
                    return ws::close_code::protocol_error;
                }
                break;
            }
            case PAYLOAD_MAX_LEN:
            {
                reallen = *(uint64_t*)(&tmp[2]);
                moon::net2host(reallen);
                if (reallen < 65536)
                {
                    // length not canonical
                    return ws::close_code::protocol_error;
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
                check_recv_buffer(static_cast<size_t>(need + reallen - size));
                return ws::close_code::none;
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

            if (fh.op != ws::opcode::close &&  fh.fin)
            {
                recv_buf_->seek(static_cast<int>(need), buffer::Current);
                message_ptr_t msg = message::create(std::move(recv_buf_));
                msg->set_subtype(static_cast<uint8_t>(socket_data_type::socket_recv));
                handle_message(std::move(msg));
            }

            if (fh.op == ws::opcode::close)
            {
                //may have error msg
                recv_buf_->seek(static_cast<int>(need), buffer::Current);
                return ws::close_code::normal;
            }

            check_recv_buffer(DEFAULT_RECV_BUFFER_SIZE);
            return ws::close_code::none;
        }

        void encode_frame(const buffer_ptr_t& data)
        {
            uint64_t size = data->size();

            if (role_ == role::client)
            {
                auto d = reinterpret_cast<unsigned char*>(data->data());
                const uint8_t* mask = randkey(4);
                for (uint64_t i = 0; i < size; i++)
                {
                    d[i] = d[i] ^ mask[i % 4];
                }
                data->write_front(mask, 0, 4);
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
                data->write_front(&n, 0, 1);
            }
            else
            {
                payload_len = static_cast<uint8_t>(PAYLOAD_MAX_LEN);
                moon::host2net(size);
                data->write_front(&size, 0, 1);
            }

            //messages from the client must be masked
            if (role_ == role::client)
            {
                payload_len |= 0x80;
            }

            data->write_front(&payload_len, 0, 1);

            uint8_t opcode = FIN_FRAME_FLAG | static_cast<uint8_t>(ws::opcode::binary);
            if (data->has_flag(buffer_flag::ws_text))
            {
                opcode = FIN_FRAME_FLAG | static_cast<uint8_t>(ws::opcode::text);
            }

            data->write_front(&opcode, 0, 1);
        }

        std::string hash_key(std::string_view seckey)
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

        std::string upgrade_response(string_view_t seckey, string_view_t wsprotocol)
        {
            std::string response;
            response.append("HTTP/1.1 101 Switching Protocols\r\n");
            response.append("Upgrade: WebSocket\r\n");
            response.append("Connection: Upgrade\r\n");
            response.append("Sec-WebSocket-Accept: ");
            response.append(hash_key(seckey));
            response.append(STR_DCRLF.data(), STR_DCRLF.size());
            if (!wsprotocol.empty())
            {
                response.append("Sec-WebSocket-Protocol: ");
                response.append(wsprotocol.data(), wsprotocol.size());
                response.append(STR_DCRLF.data(), STR_DCRLF.size());
            }
            return response;
        }

    protected:
        bool handshaked_ = false;
        role role_ = role::none;
        buffer_ptr_t recv_buf_;
    };
}