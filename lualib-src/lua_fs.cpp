#include "common/directory.hpp"
#include "common/lua_utility.hpp"
#include "lua.hpp"

using namespace moon;

static int lfs_listdir(lua_State* L) {
    std::string_view dir = lua_check<std::string_view>(L, 1);
    int depth = (int)luaL_optinteger(L, 2, 0);
    const char* ext = luaL_optstring(L, 3, nullptr);
    int idx = 0;
    lua_createtable(L, 32, 0);
    directory::scan_dir(dir, depth, [L, &idx, ext](const fs::path& path, bool isdir) -> bool {
        if (nullptr != ext) {
            if (!isdir && path.extension().string() == ext) {
                std::string v = path.string();
                lua_pushlstring(L, v.data(), v.size());
                lua_rawseti(L, -2, ++idx);
            }
        } else {
            std::string v = path.string();
            lua_pushlstring(L, v.data(), v.size());
            lua_rawseti(L, -2, ++idx);
        }
        return true;
    });
    return 1;
}

static int lfs_isdir(lua_State* L) {
    try {
        std::string_view dir = lua_check<std::string_view>(L, 1);
        bool res = fs::is_directory(dir);
        lua_pushboolean(L, res ? 1 : 0);
        return 1;
    } catch (const std::exception& e) {
        lua_pushfstring(L, "fs.isdir %s", e.what());
    }
    return lua_error(L);
}

static int lfs_join(lua_State* L) {
    int n = lua_gettop(L);
    if (0 == n)
        return 0;

    try {
        fs::path dir;
        for (int i = 1; i <= n; i++) {
            if (lua_type(L, i) != LUA_TSTRING)
                throw std::runtime_error("neen string path");
            dir /= fs::path { lua_tostring(L, i) };
        }
        std::string s = dir.string();
        lua_pushlstring(L, s.data(), s.size());
        return 1;
    } catch (const std::exception& e) {
        lua_pushfstring(L, "fs.join %s", e.what());
    }
    return lua_error(L);
}

static int lfs_exists(lua_State* L) {
    try {
        bool res = fs::exists(lua_check<std::string_view>(L, 1));
        lua_pushboolean(L, res ? 1 : 0);
        return 1;
    } catch (const std::exception& e) {
        lua_pushfstring(L, "fs.exists %s", e.what());
    }
    return lua_error(L);
}

static int lfs_mkdir(lua_State* L) {
    try {
        bool res = fs::create_directories(lua_check<std::string_view>(L, 1));
        lua_pushboolean(L, res ? 1 : 0);
        return 1;
    } catch (const std::exception& e) {
        lua_pushfstring(L, "fs.mkdir %s", e.what());
    }
    return lua_error(L);
}

static int lfs_remove(lua_State* L) {
    try {
        bool res = false;
        if (lua_isnoneornil(L, 2))
            res = fs::remove(lua_check<std::string_view>(L, 1));
        else
            res = fs::remove_all(lua_check<std::string_view>(L, 1));
        lua_pushboolean(L, res ? 1 : 0);
        return 1;
    } catch (const std::exception& e) {
        lua_pushfstring(L, "fs.remove %s", e.what());
    }
    return lua_error(L);
}

static int lfs_cwd(lua_State* L) {
    try {
        std::string s = directory::working_directory.string();
        lua_pushlstring(L, s.data(), s.size());
        return 1;
    } catch (const std::exception& e) {
        lua_pushfstring(L, "fs.cwd %s", e.what());
    }
    return lua_error(L);
}

static int lfs_split(lua_State* L) {
    try {
        fs::path dir = fs::absolute(lua_check<std::string_view>(L, 1));
        std::string dirname = dir.parent_path().string();
        lua_pushlstring(L, dirname.data(), dirname.size());
        std::string basename = dir.filename().string();
        lua_pushlstring(L, basename.data(), basename.size());
        return 2;
    } catch (const std::exception& e) {
        lua_pushfstring(L, "fs.split %s", e.what());
    }
    return lua_error(L);
}

static int lfs_ext(lua_State* L) {
    try {
        fs::path dir = fs::absolute(lua_check<std::string_view>(L, 1));
        std::string ext = dir.extension().string();
        lua_pushlstring(L, ext.data(), ext.size());
        return 1;
    } catch (const std::exception& e) {
        lua_pushfstring(L, "fs.ext %s", e.what());
    }
    return lua_error(L);
}

static int lfs_root(lua_State* L) {
    try {
        fs::path dir = fs::absolute(lua_check<std::string_view>(L, 1));
        std::string s = dir.root_path().string();
        lua_pushlstring(L, s.data(), s.size());
        return 1;
    } catch (const std::exception& e) {
        lua_pushfstring(L, "fs.root %s", e.what());
    }
    return lua_error(L);
}

static int lfs_stem(lua_State* L) {
    try {
        fs::path dir = fs::path(lua_check<std::string_view>(L, 1));
        std::string s = dir.stem().string();
        lua_pushlstring(L, s.data(), s.size());
        return 1;
    } catch (const std::exception& e) {
        lua_pushfstring(L, "fs.stem %s", e.what());
    }
    return lua_error(L);
}

static int lfs_abspath(lua_State* L) {
    try {
        fs::path dir = fs::absolute(lua_check<std::string_view>(L, 1));
        std::string s = dir.string();
        lua_pushlstring(L, s.data(), s.size());
        return 1;
    } catch (const std::exception& e) {
        lua_pushfstring(L, "fs.abspath %s", e.what());
    }
    return lua_error(L);
}

extern "C" {
int LUAMOD_API luaopen_fs(lua_State* L) {
    luaL_Reg l[] = { { "listdir", lfs_listdir },
                     { "isdir", lfs_isdir },
                     { "join", lfs_join },
                     { "exists", lfs_exists },
                     { "mkdir", lfs_mkdir },
                     { "remove", lfs_remove },
                     { "cwd", lfs_cwd },
                     { "split", lfs_split },
                     { "ext", lfs_ext },
                     { "root", lfs_root },
                     { "stem", lfs_stem },
                     { "abspath", lfs_abspath },
                     { NULL, NULL } };
    luaL_newlib(L, l);
    return 1;
}
}
