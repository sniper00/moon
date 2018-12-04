#pragma once
#include "base_connection.hpp"
#include "common/http_util.hpp"
#include "common/base64.hpp"
#include "common/byte_convert.hpp"
#include "common/sha1.hpp"

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
            std::uint64_t len;
            std::uint32_t key;
            opcode op;
            bool fin : 1;
            bool mask : 1;
            bool rsv1 : 1;
            bool rsv2 : 1;
            bool rsv3 : 1;
        };
    }

    class ws_connection : public base_connection
    {
    public:
        using base_connection_t = base_connection;

        static constexpr size_t HANDSHAKE_STREAMBUF_SIZE = 8192;

        static constexpr size_t PAYLOAD_MIN_LEN = 125;
        static constexpr size_t PAYLOAD_MID_LEN = 126;
        static constexpr size_t PAYLOAD_MAX_LEN = 127;

        static constexpr const string_view_t WEBSOCKET = "websocket"sv;
        static constexpr const string_view_t UPGRADE = "upgrade"sv;
        static constexpr const string_view_t WS_MAGICKEY = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"sv;

        template <typename... Args>
        explicit ws_connection(Args&&... args)
            :base_connection_t(std::forward<Args>(args)...)
            , header_delim_(STR_DCRLF.data(), STR_DCRLF.size())
            , buffer_()
        {
        }

        void start(bool accepted, int32_t responseid = 0) override
        {
            base_connection_t::start(accepted, responseid);
            if (socket_.is_open())
            {
                response_msg_ = message::create(1024);
                read_header();
            }
        }

        bool send(const buffer_ptr_t & data) override
        {
            encode_frame(data);
            return base_connection_t::send(data);
        }

    protected:
        void read_header()
        {
            auto sbuf = std::make_shared<asio::streambuf>(HANDSHAKE_STREAMBUF_SIZE);
            asio::async_read_until(socket_, *sbuf, header_delim_,
                make_custom_alloc_handler(read_allocator_,
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

                last_recv_time_ = std::time(nullptr);

                size_t num_additional_bytes = sbuf->size() - bytes_transferred;
                if (handshake(sbuf))
                {
                    if (num_additional_bytes > 0)
                    {
                        auto data = asio::buffer_cast<const char*>(sbuf->data());
                        cache_.write_back(data, 0, num_additional_bytes);
                        if (!handle_frame())
                        {
                            return;
                        }
                    }
                    read_some();
                }
                else
                {
                    printf("websocket handshake failed\n");
                    send_response("HTTP/1.1 400 Bad Request\r\n\r\n", true);
                }
            }));
        }

        void read_some()
        {
            socket_.async_read_some(asio::buffer(buffer_),
                make_custom_alloc_handler(read_allocator_,
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

                last_recv_time_ = std::time(nullptr);
                cache_.write_back(buffer_.data(), 0, bytes_transferred);

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

            http::request_parser request;
            int consumed = request.parse(string_view_t{ asio::buffer_cast<const char*>(buf->data()), buf->size() });
            if (-1 == consumed)
            {
                return false;
            }
            buf->consume(consumed);

            if (request.method != "GET"sv)
                return false;

            auto h = request.header("connection"sv);
            if (h.empty())
                return false;

            auto u = request.header("upgrade"sv);
            if (u.empty())
                return false;

            if (!iequal_string(h, UPGRADE))
                return false;

            if (!iequal_string(u, WEBSOCKET))
                return false;

            auto sec_ws_key_ = request.header("sec-websocket-key"sv);
            if (sec_ws_key_.empty() || sec_ws_key_.size() != 24)
                return false;

            handshaked_ = true;
            auto answer = upgrade_response(sec_ws_key_, request.header("Sec-WebSocket-Protocol"sv));
            send_response(answer);
            auto msg = message::create();
            msg->write_string(remote_addr_);
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
                error(asio::error_code(), int(close_code), std::string(cache_.data(), cache_.size()).data());
                base_connection::close();
                return false;
            }
            return true;
        }

        ws::close_code decode_frame()
        {
            const uint8_t* tmp = (const uint8_t*)(cache_.data());
            size_t len = cache_.size();

            if (len < 3)
            {
                return ws::close_code::none;
            }

            size_t need = 2;
            ws::frame_header fh;

            fh.len = tmp[1] & 0x7F;
            switch (fh.len)
            {
            case 126: need += 2; break;
            case 127: need += 8; break;
            default:
                break;
            }

            fh.mask = (tmp[1] & 0x80) != 0;
            //server must masked.
            if (!fh.mask)
            {
                return ws::close_code::protocol_error;
            }

            if (fh.mask)
            {
                need += 4;
            }

            //need more data
            if (len < need)
            {
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
            case ws::opcode::incomplete:
                if (fh.rsv1 || fh.rsv2 || fh.rsv3)
                {
                    // reserved bits not cleared
                    return ws::close_code::protocol_error;
                }
                break;
            default:
                if (!fh.fin)
                {
                    // fragmented control message
                    return ws::close_code::protocol_error;
                }
                if (fh.len > 125)
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

            switch (fh.len)
            {
            case 126:
            {
                auto reallen = *(uint16_t*)(&tmp[2]);
                moon::net2host(reallen);
                fh.len = reallen;
                if (fh.len < 126)
                {
                    // length not canonical
                    return ws::close_code::protocol_error;
                }
                break;
            }
            case 127:
            {
                auto reallen = *(uint64_t*)(&tmp[2]);
                moon::net2host(reallen);
                fh.len = reallen;
                if (fh.len < 65536)
                {
                    // length not canonical
                    return ws::close_code::protocol_error;
                }
                break;
            }
            default:
                break;
            }

            if (len < need + fh.len)
            {
                //need more data
                return ws::close_code::none;
            }

            if (fh.mask)
            {
                fh.key = *((int32_t*)(tmp + (need - sizeof(fh.key))));
                // unmask data:
                uint8_t* d = (uint8_t*)(tmp + need);
                for (uint64_t i = 0; i < fh.len; i++)
                {
                    d[i] = d[i] ^ ((uint8_t*)(&fh.key))[i % 4];
                }
            }

            if (fh.op != ws::opcode::close &&  fh.fin)
            {
                message_ptr_t msg = message::create(static_cast<size_t>(fh.len));
                msg->get_buffer()->write_back((tmp + need), 0, static_cast<size_t>(fh.len));
                cache_.seek(int(need + fh.len), buffer::Current);
                msg->set_subtype(static_cast<uint8_t>(socket_data_type::socket_recv));
                handle_message(std::move(msg));
            }

            if (fh.op == ws::opcode::close)
            {
                //may have error msg
                cache_.seek(static_cast<int>(need), buffer::Current);
                return ws::close_code::normal;
            }

            return ws::close_code::none;
        }

        void encode_frame(const buffer_ptr_t& data)
        {
            uint64_t size = data->size();
            uint8_t m = 0x81;
            if (size <= 125)
            {
                data->write_front((uint8_t*)&size, 0, 1);
            }
            else if (size <= UINT16_MAX)
            {
                uint8_t tmp = 126;
                uint16_t n = (uint16_t)size;
                moon::host2net(n);
                data->write_front((uint16_t*)&n, 0, 1);
                data->write_front(&tmp, 0, 1);
            }
            else
            {
                uint8_t tmp = 127;
                moon::host2net(size);
                data->write_back(&size, 0, 1);
                data->write_back(&tmp, 0, 1);
            }

            data->write_front(&m, 0, 1);
        }

        std::string upgrade_response(string_view_t seckey, string_view_t wsprotocol)
        {
            uint8_t keybuf[60];
            std::memcpy(keybuf, seckey.data(), seckey.size());
            std::memcpy(keybuf + seckey.size(), WS_MAGICKEY.data(), WS_MAGICKEY.size());

            uint8_t shakey[sha1::sha1_context::digest_size] = { 0 };
            sha1::sha1_context ctx;
            sha1::init(ctx);
            sha1::update(ctx, keybuf, sizeof(keybuf));
            sha1::finish(ctx, shakey);
            std::string sha1str = base64_encode(shakey, sizeof(shakey));

            std::string response;
            response.append("HTTP/1.1 101 Switching Protocols\r\n");
            response.append("Upgrade: WebSocket\r\n");
            response.append("Connection: Upgrade\r\n");
            response.append("Sec-WebSocket-Accept: ");
            response.append(sha1str);
            response.append(header_delim_);
            if (!wsprotocol.empty())
            {
                response.append("Sec-WebSocket-Protocol: ");
                response.append(wsprotocol.data(), wsprotocol.size());
                response.append(header_delim_);
            }
            return response;
        }

    protected:
        bool handshaked_ = false;
        const std::string header_delim_;
        message_ptr_t  response_msg_;
        std::array<uint8_t, 1024> buffer_;
        buffer cache_;
    };
}