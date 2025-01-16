#pragma once
#include "common/lua_utility.hpp"
#include "service.hpp"

struct callback_context {
    lua_State* L = nullptr;
};

class lua_service final: public moon::service {
public:
    lua_service();

    ~lua_service();

private:
    bool init(const moon::service_conf& conf) override;

    void dispatch(moon::message* msg) override;

    void signal(int val) override;

    static void* lalloc(void* ud, void* ptr, size_t osize, size_t nsize);

public:
    std::atomic_int trap = 0;
    lua_State* activeL = nullptr;

    static lua_service* get(lua_State* L);

    static int set_callback(lua_State* L);

    int64_t next_sequence();

private:
    ssize_t mem = 0;
    ssize_t mem_limit = std::numeric_limits<ssize_t>::max();
    ssize_t mem_report = 8 * 1024 * 1024;
    int64_t current_sequence_ = 0;
    callback_context* cb_ctx = nullptr;
    std::unique_ptr<lua_State, moon::state_deleter> lua_;
};
