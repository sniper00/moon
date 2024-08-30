#pragma once
#include "config.hpp"
#include "log.hpp"

namespace moon {
class worker;
class server;
class message;

class service {
public:
    friend class worker;

    service() = default;

    service(const service&) = delete;

    service& operator=(const service&) = delete;

    virtual ~service() {}

    uint32_t id() const {
        return id_;
    }

    const std::string& name() const {
        return name_;
    }

    void set_name(const std::string& name) {
        name_ = name;
    }

    void set_server_context(server* s, worker* w) {
        server_ = s;
        worker_ = w;
    }

    server* get_server() const {
        return server_;
    }

    worker* get_worker() const {
        return worker_;
    }

    bool unique() const {
        return unique_;
    }

    bool ok() const {
        return ok_;
    }

    void ok(bool v) {
        ok_ = v;
    }

    double cpu() {
        return std::exchange(cpu_, 0);
    }

public:
    virtual bool init(const service_conf& conf) = 0;

    virtual void dispatch(message* msg) = 0;

protected:
    void set_unique(bool v) {
        unique_ = v;
    }

    void set_id(uint32_t v) {
        id_ = v;
    }

    void add_cpu(double v) {
        cpu_ += v;
    }

protected:
    bool ok_ = false;
    bool unique_ = false;
    uint32_t id_ = 0;
    server* server_ = nullptr;
    worker* worker_ = nullptr;
    double cpu_ = 0.0; //
    std::string name_;
};

template<typename Service, typename Message>
inline void handle_message(Service&& s, Message&& m) {
    uint32_t receiver = m.receiver();
    s->dispatch(&m);
    //redirect message
    if (m.receiver() != receiver) {
        if constexpr (std::is_rvalue_reference_v<decltype(m)>) {
            s->get_server()->send_message(std::forward<message>(m));
        }
    }
}

using service_ptr_t = std::unique_ptr<service>;
} // namespace moon
