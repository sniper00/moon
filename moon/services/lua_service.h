#pragma  once
#include "service.h"
#include "common/log.hpp"
#include "luabind/lua_bind.h"
#include "luabind/lua_timer.hpp"
#include "components/tcp/tcp.h"
#include "common/buffer.hpp"

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

    void set_init(sol_function_t f);

    void set_start(sol_function_t f);

    void set_dispatch(sol_function_t f);

    void set_exit(sol_function_t f);

    void set_destroy(sol_function_t f);

    void set_on_timer(sol_function_t f);

    void set_remove_timer(sol_function_t f);

    void register_command(const std::string&, sol_function_t f);

    moon::tcp* get_tcp(const std::string& protocol);

    uint32_t make_cache(const moon::buffer_ptr_t & buf);

    void send_cache(uint32_t receiver, uint32_t cacheid, const  moon::string_view_t& header, int32_t responseid, uint8_t type) const;

    static const fs::path& work_path();
private:
    bool init(const moon::string_view_t& config) override;

    void start()  override;

    void update()  override;

    void exit() override;

    void destroy() override;

    void dispatch(moon::message* msg) override;

    void error(const std::string& msg);

    static void* lalloc(void * ud, void *ptr, size_t osize, size_t nsize);

    void runcmd(uint32_t sender, const std::string& cmd, int32_t responseid)  override;
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
    moon::lua_timer timer_;
    uint32_t cache_uuid_;
    std::unordered_map<uint32_t, moon::buffer_ptr_t> caches_;

    using command_hander_t = std::function<std::string(const std::vector<std::string>&)>;
    std::unordered_map<std::string, command_hander_t> commands_;
};