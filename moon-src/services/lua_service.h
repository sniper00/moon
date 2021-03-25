#pragma  once
#include "lua.hpp"
#include "common/log.hpp"
#include "common/buffer.hpp"
#include "service.hpp"

#define LMOON_GLOBAL "LMOON_GLOBAL"
#define LASIO_GLOBAL "LASIO_GLOBAL"

class lua_service :public moon::service
{
    struct state_deleter {
        void operator()(lua_State* L) const {
            lua_close(L);
        }
    };
public:
    lua_service();

    ~lua_service();
private:
    bool init(std::string_view config) override;

    void dispatch(moon::message* msg) override;

    static void* lalloc(void * ud, void *ptr, size_t osize, size_t nsize);
public:
    size_t mem = 0;
    size_t mem_limit = 0;
    size_t mem_report = 8 * 1024 * 1024;
private:
    std::unique_ptr<lua_State, state_deleter> lua_;
};