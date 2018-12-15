#pragma once
#include <type_traits>
namespace rapidjson
{
	namespace detail
	{
        template<typename T>
        struct  is_vector :std::false_type {};

        template<typename T>
        struct  is_vector<std::vector<T>> :std::true_type {};

        template<typename T>
        using is_vector_t = typename is_vector<T>::type;

		template<typename T, std::enable_if_t<!is_vector<std::decay_t<T>>::value, int> = 0>
        inline T get_value(rapidjson::Value* value, const T& dv = T());

        using json_value_pointer = rapidjson::Value*;

        template<>
        inline json_value_pointer get_value<json_value_pointer>(rapidjson::Value* value, const json_value_pointer& dv)
        {
			(void)dv;
            return value;
        }

        template<>
        inline bool get_value<bool>(rapidjson::Value* value, const bool& dv)
        {
            if (value->IsBool())
            {
                return value->GetBool();
            }
            return dv;
        }

		template<>
        inline int64_t get_value<int64_t>(rapidjson::Value* value, const int64_t& dv)
		{
			if (value->IsInt64())
			{
				return value->GetInt64();
			}
			return dv;
		}

		template<>
        inline int32_t get_value<int32_t>(rapidjson::Value* value, const int32_t& dv)
		{
			if (value->IsInt())
			{
				return value->GetInt();
			}
			return dv;
		}

		template<>
        inline double get_value<double>(rapidjson::Value* value, const double& dv)
		{
			if (value->IsDouble())
			{
				return value->GetDouble();
			}
			return dv;
		}

		template<>
        inline std::string get_value<std::string>(rapidjson::Value* value, const std::string& dv)
		{
			if (value->IsString())
			{
				return std::string(value->GetString(),value->GetStringLength());
			}
			return dv;
		}

        template<>
        inline moon::string_view_t get_value<moon::string_view_t>(rapidjson::Value* value, const moon::string_view_t& dv)
        {
            if (value->IsString())
            {
                return moon::string_view_t(value->GetString(), value->GetStringLength());
            }
            return dv;
        }

        template<typename T,std::enable_if_t<is_vector<std::decay_t<T>>::value,int> =0 >
        inline T get_value(rapidjson::Value* value, const T& dv)
        {
            using vector_type = std::decay_t <T>;
            using value_type = typename vector_type::value_type;
            if (value->IsArray())
            {
                vector_type vec;
                auto arr = value->GetArray();
                for (auto& v : arr)
                {
                    vec.emplace_back(get_value<value_type>(&v, value_type{}));
                }
                return std::move(vec);
            }
            return dv;
        }
	}

	template<typename T>
    inline T get_value(rapidjson::Value* value, moon::string_view_t path, const T& dv = T())
	{
        auto vecs = moon::split<moon::string_view_t>(path, ".");
		for (auto& v : vecs)
		{
            auto iter = value->FindMember(rapidjson::Value::ValueType(v.data(), static_cast<rapidjson::SizeType>(v.size())));
            if (iter != value->MemberEnd())
            {
                value = &iter->value;
            }
            else
            {
				return dv;
			}
		}
		return detail::get_value<T>(value, dv);
	}
}
