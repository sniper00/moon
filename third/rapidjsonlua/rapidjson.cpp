#include <limits>
#include <cstdio>
#include <vector>
#include <algorithm>

#include <lua.hpp>

// __SSE2__ and __SSE4_2__ are recognized by gcc, clang, and the Intel compiler.
// We use -march=native with gmake to enable -msse2 and -msse4.2, if supported.
#if defined(__SSE4_2__)
#  define RAPIDJSON_SSE42
#elif defined(__SSE2__)
#  define RAPIDJSON_SSE2
#endif

#define RAPIDJSON_SSE42

#include "rapidjson/document.h"
#include "rapidjson/encodedstream.h"
#include "rapidjson/error/en.h"
#include "rapidjson/error/error.h"
#include "rapidjson/filereadstream.h"
#include "rapidjson/filewritestream.h"
#include "rapidjson/prettywriter.h"
#include "rapidjson/rapidjson.h"
#include "rapidjson/reader.h"
#include "rapidjson/schema.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"


#include "Userdata.hpp"
#include "values.hpp"
#include "luax.hpp"
#include "file.hpp"

using namespace rapidjson;

#ifndef LUA_RAPIDJSON_VERSION
#define LUA_RAPIDJSON_VERSION "scm"
#endif


static void createSharedMeta(lua_State* L, const char* meta, const char* type)
{
	luaL_newmetatable(L, meta); // [meta]
	lua_pushstring(L, type); // [meta, name]
	lua_setfield(L, -2, "__jsontype"); // [meta]
	lua_pop(L, 1); // []
}

static int makeTableType(lua_State* L, int idx, const char* meta, const char* type)
{
	auto isnoarg = lua_isnoneornil(L, idx);
	auto istable = lua_istable(L, idx);
	if (!isnoarg && !istable)
		return luaL_argerror(L, idx, "optional table excepted");

	if (isnoarg)
		lua_createtable(L, 0, 0); // [table]
	else // is table.
	{
		lua_pushvalue(L, idx); // [table]
		if (lua_getmetatable(L, -1))
		{
			// already have a metatable, just set the __jsontype field.
			// [table, meta]
			lua_pushstring(L, type); // [table, meta, name]
			lua_setfield(L, -2, "__jsontype"); // [table, meta]
			lua_pop(L, 1); // [table]
			return 1;
		}
		// else fall through
	}

	// Now we have a table without meta table
	luaL_getmetatable(L, meta); // [table, meta]
	lua_setmetatable(L, -2); // [table]
	return 1;
}

static int json_object(lua_State* L)
{
	return makeTableType(L, 1, "json.object", "object");
}

static int json_array(lua_State* L)
{
	return makeTableType(L, 1, "json.array", "array");
}


template<typename Stream>
int decode(lua_State* L, Stream* s)
{
	auto top = lua_gettop(L);
	values::ToLuaHandler handler(L);
	Reader reader;
	ParseResult r = reader.Parse(*s, handler);

	if (!r) {
		lua_settop(L, top);
		lua_pushnil(L);
		lua_pushfstring(L, "%s (%d)", GetParseError_En(r.Code()), r.Offset());
		return 2;
	}

	return 1;
}

static int json_decode(lua_State* L)
{
	size_t len = 0;
	auto contents = luaL_checklstring(L, 1, &len);
	StringStream s(contents);
	return decode(L, &s);
}



static int json_load(lua_State* L)
{
	auto filename = luaL_checklstring(L, 1, NULL);
	auto fp = file::open(filename, "rb");
	if (fp == nullptr)
		luaL_error(L, "error while open file: %s", filename);

	char buffer[512];
	FileReadStream fs(fp, buffer, sizeof(buffer));
	AutoUTFInputStream<unsigned, FileReadStream> eis(fs);

	auto n = decode(L, &eis);

	fclose(fp);
	return n;
}

struct Key
{
	Key(const char* k, SizeType l) : key(k), size(l) {}
	bool operator<(const Key& rhs) const {
		return strcmp(key, rhs.key) < 0;
	}
	const char* key;
	SizeType size;
};




