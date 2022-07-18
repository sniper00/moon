//#define ASIO_ENABLE_HANDLER_TRACKING

#include <fstream>
#include <array>
#include <iostream>
#include <memory>
#include <filesystem>
#include <asio.hpp>
#include <asio/experimental/as_tuple.hpp>
#include <asio/experimental/awaitable_operators.hpp>
#include "json.hpp"

#if _WIN32
#    define __FILENAME__ (strrchr(__FILE__, '\\') ? (strrchr(__FILE__, '\\') + 1) : __FILE__)
#else
#    define __FILENAME__ (strrchr(__FILE__, '/') ? (strrchr(__FILE__, '/') + 1) : __FILE__)
#endif

using asio::awaitable;
using asio::buffer;
using asio::co_spawn;
using asio::detached;
using asio::ip::tcp;
namespace this_coro = asio::this_coro;
using namespace asio::experimental::awaitable_operators;
using std::chrono::steady_clock;
using namespace std::literals::chrono_literals;
using namespace std::string_view_literals;

constexpr auto use_nothrow_awaitable = asio::experimental::as_tuple(asio::use_awaitable);

using target_host_t = std::pair<asio::ip::tcp::resolver::results_type,std::string>;
using host_map_t = std::unordered_map<int, target_host_t>;

inline uint16_t network_to_host_short(uint16_t value)
{
    unsigned char* value_p = reinterpret_cast<unsigned char*>(&value);
    uint16_t result = (static_cast<uint16_t>(value_p[0]) << 8) | static_cast<uint16_t>(value_p[1]);
    return result;
}

static std::string address(asio::ip::tcp::socket& socket)
{
    std::string address;
    asio::error_code code;
    auto endpoint = socket.remote_endpoint(code);
    if (!code)
    {
        address.append(endpoint.address().to_string());
        address.append(":");
        address.append(std::to_string(endpoint.port()));
    }
    return address;
}

class worker
{
    using asio_work_t = asio::executor_work_guard<asio::io_context::executor_type>;

public:
    worker()
        : ioctx_(1)
        , work_(asio::make_work_guard(ioctx_))
    {}

    ~worker()
    {
        stop();
    }

    void run()
    {
        thread_ = std::thread([this]() { ioctx_.run(); });
    }

    void stop()
    {
        ioctx_.stop();
        if (thread_.joinable())
            thread_.join();
    }

    auto get_executor()
    {
        return ioctx_.get_executor();
    }

    size_t count() const {
        return counter_.load(std::memory_order_acquire);
    }

    void retain()
    {
        counter_.fetch_add(1, std::memory_order_release);
    }

    void release() {
        counter_.fetch_sub(1, std::memory_order_release);
    }
private:
    asio::io_context ioctx_;
    asio_work_t work_;
    std::thread thread_;
    std::atomic<size_t> counter_;
};
using worker_ptr_t = std::unique_ptr<worker>;

class back_thread
{
    worker worker_;
    asio::steady_timer timer_;
    std::atomic<host_map_t*> hosts;

    std::string hosts_file;
    std::string hosts_content;

    void auto_load_host()
    {
        timer_.expires_after(std::chrono::seconds(10));
        timer_.async_wait([this](const asio::error_code& e) {
            if (e)
            {
                return;
            }

            if (!hosts_file.empty())
            {
                load_hosts(hosts_file);
            }
            auto_load_host();
        });
    }

public:
    back_thread()
        : timer_(worker_.get_executor())
    {}

    void run()
    {
        auto_load_host();
        worker_.run();
    }

    void stop()
    {
        worker_.stop();
    }

    static std::tm* localtime(std::time_t* t, std::tm* result)
    {
#ifdef _MSC_VER
        localtime_s(result, t);
#else
        localtime_r(t, result);
#endif
        return result;
    }

    template<typename... Args>
    void log(const char* fmt, Args&&... args)
    {
        std::string line;
        line.resize(1024);
        snprintf(line.data(), 1024 - 1, fmt, args...);
        asio::post(worker_.get_executor(), [line = std::move(line)]() -> void {
            auto now = time(nullptr);
            std::tm m;
            localtime(&now, &m);
            char tmbuffer[80];
            strftime(tmbuffer, 80, "%Y-%m-%d %H:%M:%S", &m);
            printf("%s | %s\n", tmbuffer, line.data());
            fflush(stdout);
        });
    }

    std::optional<target_host_t> find_host(int node) const
    {
        host_map_t* map = hosts.load(std::memory_order_acquire);
        if (nullptr == map)
            return std::optional<target_host_t>{};
        auto iter = map->find(node);
        if (iter != map->end())
        {
            return iter->second;
        }
        return std::optional<target_host_t>{};
    }

