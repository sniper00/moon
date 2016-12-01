#pragma once
#include <cstdint>
#include <string>
#include <memory>
#include <cassert>

enum class DataType :uint8_t
{
	null,
	boolean,
	number_integer,
	number_double,
	string
};

class DataValue
{
public:
	union  Data
	{
		Data()
			:value_string(nullptr)
		{

		}
		int64_t			value_integer;
		double			value_double;
		bool				value_bool;
		std::string*  value_string;
		nullptr_t		value_null;
	};

	DataValue()
		:m_Type(DataType::null), m_IsUnsigned(true)
	{
	}

	DataValue(const DataValue& other)
	{
		*this = other;
	}

	DataValue(DataValue&& value)
	{
		*this = std::move(value);
	}

	DataValue& operator=(const DataValue& other)
	{
		if (this == &other)
			return *this;

		Clear();
		m_Type = other.m_Type;
		switch (other.m_Type)
		{
		case DataType::null:
			m_data.value_null = other.m_data.value_null;
			break;
		case DataType::number_integer:
			m_data.value_integer = other.m_data.value_integer;
			break;
		case DataType::number_double:
			m_data.value_double = other.m_data.value_double;
			break;
		case DataType::boolean:
			m_data.value_bool = other.m_data.value_bool;
			break;
		case DataType::string:
			if (m_data.value_string == nullptr)
			{
				m_data.value_string = new std::string;
			}
			*m_data.value_string = *other.m_data.value_string;
			break;
		default:
			break;
		}
		return *this;
	}


	DataValue& operator=(DataValue&& other)
	{
		if (this == &other)
			return *this;

		Clear();
		m_Type = other.m_Type;
		switch (other.m_Type)
		{
		case DataType::null:
			m_data.value_null = other.m_data.value_null;
			break;
		case DataType::number_integer:
			m_data.value_integer = other.m_data.value_integer;
			break;
		case DataType::number_double:
			m_data.value_double = other.m_data.value_double;
			break;
		case DataType::boolean:
			m_data.value_bool = other.m_data.value_bool;
			break;
		case DataType::string:
			m_data.value_string = other.m_data.value_string;
			break;
		default:
			break;
		}
		memset(&other.m_data, 0, sizeof(other.m_data));
		other.m_Type = DataType::null;
		return *this;
	}

	template<class T>
	DataValue& operator=(T t)
	{
		SetValue(t);
		return *this;
	}

	void SetValue(const char* value)
	{
		if (m_Type != DataType::string)
		{
			m_data.value_string = new std::string("");
			m_Type = DataType::string;
		}
		*m_data.value_string = value;
	}

	void SetValue(const std::string& value)
	{
		if (m_Type != DataType::string)
		{
			m_data.value_string = new std::string("");
			m_Type = DataType::string;
		}
		*m_data.value_string = value;
	}

	void SetValue(bool value)
	{
		Clear();

		m_Type = DataType::boolean;
		m_data.value_bool = value;
	}

	void SetValue(nullptr_t v)
	{
		Clear();

		m_Type = DataType::null;
		m_data.value_null = nullptr;
	}

	template<typename T>
	void SetValue(T t, typename std::enable_if<std::is_floating_point< T>::value>::type* = nullptr)
	{
		Clear();

		if (std::is_floating_point<T>::value)
		{
			m_Type = DataType::number_double;
			m_data.value_double = t;
		}
	}

	template<typename T>
	void SetValue(T t, typename  std::enable_if<std::is_integral< T>::value>::type* = nullptr)
	{
		Clear();

		if (std::is_integral<T>::value)
		{
			m_Type = DataType::number_integer;
			m_data.value_integer = t;
		}

		m_IsUnsigned = !std::is_signed<T>::value;
	}

	template<typename T>
	typename std::enable_if<std::is_same<T, std::string>::value, std::string>::type&
		Cast()
	{
		if (m_Type != DataType::string)
		{
			throw std::bad_cast();
		}
		return *m_data.value_string;
	}

	template<typename T>
	typename std::enable_if<std::is_same<T, bool>::value, bool>::type
		Cast()
	{
		if (m_Type != DataType::boolean)
		{
			throw std::bad_cast();
		}
		return m_data.value_bool;
	}

	template<typename T>
	typename std::enable_if<std::is_integral<T>::value && !std::is_same<T, bool>::value, T>::type
		Cast()
	{
		if (m_Type != DataType::number_integer)
		{
			throw std::bad_cast();
		}

		if (m_IsUnsigned != !std::is_signed<T>::value)
		{
			throw std::bad_cast();
		}

		return static_cast<T>(m_data.value_integer);
	}

	template<typename T>
	typename std::enable_if<std::is_floating_point<T>::value, T>::type
		Cast()
	{
		if (m_Type != DataType::number_double)
		{
			throw std::bad_cast();
		}
		return static_cast<T>(m_data.value_double);
	}

	template<typename T>
	typename std::enable_if<std::is_same<T, nullptr_t>::value, T>::type
		Cast()
	{
		if (m_Type != DataType::null)
		{
			throw std::bad_cast();
		}
		return static_cast<T>(m_data.value_null);
	}

	~DataValue()
	{
		if (m_Type == DataType::string)
		{
			delete m_data.value_string;
		}
	}

	std::string ToString()
	{
		switch (m_Type)
		{
		case DataType::null:
			return "";
		case DataType::number_integer:
			return std::to_string(m_data.value_integer);
		case DataType::number_double:
			return std::to_string(m_data.value_double);
		case DataType::boolean:
			return m_data.value_bool?"true":"false";
		case DataType::string:
			return *m_data.value_string;
			break;
		default:
			break;
		}
		return "";
	}

private:
	void Clear()
	{
		switch (m_Type)
		{
		case DataType::null:
			m_data.value_null = nullptr;
			break;
		case DataType::number_integer:
			m_data.value_integer = 0;
			break;
		case DataType::number_double:
			m_data.value_double = 0.0;
			break;
		case DataType::boolean:
			m_data.value_bool = false;
			break;
		case DataType::string:
			if (m_data.value_string != nullptr)
			{
				delete m_data.value_string;
				m_data.value_string = nullptr;
			}
			break;
		default:
			break;
		}
	}

private:
	DataType		 m_Type;
	Data				 m_data;
	bool				 m_IsUnsigned;
};

