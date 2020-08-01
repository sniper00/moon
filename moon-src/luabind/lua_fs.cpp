#include "common/directory.hpp"
#include "sol.hpp"

using namespace moon;

static auto listdir(const std::string& dir, sol::optional<int> depth)
{
    int n = 0;
    if (depth)
    {
        n = depth.value();
    }
    std::vector<std::string> files;
    directory::traverse_folder(dir, n, [&files](const fs::path& path, bool)->bool {
        files.emplace_back(path.string());
        return true;
    });
    return sol::as_table(files);
}

#if defined(__GNUC__)
static fs::path lexically_relative(const fs::path& p, const fs::path& _Base)
{
    using namespace std::string_view_literals; // TRANSITION, VSO#571749
    constexpr std::wstring_view _Dot = L"."sv;
    constexpr std::wstring_view _Dot_dot = L".."sv;
    fs::path _Result;
    if (p.root_name() != _Base.root_name()
        || p.is_absolute() != _Base.is_absolute()
        || (!p.has_root_directory() && _Base.has_root_directory()))
    {
        return (_Result);
    }

    const fs::path::iterator _This_end = p.end();
    const fs::path::iterator _Base_begin = _Base.begin();
    const fs::path::iterator _Base_end = _Base.end();

    auto _Mismatched = std::mismatch(p.begin(), _This_end, _Base_begin, _Base_end);
    fs::path::iterator& _A_iter = _Mismatched.first;
    fs::path::iterator& _B_iter = _Mismatched.second;

    if (_A_iter == _This_end && _B_iter == _Base_end)
    {
        _Result = _Dot;
        return (_Result);
    }

    {	// Skip root-name and root-directory elements, N4727 30.11.7.5 [fs.path.itr]/4.1, 4.2
        ptrdiff_t _B_dist = std::distance(_Base_begin, _B_iter);

        const ptrdiff_t _Base_root_dist = static_cast<ptrdiff_t>(_Base.has_root_name())
            + static_cast<ptrdiff_t>(_Base.has_root_directory());

        while (_B_dist < _Base_root_dist)
        {
            ++_B_iter;
            ++_B_dist;
        }
    }

    ptrdiff_t _Num = 0;

    for (; _B_iter != _Base_end; ++_B_iter)
    {
        const fs::path& _Elem = *_B_iter;

        if (_Elem.empty())
        {	// skip empty element, N4727 30.11.7.5 [fs.path.itr]/4.4
        }
        else if (_Elem == _Dot)
        {	// skip filename elements that are dot, N4727 30.11.7.4.11 [fs.path.gen]/4.2
        }
        else if (_Elem == _Dot_dot)
        {
            --_Num;
        }
        else
        {
            ++_Num;
        }
    }

    if (_Num < 0)
    {
        return (_Result);
    }

    for (; _Num > 0; --_Num)
    {
        _Result /= _Dot_dot;
    }

    for (; _A_iter != _This_end; ++_A_iter)
    {
        _Result /= *_A_iter;
    }
    return (_Result);
}
#endif

static sol::table bind_fs(sol::this_state L)
{
    sol::state_view lua(L);
    sol::table m = lua.create_table();
    m.set_function("listdir", listdir);

    m.set_function("isdir", [](const std::string_view& s) {
        return fs::is_directory(s);
    });

    m.set_function("join", [](const std::string_view& p1, const std::string_view& p2) {
        fs::path p(p1);
        p += fs::path(p2);
        return p.string();
    });

    m.set_function("exists", directory::exists);
    m.set_function("create_directory", directory::create_directory);
    m.set_function("remove", directory::remove);
    m.set_function("remove_all", directory::remove_all);
    m.set_function("working_directory", []() {
        return directory::working_directory.string();
    });
    m.set_function("parent_path", [](const std::string_view& s) {
        return fs::absolute(fs::path(s)).parent_path().string();
    });

    m.set_function("filename", [](const std::string_view& s) {
        return fs::path(s).filename().string();
    });

    m.set_function("extension", [](const std::string_view& s) {
        return fs::path(s).extension().string();
    });

    m.set_function("root_path", [](const std::string_view& s) {
        return fs::path(s).root_path().string();
    });

    //filename_without_extension
    m.set_function("stem", [](const std::string_view& s) {
        return fs::path(s).stem().string();
    });

    m.set_function("relative_work_path", [](const std::string_view& p) {
#if defined(__GNUC__)
        return  lexically_relative(fs::absolute(p), directory::working_directory).string();
#else
        return  fs::absolute(p).lexically_relative(directory::working_directory).string();
#endif
    });
    return m;
}

extern "C"
{
    int LUAMOD_API luaopen_fs(lua_State* L)
    {
        return sol::stack::call_lua(L, 1, bind_fs);
    }
}
