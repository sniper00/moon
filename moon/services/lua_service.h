#pragma  once
#include "common/log.hpp"
#include "luabind/lua_bind.h"
#include "common/buffer.hpp"
#include "service.hpp"

class lua_service :public moon::service
{
public:
    static constexpr std::string_view LUA_PATH_STR = "/?.lua;"sv;

#if TARGET_PLATFORM == PLATFORM_WINDOWS
    static constexpr std::string_view LUA_CPATH_STR = "/?.dll;"sv;
#else
    static constexpr std::string_view LUA_CPATH_STR = "/?.so;"sv;
#endif

    using lua_state_ptr_t = std::unique_ptr<lua_State, void(*)(lua_State*)>;
    /*
    http://sol2.readthedocs.io/en/latest/safety.html
    http://sol2.readthedocs.io/en/latest/api/protected_function.html
    http://sol2.readthedocs.io/en/latest/errors.html
    */
    using sol_function_t = sol::function;

    lua_service();

    ~lua_service();

    size_t memory_use();

    void set_callback(char c, sol_function_t f);
private:
    bool init(moon::string_view_t config) override;

    void start()  override;

    void exit() override;

    void destroy() override;

    void dispatch(moon::message* msg) override;

    void on_timer(uint32_t timerid, bool remove) override;

    void error(const std::string& msg, bool initialized = true);

    static void* lalloc(void * ud, void *ptr, size_t osize, size_t nsize);
public:
    size_t mem = 0;
    size_t mem_limit = 0;
    size_t mem_report = 8 * 1024 * 1024;
private:
    sol::state lua_;
    sol_function_t init_;
    sol_function_t start_;
    sol_function_t dispatch_;
    sol_function_t exit_;
    sol_function_t destroy_;
    sol_function_t on_timer_;
};