    bool load_hosts(const std::string& file)
    {
        try
        {
            std::fstream is(file, std::ios::binary | std::ios::in);
            if (!is.is_open())
                return false;
            std::string content((std::istreambuf_iterator<char>(is)), std::istreambuf_iterator<char>());
            if (!hosts_content.empty() && hosts_content.size() == content.size() && hosts_content == content)
            {
                return true;
            }

            hosts_file = file;
            hosts_content = content;

            auto json = nlohmann::json::parse(content);
            if (!json.is_array())
                return false;

            host_map_t* new_hosts = new host_map_t{};

            for (auto& obj : json)
            {
                if (!obj.is_object())
                    return false;
                if (obj.contains("node") && obj.contains("type") && obj.contains("host"))
                {
                    int node = obj["node"];
                    std::string type = obj["type"];
                    std::string host = obj["host"];
                    if(type!="game")
                        continue;

                    log("load: %s %d %s", type.data(), node, host.data());
                    auto res = parse_host_port(host, 0);
                    if (res.second == 0)
                    {
                        log("host format error %s", host.data());
                        return false;
                    }
                    asio::ip::tcp::resolver resolver(worker_.get_executor());
                    auto endpoints = resolver.resolve(res.first, std::to_string(res.second));
                    new_hosts->emplace(node, std::make_pair(endpoints, host));
                }
            }
            hosts.store(new_hosts, std::memory_order_release);
            log("load hosts count %d", (int)new_hosts->size());
            return true;
        }
        catch (std::exception& e)
        {
            log("load_hosts exception: %s", e.what());
        }
        return false;
    }

    static std::pair<std::string, unsigned short> parse_host_port(const std::string& host_port, unsigned short default_port)
    {
        std::string host, port;
        host.reserve(host_port.size());
        bool parse_port = false;
        int square_count = 0; // To parse IPv6 addresses
        for (auto chr : host_port)
        {
            if (chr == '[')
                ++square_count;
            else if (chr == ']')
                --square_count;
            else if (square_count == 0 && chr == ':')
                parse_port = true;
            else if (!parse_port)
                host += chr;
            else
                port += chr;
        }

        if (port.empty())
            return {std::move(host), default_port};
        else
        {
            try
            {
                return {std::move(host), static_cast<unsigned short>(std::stoul(port))};
            }
            catch (...)
            {
                return {std::move(host), default_port};
            }
        }
    }
};

static back_thread g_back_thread;

#define CONSOLE_LOG(fmt, ...) g_back_thread.log(fmt " (%s:%d)", ##__VA_ARGS__, __FILENAME__, __LINE__);

class proxy
{
    static awaitable<asio::error_code> read_message(tcp::socket& sock, asio::streambuf& buf)
    {
        std::tuple<asio::error_code, size_t> res =
            co_await asio::async_read(sock, buf, asio::transfer_exactly(sizeof(uint16_t)), use_nothrow_awaitable);
        if (!std::get<0>(res))
        {
            auto data = reinterpret_cast<const char*>(buf.data().data());
            uint16_t size;
            memcpy(&size, data, sizeof(uint16_t));
            size = network_to_host_short(size);
            buf.prepare(size);
            res = co_await asio::async_read(sock, buf, asio::transfer_exactly(size), use_nothrow_awaitable);
        }
        co_return std::get<0>(res);
    }

    static asio::mutable_buffer message_content(asio::streambuf& buf)
    {
        auto p = (char*)(buf.data().data());
        return asio::mutable_buffer(p + sizeof(uint16_t), buf.size() - sizeof(uint16_t));
    }

    static awaitable<void> timeout(steady_clock::duration duration)
    {
        asio::steady_timer timer(co_await this_coro::executor);
        timer.expires_after(duration);
        co_await timer.async_wait(use_nothrow_awaitable);
    }

    static awaitable<bool> handshake(tcp::socket& from, tcp::socket& to)
    {
        //asio::streambuf handshake;
        //auto e1 = co_await read_message(from, handshake);
        //if (e1)
        //{
        //    CONSOLE_LOG("Handshake read %s: %s\n", address(from).data(), e1.message().data());
        //    co_return false;
        //}

        // auto content = message_content(handshake);
        // TODO
        // e. Diffie-Hellman Key exchange

        int node = 1;

        auto target = g_back_thread.find_host(node);
        if (!target.has_value())
        {
            CONSOLE_LOG("Connect %s: unknown node %d", address(from).data(), node);
            co_return false;
        }

        auto [e2, ep] = co_await asio::async_connect(to, target.value().first, use_nothrow_awaitable);
        if (e2)
        {
            if (e2 == asio::error::operation_aborted)
            {
                CONSOLE_LOG("Connect %s->%s: timeout", address(from).data(), target.value().second.data());
            }
            else
            {
                CONSOLE_LOG("Connect %s->%s: %s", address(from).data(), target.value().second.data(), e2.message().data());
            }
            co_return false;
        }
        CONSOLE_LOG("%s->%s: Handshake success", address(from).data(), target.value().second.data());
        co_return true;
    }

