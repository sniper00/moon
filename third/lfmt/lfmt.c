#define LUA_LIB
#include <lua.h>
#include <lauxlib.h>

#include <string.h>
#include <float.h>

#if LUA_VERSION_NUM == 501
static void luaL_tolstring(lua_State *L, int idx, size_t *len) {
    int tt; const char *kind; (void)len;
    if (luaL_callmeta(L, idx, "__tostring")) {
        if (!lua_isstring(L, -1))
            luaL_error(L, "'__tostring' must return a string");
        return;
    }
    switch (lua_type(L, idx)) {
    case LUA_TSTRING:
        lua_pushvalue(L, idx); break;
    case LUA_TBOOLEAN:
        lua_pushstring(L, (lua_toboolean(L, idx) ? "true" : "false"));
        break;
    case LUA_TNIL:
        lua_pushliteral(L, "nil");
        break;
    default: 
        tt = luaL_getmetafield(L, idx, "__name");
        kind = (tt == LUA_TSTRING) ? lua_tostring(L, -1) :
            luaL_typename(L, idx);
        lua_pushfstring(L, "%s: %p", kind, lua_topointer(L, idx));
        if (tt != LUA_TNIL) lua_remove(L, -2);
    }
}
#endif

#if LUA_VERSION_NUM < 502 && !defined(LUA_OK)
static lua_Integer lua_tointegerx(lua_State *L, int idx, int *isint) { 
    lua_Integer i = lua_tointeger(L, idx);
    *isint = i == 0 ? lua_type(L, idx)==LUA_TNUMBER : lua_tonumber(L, idx)==i;
    return i;
}
#endif

#if LUA_VERSION_NUM < 503
static void lua_geti(lua_State *L, int idx, int i)
{ lua_pushinteger(L, i); lua_gettable(L, idx); }

static int lua_isinteger(lua_State *L, int idx) {
    lua_Number v = lua_tonumber(L, idx);
    if (v == 0.0 && lua_type(L,idx) != LUA_TNUMBER) return 0;
    return (lua_Number)(lua_Integer)v == v;
}
#endif

typedef struct fmt_State {
    lua_State *L;
    luaL_Buffer B;
    int idx, top, zeroing;
    const char *p, *e;
} fmt_State;

#define fmt_check(S,cond,...) ((void)((cond)||luaL_error((S)->L,__VA_ARGS__)))


/* read argid */

#define fmt_value(S,i)  ((S)->top+(i))
#define fmt_isdigit(ch) ((ch) >= '0' && (ch) <= '9')
#define fmt_isalpha(ch) ((ch) == '_' \
        || ((ch) >= 'A' && (ch) <= 'Z') \
        || ((ch) >= 'a' && (ch) <= 'z'))

#define FMT_AUTO   "automatic field numbering"
#define FMT_MANUAL "manual field specification"
#define FMT_A2M    "cannot switch from " FMT_AUTO " to " FMT_MANUAL
#define FMT_M2A    "cannot switch from " FMT_MANUAL " to " FMT_AUTO

static void fmt_manualidx(fmt_State *S)
{ fmt_check(S, S->idx <= 1, FMT_A2M); S->idx = 0; }

static int fmt_autoidx(fmt_State *S) {
    fmt_check(S, S->idx != 0, FMT_M2A);
    fmt_check(S, ++S->idx <= S->top, "automatic index out of range");
    return S->idx;
}

static int fmt_integer(fmt_State *S, int *pv) {
    const char *p = S->p;
    unsigned idx = 0;
    while (p < S->e && fmt_isdigit(*p)) {
        int o = idx < INT_MAX/10 || (idx == INT_MAX/10 && *p++ <= INT_MAX%10);
        fmt_check(S, o, "Too many decimal digits in format string");
        idx = idx*10 + (*p++ - '0');
    }
    if (p == S->p) return 0;
    if (pv) *pv = (int)idx;
    S->p = p;
    return 1;
}

static int fmt_identity(fmt_State *S) {
    const char *p = S->p;
    if (fmt_isalpha(*p))
        while (++p < S->e && (fmt_isalpha(*p) || fmt_isdigit(*p)))
            ;
    if (p == S->p) return 0;
    lua_pushlstring(S->L, S->p, p - S->p);
    S->p = p;
    return 1;
}

