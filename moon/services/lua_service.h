#pragma  once
#include "service.h"
#include "log.h"
#include "luabind/lua_bind.h"
#include "components/tcp/tcp.h"

class lua_service :public moon::service
{
public:
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

    void set_init(sol_function_t f);

    void set_start(sol_function_t f);

    void set_dispatch(sol_function_t f);

    void set_exit(sol_function_t f);

    void set_destroy(sol_function_t f);

    moon::tcp* add_component_tcp(const std::string& name);

    moon::tcp* get_component_tcp(const std::string& name);

private:
    bool     init(const std::string& config) override;

    void     start()  override;

    void     update()  override;

    void     exit() override;

    void     destroy() override;

    void     dispatch(moon::message* msg) override;

    void     error(const std::string& msg);

    static void* lalloc(void * ud, void *ptr, size_t osize, size_t nsize);
public:
    size_t mem = 0;
    size_t mem_limit = 0;
    size_t mem_report = 8 * 1024 * 1024;
private:
    bool error_;
    sol::state lua_;
    sol_function_t init_;
    sol_function_t start_;
    sol_function_t dispatch_;
    sol_function_t exit_;
    sol_function_t destroy_;
    moon::timer timer_;
};