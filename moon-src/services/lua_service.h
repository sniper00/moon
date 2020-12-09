#pragma  once
#include "common/log.hpp"
#include "luabind/lua_bind.h"
#include "common/buffer.hpp"
#include "service.hpp"

class lua_service :public moon::service
{
public:
    /*
    http://sol2.readthedocs.io/en/latest/safety.html
    http://sol2.readthedocs.io/en/latest/api/protected_function.html
    http://sol2.readthedocs.io/en/latest/errors.html
    */
    using sol_function_t = sol::function;

    lua_service();

    ~lua_service();

    void set_callback(sol_function_t f);
private:
    bool init(std::string_view config) override;

    void dispatch(moon::message* msg) override;

    static void* lalloc(void * ud, void *ptr, size_t osize, size_t nsize);
public:
    size_t mem = 0;
    size_t mem_limit = 0;
    size_t mem_report = 8 * 1024 * 1024;
private:
    sol::state lua_;
    sol_function_t dispatch_;
};