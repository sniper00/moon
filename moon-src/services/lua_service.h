#pragma  once
#include "common/lua_utility.hpp"
#include "service.hpp"

#define LMOON_GLOBAL "LMOON_GLOBAL"

class lua_service :public moon::service
{
public:
    lua_service();

    ~lua_service();

private:
    bool init(const moon::service_conf& conf) override;

    void dispatch(moon::message* msg) override;

    static void* lalloc(void * ud, void *ptr, size_t osize, size_t nsize);
public:
    size_t mem = 0;
    size_t mem_limit = 0;
    size_t mem_report = 8 * 1024 * 1024;
private:
    std::unique_ptr<lua_State, moon::state_deleter> lua_;
};