static int fmt_accessor(fmt_State *S, int to) {
    /* "." (number | identity) | "[" <anychar except ']'> "]" */
    while (*S->p == '.' || *S->p == '[') {
        int idx;
        const char *p = ++S->p;
        if (p[-1] == '.') {
            if (fmt_integer(S, &idx))
                lua_geti(S->L, to, idx);
            else if (fmt_identity(S))
                lua_gettable(S->L, to);
            else luaL_error(S->L, "unexpected '%c' in field name", *S->p);
        } else if (fmt_integer(S, &idx) && *S->p == ']')
            lua_geti(S->L, to, idx), ++S->p;
        else {
            while (p < S->e && *p != ']') ++p;
            fmt_check(S, p < S->e,  "expected '}' before end of string");
            lua_pushlstring(S->L, S->p, p - S->p);
            S->p = p + 1;
            lua_gettable(S->L, to);
        }
        lua_replace(S->L, to);
    }
    return 1;
}

static int fmt_argid(fmt_State *S, int to) {
    /* [(number | identity) [accessor]] */
    int idx;
    fmt_check(S, S->p < S->e, "expected '}' before end of string");
    if (*S->p == ':' || *S->p == '}')
        lua_pushvalue(S->L, fmt_autoidx(S));
    else if (fmt_integer(S, &idx)) {
        fmt_manualidx(S);
        fmt_check(S, idx>=1 && idx <=S->top, "argument index out of range");
        lua_pushvalue(S->L, idx+1);
    } else {
        fmt_manualidx(S);
        fmt_check(S, fmt_identity(S), "unexpected '%c' in field name", *S->p);
        lua_gettable(S->L, 2);
    }
    lua_replace(S->L, to);
    return fmt_accessor(S, to);
}


/* read spec */

typedef struct fmt_Spec {
    int fill;
    int align;     /* '<', '^', '>' */
    int sign;      /* ' ', '+', '-' */
    int alter;     /* '#' */
    int zero;      /* '0' */
    int width;
    int grouping;  /* '_', ',' */
    int precision;
    int type;
} fmt_Spec;

static int fmt_readchar(fmt_State *S) {
    int ch = *S->p++;
    fmt_check(S, S->p < S->e, "unmatched '{' in format spec");
    return ch;
}

static int fmt_readint(fmt_State *S, int required, const char *name) {
    int isint, v = 0;
    if (*S->p != '{') {
        fmt_check(S, fmt_integer(S, &v) || !required,
                "Format specifier missing %s", name);
        fmt_check(S, S->p < S->e, "unmatched '{' in format spec");
    } else {
        ++S->p;
        fmt_argid(S, fmt_value(S, 2));
        fmt_check(S, *S->p == '}', "unexpected '%c' in field name", *S->p);
        ++S->p;
        v = (int)lua_tointegerx(S->L, fmt_value(S, 2), &isint);
        fmt_check(S, isint, "integer expected for %s, got %s",
                name, luaL_typename(S->L, fmt_value(S, 2)));
    }
    return v;
}

static int fmt_spec(fmt_State *S, fmt_Spec *d) {
    /* [[fill]align][sign]["#"]["0"][width][grouping]["." precision][type] */
    if (S->p[1] == '<' || S->p[1] == '>' || S->p[1] == '^')
        d->fill  = fmt_readchar(S), d->align = fmt_readchar(S);
    else if (*S->p == '<' || *S->p == '>' || *S->p == '^')
        d->align = fmt_readchar(S);
    if (*S->p == ' ' || *S->p == '+' || *S->p == '-')
        d->sign = fmt_readchar(S);
    if (*S->p == '#') d->alter = fmt_readchar(S);
    if (*S->p == '0') d->zero  = fmt_readchar(S);
    d->width = fmt_readint(S, 0, "width");
    if (*S->p == '_' || *S->p == ',') d->grouping = fmt_readchar(S);
    if (*S->p == '.') ++S->p, d->precision = fmt_readint(S, 1, "precision");
    if (*S->p != '}') {
        const char *p = S->p++;
        d->type = *p;
        if (*S->p != '}') {
            while (S->p < S->e && *S->p != '}') ++S->p;
            fmt_check(S, S->p < S->e, "unmatched '{' in format spec");
            return luaL_error(S->L, "Invalid format specifier: '%s'", p);
        }
    }
    return 1;
}