    static awaitable<asio::error_code> transfer_client_to_server(tcp::socket& from, tcp::socket& to)
    {
        asio::streambuf buf;
        for (;;)
        {
            if (auto e = co_await read_message(from, buf); e)
                co_return e;

            auto content = message_content(buf);
            // TODO
            // e. Decrypt message use streaming encryption algorithm , RC4, chacha20

            if (auto [e, n] = co_await asio::async_write(to, buf.data(), use_nothrow_awaitable); e)
                co_return e;

            buf.consume(buf.size());
        }
    }

    static awaitable<asio::error_code> transfer_server_to_client(tcp::socket& from, tcp::socket& to)
    {
        std::array<char, 1024> data{};
        for (;;)
        {
            auto [e1, n1] = co_await from.async_read_some(buffer(data), use_nothrow_awaitable);
            if (e1)
                co_return e1;

            // TODO if you wang handle message from server to client

            if (auto [e2, n2] = co_await async_write(to, buffer(data, n1), use_nothrow_awaitable); e2)
                co_return e2;
        }
    }

public:
    static awaitable<void> start(tcp::socket client, worker* w)
    {
        w->retain();
        tcp::socket server(client.get_executor());
        do
        {
            auto res = co_await(handshake(client, server) || timeout(5s));
            if (res.index() == 1 || !std::get<0>(res))
                break;
            auto res2 = co_await(transfer_client_to_server(client, server) || transfer_server_to_client(server, client));
            if (res.index() == 0)
            {
                CONSOLE_LOG("%s->%s: %s\n", address(client).data(), address(server).data(), std::get<0>(res2).message().data());
            }
            else
            {
                CONSOLE_LOG("%s->%s: %s\n", address(server).data(), address(client).data(), std::get<1>(res2).message().data());
            }
        } while (false);

        asio::error_code ignore;
        client.shutdown(asio::socket_base::shutdown_both, ignore);
        if (server.is_open())
            server.shutdown(asio::socket_base::shutdown_both, ignore);

        w->release();
    }
};

awaitable<void> listen(tcp::acceptor& acceptor, std::vector<worker_ptr_t>& worker_ctx)
{
    for (;;)
    {
        worker* w = nullptr;
        size_t min_worker_load = std::numeric_limits<size_t>::max();
        for (const auto& one : worker_ctx) {
            size_t count = one->count();
            if (count < min_worker_load)
            {
                min_worker_load = count;
                w = one.get();
            }
        }

        tcp::socket client(w->get_executor());
        auto [e] = co_await acceptor.async_accept(client, use_nothrow_awaitable);
        if (e) {
            break;
        }
        co_spawn(w->get_executor(), proxy::start(std::move(client), w), detached);
    }
}

void usage(void)
{
    std::cout << "Usage:\n";
    std::cout << "        proxy [-p listenport] [-c filename] [-t threadnum]\n";
    std::cout << "The options are:\n";
    std::cout << "        -p          listen port\n";
    std::cout << "        -c          game list configure\n";
    std::cout << "        -t          thread num for handle connection\n";
}

int main(int argc, char* argv[])
{
    try
    {
        g_back_thread.run();

        std::string file;
        uint32_t nthread = 2 * std::thread::hardware_concurrency();
        uint16_t port = 3001;

        for (int i = 1; i < argc; ++i)
        {
            bool lastarg = i == (argc - 1);
            std::string_view v{argv[i]};
            if ((v == "-h"sv || v == "--help"sv) && !lastarg)
            {
                usage();
                return -1;
            }
            else if ((v == "-c"sv || v == "--config"sv) && !lastarg)
            {
                file = argv[++i];
            }
            else if ((v == "-t"sv || v == "--thread"sv) && !lastarg)
            {
                nthread = std::clamp(std::stoi(argv[++i]), 1, 256);
            }
            else if ((v == "-p"sv || v == "--port"sv) && !lastarg)
            {
                port = static_cast<uint16_t>(std::stoi(argv[++i]));
            }
            else
            {
                usage();
                return -1;
            }
        }

        if (!std::filesystem::exists(file) || nthread == 0 || port == 0)
        {
            usage();
            return -1;
        }

        if (!g_back_thread.load_hosts(file))
        {
            usage();
            return -1;
        }

        asio::io_context main_ctx;

        std::vector<worker_ptr_t> worker_ctx;
        for (uint32_t i = 0; i < nthread; ++i)
        {
            worker_ctx.emplace_back(std::make_unique<worker>())->run();
        }

        tcp::acceptor acceptor(main_ctx, {tcp::v4(), port}, true);

        asio::signal_set signals(main_ctx, SIGINT, SIGTERM);
        signals.async_wait([&](auto, auto) { main_ctx.stop(); });

        co_spawn(main_ctx, listen(acceptor, worker_ctx), detached);

        main_ctx.run();

        for (auto& w : worker_ctx)
        {
            w->stop();
        }

        g_back_thread.stop();
    }
    catch (std::exception& e)
    {
        std::cerr << "Exception: " << e.what() << "\n";
    }
}
