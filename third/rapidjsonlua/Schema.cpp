#include <lua.hpp>

#include <rapidjson/document.h>
#include <rapidjson/error/en.h>
#include <rapidjson/istreamwrapper.h>
#include <rapidjson/schema.h>
#include <rapidjson/stringbuffer.h>

#include "Userdata.hpp"
#include "values.hpp"

using namespace rapidjson;

template<>
const char* const Userdata<SchemaDocument>::metatable = "rapidjson.SchemaDocument";

template<>
SchemaDocument* Userdata<SchemaDocument>::construct(lua_State * L)
{
	switch (lua_type(L, 1)) {
	case LUA_TNONE:
		return new SchemaDocument(Document()); // empty schema
	case LUA_TSTRING: {
		auto d = Document();
		size_t len = 0;
		auto s = lua_tolstring(L, 1, &len);
		d.Parse(s, len);
		return new SchemaDocument(d);
	}
	case LUA_TTABLE: {
		auto doc = Document();
		values::toDocument(L, 1, &doc);
		return new SchemaDocument(doc);
	}
	case LUA_TUSERDATA: {
		auto doc = Userdata<Document>::check(L, 1);
		return new SchemaDocument(*doc);
	}
	default:
		luax::typerror(L, 1, "none, string, table or rapidjson.Document");
		return nullptr; // Just make compiler happy
	}
}



template <>
const luaL_Reg* Userdata<SchemaDocument>::methods() {
	static const luaL_Reg reg[] = {
		{ "__gc", metamethod_gc },

		{ nullptr, nullptr }
	};
	return reg;
}


template<>
const char* const Userdata<SchemaValidator>::metatable = "rapidjson.SchemaValidator";

template<>
SchemaValidator* Userdata<SchemaValidator>::construct(lua_State * L)
{
	auto sd = Userdata<SchemaDocument>::check(L, 1);
	return new SchemaValidator(*sd);
}


static void pushValidator_error(lua_State* L, SchemaValidator* validator) {
	luaL_Buffer b;
	luaL_buffinit(L, &b);

	luaL_addstring(&b, "invalid \"");

	luaL_addstring(&b, validator->GetInvalidSchemaKeyword());
	luaL_addstring(&b, "\" in docuement at pointer \"");

	// docuement pointer
	StringBuffer sb;
	validator->GetInvalidDocumentPointer().StringifyUriFragment(sb);
	luaL_addlstring(&b, sb.GetString(), sb.GetSize());
	luaL_addchar(&b, '"');

	luaL_pushresult(&b);
}


static int SchemaValidator_validate(lua_State* L) {
	auto validator = Userdata<SchemaValidator>::check(L, 1);
	auto doc = Userdata<Document>::check(L, 2);
	auto ok = doc->Accept(*validator);
	lua_pushboolean(L, ok);
	int nr;
	if (ok)
		nr = 1;
	else
	{
		pushValidator_error(L, validator);
		nr = 2;
	}
	validator->Reset();

	return nr;
}


template <>
const luaL_Reg* Userdata<SchemaValidator>::methods() {
	static const luaL_Reg reg[] = {
		{ "__gc", metamethod_gc },
		{ "validate", SchemaValidator_validate },
		{ nullptr, nullptr }
	};
	return reg;
}