/* write spec */

#define FMT_DELIMITPOS  3
#define FMT_UTF8BUFFSIZ 8
#define FMT_FMTLEN      10 /* "%#.99f" */
#define FMT_FLTMAXPREC  100
#define FMT_INTBUFFSIZ  100
#define FMT_FLTBUFFSIZ  (10 + FMT_FLTMAXPREC + FLT_MAX_10_EXP)

static void fmt_addpadding(fmt_State *S, int ch, size_t len) {
    char *s;
    if (ch == 0) ch = ' ';
    while (len > LUAL_BUFFERSIZE) {
        s = luaL_prepbuffer(&S->B);
        memset(s, ch, LUAL_BUFFERSIZE);
        luaL_addsize(&S->B, LUAL_BUFFERSIZE);
        len -= LUAL_BUFFERSIZE;
    }
    s = luaL_prepbuffer(&S->B);
    memset(s, ch, len);
    luaL_addsize(&S->B, len);
}

static void fmt_addzeroing(fmt_State *S, const fmt_Spec *d, size_t len) {
    char *s = luaL_prepbuffer(&S->B);
    if (len > (size_t)S->zeroing) {
        int pref = (len - S->zeroing) % 4;
        if (pref > 2) *s++ = '0', luaL_addsize(&S->B, 1);
        if (pref > 0) *s++ = '0', *s++ = (char)d->grouping, luaL_addsize(&S->B, 2);
        len -= pref;
        while (len > 4) {
            size_t curr = len > LUAL_BUFFERSIZE ? LUAL_BUFFERSIZE : len;
            s = luaL_prepbuffer(&S->B);
            while (curr > 4) {
                s[0] = s[1] = s[2] = '0', s[3] = (char)d->grouping;
                s += 4, luaL_addsize(&S->B, 4), curr -= 4, len -= 4;
            }
        }
    }
    memset(s, '0', len), luaL_addsize(&S->B, len);
}

static void fmt_addstring(fmt_State *S, int shrink, size_t width, const fmt_Spec *d) {
    size_t len, plen;
    const char *s = lua_tolstring(S->L, fmt_value(S,1), &len);
    if (shrink && d->precision)
        len = len > (size_t)d->precision ? (size_t)d->precision : len;
    if (len > width) {
        lua_pushvalue(S->L, fmt_value(S,1));
        luaL_addvalue(&S->B);
        return;
    }
    plen = width - (int)len;
    switch (d->align) {
    case 0:
    case '<': !d->zero || d->grouping == 0 ?
              fmt_addpadding(S, d->fill ? d->fill : d->zero, plen) :
                  fmt_addzeroing(S, d, plen);
              luaL_addlstring(&S->B, s, len); break;
    case '>': luaL_addlstring(&S->B, s, len);
              fmt_addpadding(S, d->fill, plen); break;
    case '^': fmt_addpadding(S, d->fill, plen/2);
              luaL_addlstring(&S->B, s, len);
              fmt_addpadding(S, d->fill, plen - plen/2); break;
    }
}

static void fmt_dumpstr(fmt_State *S, const fmt_Spec *d) {
    fmt_check(S, !d->type || d->type == 's' || d->type == 'p',
            "Unknown format code '%c' for object of type 'string'", d->type);
    fmt_check(S, !d->sign,
            "Sign not allowed in string format specifier");
    fmt_check(S, !d->alter,
            "Alternate form (#) not allowed in string format specifier");
    fmt_check(S, !d->zero,
            "Zero form (0) not allowed in string format specifier");
    fmt_check(S, !d->grouping,
            "Grouping form (%c) not allowed in string format specifier",
            d->grouping);
    fmt_addstring(S, 1, d->width, d);
}

static void fmt_pushutf8(fmt_State *S, unsigned long x) {
    char buff[FMT_UTF8BUFFSIZ], *p = buff + FMT_UTF8BUFFSIZ;
    unsigned int mfb = 0x3f;
    if (x < 0x80) { lua_pushfstring(S->L, "%c", x); return; }
    do {
        *--p = (char)(0x80 | (x & 0x3f));
        x >>= 6, mfb >>= 1;
    } while (x > mfb);
    *--p = (char)((~mfb << 1) | x);
    lua_pushlstring(S->L, p, FMT_UTF8BUFFSIZ - (p-buff));
}

