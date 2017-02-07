#pragma once
#include <type_traits>
#include <string>
#include <vector>
#include <cstdint>
#include "Value.hpp"

enum class NativeType
{
	NONE,
	UINT8,
	INT8,
	UINT16,
	INT16,
	UINT32,
	INT32,
	UINT64,
	INT64,
	FLOAT,
	DOUBLE,
	STRING
};

struct BindData
{
	BindData()
		:Type(NativeType::NONE),Data(nullptr),Size(0), Index(0)
	{
	}

	void*						Data;
	size_t						Size;
	NativeType				Type;
	int							Index;
	DataValue               Value;
};

inline BindData ToBindData(std::string& str)
{
	BindData ret;
	ret.Type = NativeType::STRING;
	ret.Size = (uint32_t)str.size();
	ret.Data = (void*)str.data();
	return ret;
}

inline BindData ToBindData(const char*& sz)
{
	std::string str(sz);
	BindData ret;
	ret.Type = NativeType::STRING;
	ret.Size = (uint32_t)str.size();
	ret.Data = (void*)sz;
	return ret;
}

template<class T>
BindData ToBindData(T& t)
{
	BindData ret;

	if (std::is_same<T, int64_t>::value)
	{
		ret.Type = NativeType::INT64;
	}
	else 	if (std::is_same<T, uint64_t>::value)
	{
		ret.Type = NativeType::UINT64;
	}
	else if (std::is_same<T, int32_t>::value)
	{
		ret.Type = NativeType::INT32;
	}
	else 	if (std::is_same<T, uint32_t>::value)
	{
		ret.Type = NativeType::UINT32;
	}
	else if (std::is_same<T, int16_t>::value)
	{
		ret.Type = NativeType::INT16;
	}
	else 	if (std::is_same<T, uint16_t>::value)
	{
		ret.Type = NativeType::UINT16;
	}
	else if (std::is_same<T, int8_t>::value)
	{
		ret.Type = NativeType::INT8;
	}
	else 	if (std::is_same<T, uint8_t>::value || std::is_same<T, bool>::value)
	{
		ret.Type = NativeType::UINT8;
	}
	else if (std::is_same<T, nullptr_t>::value)
	{
		ret.Type = NativeType::NONE;
	}
	else if (std::is_same<T, float>::value)
	{
		ret.Type = NativeType::FLOAT;
	}
	else if (std::is_same<T, double>::value)
	{
		ret.Type = NativeType::DOUBLE;
	}
	ret.Size = 0;
	ret.Data = (void*)&t;
	return ret;
}

//µÝ¹é½áÊøÌõ¼þ
inline void BindParams(std::vector<BindData>& bindDatas)
{

}

template<typename T, typename... Args>
void BindParams(std::vector<BindData>& bindDatas,T&& first, Args&&... args)
{
	bindDatas.emplace_back(ToBindData(first));
	BindParams(bindDatas,std::forward<Args>(args)...);
}