class Encoder {
	bool pretty;
	bool sort_keys;
	int max_depth;
	static const int MAX_DEPTH_DEFAULT = 128;
public:
	Encoder(lua_State*L, int opt) : pretty(false), sort_keys(false), max_depth(MAX_DEPTH_DEFAULT)
	{
		if (lua_isnoneornil(L, opt))
			return;
		luaL_checktype(L, opt, LUA_TTABLE);

		pretty = luax::optboolfield(L, opt, "pretty", false);
		sort_keys = luax::optboolfield(L, opt, "sort_keys", false);
		max_depth = luax::optintfield(L, opt, "max_depth", MAX_DEPTH_DEFAULT);
	}

private:
	template<typename Writer>
	void encodeValue(lua_State* L, Writer* writer, int idx, int depth = 0)
	{
		size_t len;
		const char* s;
		int64_t integer;
		auto t = lua_type(L, idx);
		switch (t) {
		case LUA_TBOOLEAN:
			writer->Bool(lua_toboolean(L, idx) != 0);
			return;
		case LUA_TNUMBER:
			if (luax::isinteger(L, idx, &integer))
				writer->Int64(integer);
			else {
				if (!writer->Double(lua_tonumber(L, idx)))
					luaL_error(L, "error while encode double value.");
			}
			return;
		case LUA_TSTRING:
			s = lua_tolstring(L, idx, &len);
			writer->String(s, static_cast<SizeType>(len));
			return;
		case LUA_TTABLE:
			return encodeTable(L, writer, idx, depth + 1);
		case LUA_TNIL:
			writer->Null();
			return;
		case LUA_TFUNCTION:
			if (values::isnull(L, idx)) {
				writer->Null();
				return;
			}
			// otherwise fall thought
		case LUA_TLIGHTUSERDATA: // fall thought
		case LUA_TUSERDATA: // fall thought
		case LUA_TTHREAD: // fall thought
		case LUA_TNONE: // fall thought
		default:
			luaL_error(L, "value type : %s", lua_typename(L, t));
		}
	}

	template<typename Writer>
	void encodeTable(lua_State* L, Writer* writer, int idx, int depth)
	{
		if (depth > max_depth)
			luaL_error(L, "nested too depth");

		if (!lua_checkstack(L, 4)) // requires at least 4 slots in stack: table, key, value, key
			luaL_error(L, "stack overflow");

		lua_pushvalue(L, idx); // [table]
		if (values::isarray(L, -1))
		{
			encodeArray(L, writer, depth);
			lua_pop(L, 1); // []
			return;
		}

		// is object.
		if (!sort_keys)
		{
			encodeObject(L, writer, depth);
			lua_pop(L, 1); // []
			return;
		}

		lua_pushnil(L); // [table, nil]
		std::vector<Key> keys;

		while (lua_next(L, -2))
		{
			// [table, key, value]

			if (lua_type(L, -2) == LUA_TSTRING)
			{
				size_t len = 0;
				auto key = lua_tolstring(L, -2, &len);
				keys.push_back(Key(key, static_cast<SizeType>(len)));
			}

			// pop value, leaving original key
			lua_pop(L, 1);
			// [table, key]
		}
		// [table]
		encodeObject(L, writer, depth, keys);
		lua_pop(L, 1);
	}

	template<typename Writer>
	void encodeObject(lua_State* L, Writer* writer, int depth)
	{
		writer->StartObject();

		// [table]
		lua_pushnil(L); // [table, nil]
		while (lua_next(L, -2))
		{
			// [table, key, value]
			if (lua_type(L, -2) == LUA_TSTRING)
			{
				size_t len = 0;
				auto key = lua_tolstring(L, -2, &len);
				writer->Key(key, static_cast<SizeType>(len));
				encodeValue(L, writer, -1, depth);
			}

			// pop value, leaving original key
			lua_pop(L, 1);
			// [table, key]
		}
		// [table]
		writer->EndObject();
	}

	template<typename Writer>
	void encodeObject(lua_State* L, Writer* writer, int depth, std::vector<Key> &keys)
	{
		// [table]
		writer->StartObject();

		std::sort(keys.begin(), keys.end());

		std::vector<Key>::const_iterator i = keys.begin();
		std::vector<Key>::const_iterator e = keys.end();
		for (; i != e; ++i)
		{
			writer->Key(i->key, static_cast<SizeType>(i->size));
			lua_pushlstring(L, i->key, i->size); // [table, key]
			lua_gettable(L, -2); // [table, value]
			encodeValue(L, writer, -1, depth);
			lua_pop(L, 1); // [table]
		}
		// [table]
		writer->EndObject();
	}