static void fmt_dumpchar(fmt_State *S, lua_Integer cp, const fmt_Spec *d) {
    fmt_check(S, !d->sign,
            "Sign not allowed with integer format specifier 'c'");
    fmt_check(S, !d->alter,
            "Alternate form (#) not allowed with integer format specifier 'c'");
    fmt_check(S, !d->zero,
            "Zero form (0) not allowed with integer format specifier 'c'");
    fmt_check(S, !d->grouping,
            "Cannot specify '%c' with 'c'", d->grouping);
    fmt_check(S, cp >= 0 && cp <= INT_MAX,
            "'c' arg not in range(%d)", INT_MAX);
    fmt_pushutf8(S, (unsigned long)cp);
    lua_replace(S->L, fmt_value(S,1));
    fmt_addstring(S, 0, d->width, d);
}

static int fmt_writesign(int sign, int dsign) {
    switch (dsign) {
    case '+': return sign ? '+' : '-';
    case ' ': return sign ? ' ' : '-';
    default:  return sign ?  0  : '-';
    }
}

static int fmt_writeint(char **pp, lua_Integer v, const fmt_Spec *d) {
    const char *hexa = "0123456789abcdef";
    int radix = 10, zeroing;
    char *p = *pp;
    switch (d->type) {
    case 'X': hexa = "0123456789ABCDEF"; /* FALLTHROUGH */
    case 'x':           radix = 16; break;
    case 'o': case 'O': radix =  8; break;
    case 'b': case 'B': radix =  2; break;
    }
    zeroing = d->grouping ? FMT_DELIMITPOS : 0;
    while (*--p = hexa[v % radix], v /= radix, --zeroing, v)
        if (!zeroing) zeroing = FMT_DELIMITPOS, *--p = (char)d->grouping;
    *pp = p;
    return zeroing;
}

static void fmt_dumpint(fmt_State *S, lua_Integer v, const fmt_Spec *d) {
    char buff[FMT_INTBUFFSIZ], *p = buff + FMT_INTBUFFSIZ, *dp;
    int sign = !(v < 0), width = d->width;
    if (!sign) v = -v;
    S->zeroing = fmt_writeint(&p, v, d);
    dp = p;
    if (d->alter && d->type != 0 && d->type != 'd')
        *--p = (char)d->type, *--p = '0';
    if ((p[-1] = (char)fmt_writesign(sign, d->sign)) != 0) --p;
    if (d->zero && d->width > FMT_INTBUFFSIZ - (p-buff)) {
        if (dp > p) luaL_addlstring(&S->B, p, dp - p);
        width -= (int)(dp - p), p = dp;
    }
    lua_pushlstring(S->L, p, FMT_INTBUFFSIZ - (p-buff));
    lua_replace(S->L, fmt_value(S,1));
    fmt_addstring(S, 0, width, d);
}

static int fmt_writeflt(char *s, size_t n, lua_Number v, const fmt_Spec *d) {
    int type = d->type ? d->type : 'g';
    int (*ptr_snprintf)(char *s, size_t n, const char *fmt, ...) = snprintf;
    char fmt[FMT_FMTLEN];
    const char *percent = "";
    if (d->type == '%') type = 'f', v *= 100.0, percent = "%%";
    if (d->precision)
        ptr_snprintf(fmt, FMT_FMTLEN, "%%%s.%d%c%s",
                d->alter ? "#" : "", d->precision, type, percent);
    else if ((lua_Number)(lua_Integer)v == v)
        ptr_snprintf(fmt, FMT_FMTLEN, "%%.1f%s", percent);
    else
        ptr_snprintf(fmt, FMT_FMTLEN, "%%%s%c%s",
                d->alter ? "#" : "", type, percent);
    return ptr_snprintf(s, n, fmt, v);
}

