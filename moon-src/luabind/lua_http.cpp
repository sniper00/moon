#include "sol.hpp"
#include "common/http_util.hpp"

using namespace moon;

static sol::table bind_http(sol::this_state L)
{
    sol::state_view lua(L);
    sol::table module = lua.create_table();

    module.set_function("parse_request", [](std::string_view data) {
        std::string_view method;
        std::string_view path;
        std::string_view query_string;
        std::string_view version;
        http::case_insensitive_multimap_view header;
        bool ok = http::request_parser::parse(data, method, path, query_string, version, header);
        return std::make_tuple(ok, method, path, query_string, version, sol::as_table(header));
    });

    module.set_function("parse_response", [](std::string_view data) {
        std::string_view version;
        std::string_view status_code;
        http::case_insensitive_multimap_view header;
        bool ok = http::response_parser::parse(data, version, status_code, header);
        return std::make_tuple(ok, version, status_code, sol::as_table(header));
    });

    module.set_function("parse_query_string", [](const std::string &data) {
        return sol::as_table(http::query_string::parse(data));
    });

    module.set_function("create_query_string", [](sol::as_table_t<http::case_insensitive_multimap> src) {
        return http::query_string::create(src.value());
    });

    module.set_function("urlencode", [](std::string_view src) {
        return http::percent::encode(std::string{ src });
    });

    module.set_function("urldecode", [](std::string_view src) {
        return http::percent::decode(std::string{ src });
    });

    return module;
}

extern "C"
{
    int LUAMOD_API luaopen_http(lua_State *L)
    {
        return sol::stack::call_lua(L, 1, bind_http);
    }
}