	template<typename Writer>
	void encodeArray(lua_State* L, Writer* writer, int depth)
	{
		// [table]
		writer->StartArray();
		auto MAX = static_cast<int>(luax::rawlen(L, -1)); // lua_rawlen always returns value >= 0
		for (auto n = 1; n <= MAX; ++n)
		{
			lua_rawgeti(L, -1, n); // [table, element]
			encodeValue(L, writer, -1, depth);
			lua_pop(L, 1); // [table]
		}
		writer->EndArray();
		// [table]
	}

public:
	template<typename Stream>
	void encode(lua_State* L, Stream* s, int idx)
	{
		if (pretty)
		{
			PrettyWriter<Stream> writer(*s);
			encodeValue(L, &writer, idx);
		}
		else
		{
			Writer<Stream> writer(*s);
			encodeValue(L, &writer, idx);
		}
	}
};


static int json_encode(lua_State* L)
{
	try{
		Encoder encode(L, 2);
		StringBuffer s;
		encode.encode(L, &s, 1);
		lua_pushlstring(L, s.GetString(), s.GetSize());
		return 1;
	}
	catch (...) {
		luaL_error(L, "error while encoding");
	}
	return 0;
}


static int json_dump(lua_State* L)
{
	Encoder encoder(L, 3);

	auto filename = luaL_checkstring(L, 2);
	auto fp = file::open(filename, "wb");
	if (fp == nullptr)
		luaL_error(L, "error while open file: %s", filename);

	char buffer[512];
	FileWriteStream fs(fp, buffer, sizeof(buffer));
	encoder.encode(L, &fs, 1);
	fclose(fp);
	return 0;
}


namespace values {
	static auto nullref = LUA_NOREF;
	/**
	* Returns rapidjson.null.
	*/
	int json_null(lua_State* L)
	{
		lua_rawgeti(L, LUA_REGISTRYINDEX, nullref);
		return 1;
	}
}

static const luaL_Reg methods[] = {
	// string <--> lua table
	{ "decode", json_decode },
	{ "encode", json_encode },

	// file <--> lua table
	{ "load", json_load },
	{ "dump", json_dump },

	// special tags and functions
	{ "null", values::json_null },
	{ "object", json_object },
	{ "array", json_array },

	// JSON types
	{ "Document", Userdata<Document>::create },
	{ "SchemaDocument", Userdata<SchemaDocument>::create },
	{ "SchemaValidator", Userdata<SchemaValidator>::create },

	{nullptr, nullptr }
};

/*-------------------------------------------------------------------------*\
* This macro prefixes all exported API functions
\*-------------------------------------------------------------------------*/
#ifndef LUARAPIDJSON_EXPORT
#define LUARAPIDJSON_API extern
#else
#define LUARAPIDJSON_API __declspec(dllexport)
#endif

extern "C" {

LUARAPIDJSON_API int luaopen_rapidjson(lua_State* L)
{
    luaL_checkversion(L);

	lua_newtable(L); // [rapidjson]

	luax::setfuncs(L, methods); // [rapidjson]

	lua_pushliteral(L, "rapidjson"); // [rapidjson, name]
	lua_setfield(L, -2, "_NAME"); // [rapidjson]

	lua_pushliteral(L, LUA_RAPIDJSON_VERSION); // [rapidjson, version]
	lua_setfield(L, -2, "_VERSION"); // [rapidjson]

	lua_getfield(L, -1, "null"); // [rapidjson, json.null]
	values::nullref = luaL_ref(L, LUA_REGISTRYINDEX); // [rapidjson]


	createSharedMeta(L, "json.object", "object");
	createSharedMeta(L, "json.array", "array");

	Userdata<Document>::luaopen(L);
	Userdata<SchemaDocument>::luaopen(L);
	Userdata<SchemaValidator>::luaopen(L);

	return 1;
}

}