static void fmt_dumpflt(fmt_State *S, lua_Number v, const fmt_Spec *d) {
    int sign = !(v < 0), len, width = d->width;
    char buff[FMT_FLTBUFFSIZ], *p = buff, *dp = p;
    fmt_check(S, d->precision < FMT_FLTMAXPREC,
            "precision specifier too large");
    fmt_check(S, !d->grouping,
            "Grouping form (%c) not allowed in float format specifier",
            d->grouping);
    if (!sign) v = -v;
    if ((*dp = (char)fmt_writesign(sign, d->sign)) != 0) ++dp;
    len = fmt_writeflt(dp, FMT_FLTBUFFSIZ - (dp-buff), v, d);
    if (d->zero && width > len) {
        if (dp > p) luaL_addlstring(&S->B, buff, dp - p);
        width -= (int)(dp - buff), p = dp;
    }
    lua_pushlstring(S->L, p, len);
    lua_replace(S->L, fmt_value(S,1));
    fmt_addstring(S, 0, width, d);
}

static void fmt_dumpnumber(fmt_State *S, const fmt_Spec *d) {
    int type = d->type;
    if (type == 0) type = lua_isinteger(S->L, fmt_value(S,1)) ? 'd' : 'g';
    switch (type) {
    case 'c':
        fmt_dumpchar(S, lua_tointeger(S->L, fmt_value(S,1)), d); break;
    case 'd': case 'b': case 'B': case 'o': case 'O': case 'x': case 'X':
        fmt_dumpint(S, lua_tointeger(S->L, fmt_value(S,1)), d); break;
    case 'e': case 'E': case 'f': case 'F': case 'g': case 'G': case '%':
        fmt_dumpflt(S, lua_tonumber(S->L, fmt_value(S,1)), d); break;
    default:
        luaL_error(S->L, "Unknown format code '%c' for object of type 'number'",
                      d->type);
    }
}

static void fmt_dump(fmt_State *S, const fmt_Spec *d) {
    int type = lua_type(S->L, fmt_value(S,1));
    if (type == LUA_TNUMBER) { fmt_dumpnumber(S, d); return; }
    if (d->type != 'p')
        luaL_tolstring(S->L, fmt_value(S,1), NULL);
    else {
        fmt_check(S, type != LUA_TNIL && type != LUA_TBOOLEAN,
                "Unknown format code '%c' for object of type '%s'",
                d->type, lua_typename(S->L, type));
        lua_pushfstring(S->L, "%p", lua_topointer(S->L, fmt_value(S,1)));
    }
    lua_replace(S->L, fmt_value(S,1));
    fmt_dumpstr(S, d);
}


/* format */

static void fmt_parse(fmt_State *S, fmt_Spec *d) {
    /* "{" [arg_id] [":" format_spec] "}" */
    fmt_argid(S, fmt_value(S, 1));
    if (*S->p == ':' && ++S->p < S->e)
        fmt_spec(S, d);
    fmt_check(S, S->p < S->e && *S->p == '}',
            "expected '}' before end of string");
    ++S->p;
}

static int fmt_format(fmt_State *S) {
    lua_settop(S->L, fmt_value(S, 2));
    luaL_buffinit(S->L, &S->B);
    while (S->p < S->e) {
        const char *p = S->p;
        while (p < S->e && *p != '{' && *p != '}') ++p;
        luaL_addlstring(&S->B, S->p, p - S->p), S->p = p;
        if (S->p >= S->e) break;
        if (*S->p == S->p[1])
            luaL_addchar(&S->B, *S->p), S->p += 2;
        else {
            fmt_Spec d;
            if (*S->p++ == '}' || S->p >= S->e)
                return luaL_error(S->L,
                    "Single '%c' encountered in format string", S->p[-1]);
            memset(&d, 0, sizeof(d));
            fmt_parse(S, &d);
            fmt_dump(S, &d);
        }
    }
    luaL_pushresult(&S->B);
    return 1;
}

static int Lformat(lua_State *L) {
    size_t len;
    fmt_State S;
    S.p   = luaL_checklstring(L, 1, &len);
    S.e   = S.p + len;
    S.L   = L;
    S.idx = 1;
    S.top = lua_gettop(L);
    return fmt_format(&S);
}

LUALIB_API int luaopen_fmt(lua_State *L) {
    lua_pushcfunction(L, Lformat);
    return 1;
}

/* cc: flags+='-O3 --coverage -pedantic'
 * unixcc: flags+='-shared -fPIC' output='fmt.so'
 * maccc: flags+='-undefined dynamic_lookup'
 * win32cc: flags+='-s -mdll -DLUA_BUILD_AS_DLL '
 * win32cc: libs+='-llua54' output='fmt.dll' */

