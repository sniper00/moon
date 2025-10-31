#include "server.h"
#include "message.hpp"
#include "worker.h"

namespace moon {

server::~server() {
    wait();
}

void server::init(uint32_t worker_num) {
    worker_num = (worker_num == 0) ? 1 : worker_num;

    CONSOLE_INFO("INIT with %d workers.", worker_num);

    for (uint32_t i = 0; i != worker_num; i++) {
        workers_.emplace_back(std::make_unique<worker>(this, i + 1));
        timer_.emplace_back(std::make_unique<timer_type>());
    }

    for (const auto& w: workers_) {
        w->run();
    }

    state_.store(state::init, std::memory_order_release);
}

int server::run() {
    asio::io_context io_context;
    asio::steady_timer timer(io_context);
    asio::error_code ignore;
    bool stop_once = false;

    state_.store(state::ready, std::memory_order_release);
    while (true) {
        now_ = time::now();
        now_without_offset_ = now_ - time::offset();

        int current_exitcode = exitcode_.load(std::memory_order_acquire);
        if (current_exitcode < 0) {
            break;
        }

        if (current_exitcode != std::numeric_limits<int>::max() && !stop_once) {
            stop_once = true;
            CONSOLE_WARN("Received signal code %d", current_exitcode);
            for (auto iter = workers_.rbegin(); iter != workers_.rend(); ++iter) {
                (*iter)->stop();
            }
        }

        if (state_.load(std::memory_order_acquire) == state::stopping) {
            size_t alive = 0;
            for (const auto& w: workers_) {
                if (w->count() != 0) {
                    ++alive;
                }
            }

            if (alive == 0) {
                break;
            }
        }

        for (const auto& t: timer_) {
            t->update(now_);
        }
        timer.expires_after(std::chrono::milliseconds(1));
        timer.wait(ignore);
    }
    wait();
    if (exitcode_.load(std::memory_order_acquire) == std::numeric_limits<int>::max())
        exitcode_.store(0, std::memory_order_release);
    return exitcode_.load(std::memory_order_relaxed);
}

void server::stop(int exitcode) {
    exitcode_.store(exitcode, std::memory_order_release);
}

void server::wait() {
    for (auto iter = workers_.rbegin(); iter != workers_.rend(); ++iter) {
        (*iter)->wait();
    }
    state_.store(state::stopped, std::memory_order_release);
}

state server::get_state() const {
    return state_.load(std::memory_order_acquire);
}

void server::set_state(state st) {
    state_.store(st, std::memory_order_release);
}

std::time_t server::now(bool sync) {
    if (sync) {
        now_ = time::now();
    }

    if (now_ == 0) {
        return time::now();
    }
    return now_;
}

uint32_t server::service_count() const {
    uint32_t res = 0;
    for (const auto& w: workers_) {
        res += w->count();
    }
    return res;
}

worker* server::next_worker() {
    assert(workers_.size() > 0);
    uint32_t min_shared_count = std::numeric_limits<uint32_t>::max();
    uint32_t min_shared_workerid = 0;
    uint32_t min_all_count = std::numeric_limits<uint32_t>::max();
    uint32_t min_all_workerid = 0;
    
    // Single pass optimization: find both shared and overall minimum
    for (const auto& w: workers_) {
        auto n = w->count();
        
        // Track minimum among shared workers
        if (w->shared() && n < min_shared_count) {
            min_shared_count = n;
            min_shared_workerid = w->id();
        }
        
        // Track overall minimum as fallback
        if (n < min_all_count) {
            min_all_count = n;
            min_all_workerid = w->id();
        }
    }
    
    // Prefer shared worker, fallback to any worker with minimum count
    uint32_t selected_id = min_shared_workerid != 0 ? min_shared_workerid : min_all_workerid;
    return workers_[selected_id - 1].get();
}

worker* server::get_worker(uint32_t workerid, uint32_t serviceid) const {
    workerid = workerid ? workerid : worker_id(serviceid);
    if (workerid == 0 || workerid > static_cast<uint32_t>(workers_.size())) {
        return nullptr;
    }
    return workers_[workerid - 1].get();
}

void server::timeout(int64_t interval, uint32_t serviceid, int64_t timerid) {
    auto workerid = worker_id(serviceid);
    assert(workerid > 0);
    if (interval <= 0) {
        on_timer(serviceid, timerid);
        return;
    }
    timer_[workerid - 1]->add(now_ + interval, serviceid, timerid, this);
}

void server::on_timer(uint32_t serviceid, int64_t timerid) const {
    send_message(message{
        PTYPE_TIMER, 0, serviceid, 0, timerid
    });
}

void server::new_service(std::unique_ptr<service_conf> conf) {
    worker* w = get_worker(conf->threadid);
    if (nullptr != w) {
        w->shared(false);
    } else {
        w = next_worker();
    }
    w->new_service(std::move(conf));
}

void server::remove_service(uint32_t serviceid, uint32_t sender, int64_t sessionid) const {
    worker* w = get_worker(0, serviceid);
    if (nullptr != w) {
        w->remove_service(serviceid, sender, sessionid);
    } else {
        auto content = moon::format("server::remove_service invalid service id %u.", serviceid);
        response(sender, content, sessionid, PTYPE_ERROR);
    }
}

bool server::scan_services(uint32_t sender, uint32_t workerid, int64_t sessionid) const {
    auto* w = get_worker(workerid);
    if (nullptr == w) {
        return false;
    }
    w->scan(sender, sessionid);
    return true;
}

bool server::send_message(message&& m) const {
    worker* w = get_worker(0, m.receiver);
    if (nullptr == w) {
        CONSOLE_ERROR("Invalid message receiver id: %X", m.receiver);
        return false;
    }
    w->send(std::move(m));
    return true;
}

bool server::send(
    uint32_t sender,
    uint32_t receiver,
    buffer_ptr_t data,
    int64_t sessionid,
    uint8_t type
) const {
    sessionid = -sessionid;
    return send_message({
        type, sender, receiver, sessionid, std::move(data)
    });
}

void server::broadcast(uint32_t sender, const buffer& buf, uint8_t type) const {
    for (auto& w: workers_) {
        w->send(message{
            type, sender, 0, 0, std::make_unique<buffer>(buf.clone())
        });
    }
}

bool server::register_service(const std::string& type, register_func f) {
    auto ret = regservices_.try_emplace(type, f);
    MOON_ASSERT(ret.second, moon::format("already registed service type[%s].", type.data()).data());
    return ret.second;
}

service_ptr_t server::make_service(const std::string& type) {
    if (auto iter = regservices_.find(type); iter != regservices_.end()) {
        return iter->second();
    }
    return nullptr;
}

std::shared_ptr<const std::string> server::get_env(const std::string& name) const {
    if (std::shared_ptr<const std::string> value; env_.try_get_value(name, value)) {
        return value;
    }
    return nullptr;
}

void server::set_env(std::string name, std::string value) {
    env_.set(std::move(name), std::make_shared<const std::string>(std::move(value)));
}

uint32_t server::get_unique_service(const std::string& name) const {
    if (name.empty()) {
        return 0;
    }
    uint32_t id = 0;
    unique_services_.try_get_value(name, id);
    return id;
}

bool server::set_unique_service(std::string name, uint32_t v) {
    if (name.empty()) {
        return false;
    }
    return unique_services_.try_set(std::move(name), v);
}

void server::response(uint32_t to, std::string_view content, int64_t sessionid, uint8_t mtype)
    const {
    if (to == 0 || sessionid == 0) {
        if (get_state() == state::ready && mtype == PTYPE_ERROR && !content.empty()) {
            CONSOLE_DEBUG("%s", std::string(content).data());
        }
        return;
    }

    send_message(message{
        mtype, 0, to, sessionid, content
    });
}

std::string server::info() const {
    size_t timer_size = 0;
    for (const auto& timer: timer_) {
        timer_size += timer->size();
    }

    std::string req;
    req.reserve(256 + workers_.size() * 128);
    req.append("[\n");
    req.append(moon::format(
        R"({"id":0, "socket":%zu, "timer":%zu, "log":%zu, "service":%u, "error":%zu})",
        socket_num(),
        timer_size,
        log::instance().size(),
        service_count(),
        log::instance().error_count()
    ));
    for (const auto& w: workers_) {
        req.append(",\n");
        req.append(moon::format(
            R"({"id":%u, "cpu":%f, "mqsize":%u, "service":%u, "timer":%zu, "alive":%u})",
            w->id(),
            w->cpu(),
            w->mq_size(),
            w->count(),
            timer_[w->id() - 1]->size(),
            w->alive()
        ));
    }
    req.append("]");
    return req;
}

uint32_t server::nextfd() {
    uint32_t fd = 0;
    do {
        fd = fd_seq_.fetch_add(1, std::memory_order_relaxed);
    } while (fd == 0 || !try_lock_fd(fd));
    return fd;
}

bool server::try_lock_fd(uint32_t fd) {
    std::unique_lock lck(fd_lock_);
    return fd_watcher_.emplace(fd).second;
}

void server::unlock_fd(uint32_t fd) {
    std::unique_lock lck(fd_lock_);
    size_t count = fd_watcher_.erase(fd);
    MOON_CHECK(count == 1, "socket fd erase failed!");
}

size_t server::socket_num() const {
    std::unique_lock lck(fd_lock_);
    return fd_watcher_.size();
}
} // namespace moon
