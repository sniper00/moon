#include "worker.h"
#include "common/hash.hpp"
#include "common/string.hpp"
#include "common/time.hpp"
#include "message.hpp"
#include "server.h"
#include "service.hpp"

namespace moon {
worker::worker(server* srv, uint32_t id):
    workerid_(id),
    server_(srv),
    io_ctx_(1),
    work_(asio::make_work_guard(io_ctx_)) {}

worker::~worker() {
    wait();
}

uint32_t worker::alive() {
    auto n = version_;
    asio::post(io_ctx_, [this]() { ++version_; });
    return n;
}

void worker::run() {
    socket_server_ = std::make_unique<moon::socket_server>(server_, this, io_ctx_);

    thread_ = std::thread([this]() {
        CONSOLE_INFO("WORKER-%u START", workerid_);
        io_ctx_.run();
        socket_server_->close_all();
        services_.clear();
        CONSOLE_INFO("WORKER-%u STOP", workerid_);
    });
}

void worker::stop() {
    asio::post(io_ctx_, [this] {
        auto m = message { PTYPE_SHUTDOWN, 0, 0, 0 };
        for (const auto&[k,v]: services_) {
            v->dispatch(&m);
        }
    });
}

void worker::wait() {
    io_ctx_.stop();
    if (thread_.joinable()) {
        thread_.join();
    }
}

void worker::signal(int val) const{
    if (auto s = current_.load(std::memory_order_acquire); s != nullptr) {
        s->signal(val);
    }
}

uint32_t worker::allocate_service_id(uint32_t opt_service_id) {
    // Use specified service ID if provided
    if (opt_service_id != 0) {
        if (services_.find(opt_service_id) != services_.end()) {
            CONSOLE_ERROR(
                "new service failed: serviceid[%08X] already exists, worker[%u] service num[%zu].",
                opt_service_id,
                id(),
                services_.size()
            );
            return 0;
        }
        return opt_service_id;
    }

    // Auto-allocate service ID
    size_t counter = 0;
    uint32_t serviceid = 0;
    
    do {
        if (counter >= WORKER_MAX_SERVICE) {
            CONSOLE_ERROR(
                "new service failed: can not get more service id. worker[%u] service num[%zu].",
                id(),
                services_.size()
            );
            return 0;
        }

        ++nextid_;
        if (nextid_ == WORKER_MAX_SERVICE) {
            nextid_ = 1;
        }
        serviceid = nextid_ | (id() << WORKER_ID_SHIFT);
        ++counter;
    } while (services_.find(serviceid) != services_.end());

    return serviceid;
}

void worker::new_service(std::unique_ptr<service_conf> conf) {
    count_.fetch_add(1, std::memory_order_relaxed);
    asio::post(io_ctx_, [this, conf = std::move(conf)]() {
        // Allocate service ID
        uint32_t serviceid = allocate_service_id(conf->opt_service_id);
        if (serviceid == 0) {
            count_.fetch_sub(1, std::memory_order_relaxed);
            if (services_.empty()) {
                shared(true);
            }
            if (conf->session != 0) {
                server_->send_message(message { PTYPE_INTEGER, 0, conf->creator, conf->session, 0 });
            }
            return;
        }

        // Create service instance
        auto s = server_->make_service(conf->type);
        if (!s) {
            CONSOLE_ERROR(
                "new service failed: service type[%s] was not registered",
                conf->type.data()
            );
            count_.fetch_sub(1, std::memory_order_relaxed);
            if (services_.empty()) {
                shared(true);
            }
            if (conf->session != 0) {
                server_->send_message(message { PTYPE_INTEGER, 0, conf->creator, conf->session, 0 });
            }
            return;
        }

        // Initialize service
        s->set_id(serviceid);
        s->set_unique(conf->unique);
        s->set_server_context(server_, this);

        if (!s->init(*conf)) {
            if (serviceid == BOOTSTRAP_ADDR) {
                server_->stop(-1);
            }
            count_.fetch_sub(1, std::memory_order_relaxed);
            if (services_.empty()) {
                shared(true);
            }
            if (conf->session != 0) {
                server_->send_message(message { PTYPE_INTEGER, 0, conf->creator, conf->session, 0 });
            }
            return;
        }

        // Service created successfully
        s->ok(true);
        services_.emplace(serviceid, std::move(s));

        if (conf->session != 0) {
            server_->send_message(
                message { PTYPE_INTEGER, 0, conf->creator, conf->session, serviceid }
            );
        }
    });
}

void worker::remove_service(uint32_t serviceid, uint32_t sender, int64_t sessionid) {
    asio::post(io_ctx_, [this, serviceid, sender, sessionid]() {
        auto s = find_service(serviceid);
        if (nullptr == s) {
            server_->response(
                sender,
                moon::format("worker::remove_service [%08X] not found", serviceid),
                sessionid,
                PTYPE_ERROR
            );
            return;
        }

        auto name = s->name();
        auto id = s->id();
        count_.fetch_sub(1, std::memory_order_relaxed);
        services_.erase(serviceid);
        
        // Mark worker as shared if no services remain
        if (services_.empty()) {
            shared(true);
        }

        server_->response(sender, "service destroy"sv, sessionid);

        // Broadcast service exit notification (only if server is ready)
        if (server_->get_state() == state::ready) {
            auto content = moon::format("_service_exit,name:%s serviceid:%08X", name.data(), id);
            auto buf = buffer { content.size() };
            buf.write_back(content);
            server_->broadcast(serviceid, buf, PTYPE_SYSTEM);
        }

        // Check if bootstrap service was removed
        if (serviceid == BOOTSTRAP_ADDR) {
            server_->set_state(state::stopping);
        }
    });
}

void worker::scan(uint32_t sender, int64_t sessionid) {
    asio::post(io_ctx_, [this, sender, sessionid] {
        if (services_.empty()) {
            server_->response(sender, std::string_view{}, sessionid);
            return;
        }

        std::string content;
        // Pre-allocate approximate space: each entry ~50 chars
        content.reserve(services_.size() * 50 + 2);
        content.append("[");

        bool first = true;
        for (const auto& [k,v]: services_) {
            if (!first) {
                content.append(",");
            }
            first = false;

            content.append(
                moon::format(
                    R"({"name":"%s","serviceid":"%X"})",
                    v->name().data(),
                    v->id()
                )
            );
        }

        content.append("]");
        server_->response(sender, content, sessionid);
    });
}

asio::io_context& worker::io_context() {
    return io_ctx_;
}

void worker::send(message&& msg) {
    if (mq_.push_back(std::move(msg)) == 1) {
        asio::post(io_ctx_, [this]() {
            auto& read_queue = mq_.swap_on_read();
            if (read_queue.empty()) {
                return;
            }

            auto size = read_queue.size();
            swapped_size_.store(size, std::memory_order_relaxed);
            
            // Process all messages in the queue
            service* cached_service = nullptr;
            for (auto& m: read_queue) {
                cached_service = handle_one(cached_service, std::move(m));
                swapped_size_.store(--size, std::memory_order_relaxed);
            }
            
            read_queue.clear();
            current_.store(nullptr, std::memory_order_relaxed);
        });
    }
}

uint32_t worker::id() const {
    return workerid_;
}

service* worker::find_service(uint32_t serviceid) const {
    if (auto iter = services_.find(serviceid); services_.end() != iter) {
        return iter->second.get();
    }
    return nullptr;
}

void worker::shared(bool v) {
    shared_ = v;
}

bool worker::shared() const {
    return shared_.load();
}

service* worker::handle_one(service* s, message&& msg) {
    uint32_t sender = msg.sender;
    uint32_t receiver = msg.receiver;
    uint8_t type = msg.type;

    if (receiver > 0) {
        if (nullptr == s || s->id() != receiver) {
            s = find_service(receiver);
            if (nullptr == s || !s->ok()) {
                if (sender != 0 && msg.type != PTYPE_TIMER) {
                    if (msg.session >= 0) {
                        CONSOLE_DEBUG(
                            "Dead service [%08X] recv message from [%08X]: %s.",
                            receiver,
                            sender,
                            moon::escape_print({ msg.data(), msg.size() }).data()
                        );
                    } else {
                        std::string str = moon::format(
                            "Attemp call dead service [%08X]: %s.",
                            receiver,
                            moon::escape_print({ msg.data(), msg.size() }).data()
                        );
                        msg.session = -msg.session;
                        server_->response(sender, str, msg.session, PTYPE_ERROR);
                    }
                }
                return nullptr;
            }
        }

        double start_time = moon::time::clock();
        current_.store(s, std::memory_order_release);
        handle_message(s, std::move(msg));
        double diff_time = moon::time::clock() - start_time;
        s->add_cpu(diff_time);
        cpu_ += diff_time;
        if (diff_time > 0.1) {
            CONSOLE_WARN(
                "worker %u handle one message(%d) cost %f, from %08X to %08X",
                id(),
                type,
                diff_time,
                sender,
                receiver
            );
        }
        return s->ok() ? s : nullptr;
    }

    for (const auto& [k,v]: services_) {
        if (!v->unique() && type == PTYPE_SYSTEM) {
            continue;
        }

        if (v->ok() && v->id() != sender) {
            current_.store(v.get(), std::memory_order_release);
            handle_message(v, msg);
        }
    }
    return nullptr;
}
} // namespace moon
