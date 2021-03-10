#pragma once
#include <type_traits>
namespace rapidjson
{
    using json_value_pointer = rapidjson::Value*;

    namespace detail
    {
        template<typename T>
        struct  is_vector :std::false_type {};

        template<typename T>
        struct  is_vector<std::vector<T>> :std::true_type {};

        template<typename T>
        constexpr bool is_vector_v = is_vector<T>::value;

        template<typename ValueType>
        inline ValueType get_value(json_value_pointer value, const ValueType& dv = ValueType())
        {
            (void)dv;
            using value_type = std::decay_t<ValueType>;
            if constexpr (std::is_same_v<value_type, bool>)
            {
                assert(value->IsBool());
                return value->GetBool();
            }
            else if constexpr (std::is_integral_v<value_type>)
            {
                if constexpr (std::is_signed_v<value_type>)
                {
                    assert(value->IsInt64());
                    return static_cast<ValueType>(value->GetInt64());
                }
                else
                {
                    assert(value->IsUint64());
                    return static_cast<ValueType>(value->GetUint64());
                }
            }
            else if constexpr (std::is_floating_point_v<value_type>)
            {
                assert(value->IsDouble());
                return static_cast<ValueType>(value->GetDouble());
            }
            else if constexpr (std::is_same_v<value_type, std::string>)
            {
                assert(value->IsString());
                return std::string{ value->GetString(), value->GetStringLength() };
            }
            else if constexpr (std::is_same_v<value_type, std::string_view>)
            {
                assert(value->IsString());
                return std::string_view{ value->GetString(), value->GetStringLength() };
            }
            else if constexpr (is_vector_v<value_type>)
            {
                using vector_type = value_type;
                using Ty = typename vector_type::value_type;
                assert(value->IsArray());
                Ty vec;
                auto arr = value->GetArray();
                for (auto& v : arr)
                {
                    vec.emplace_back(get_value<Ty>(&v, Ty{}));
                }
                return vec;
            }
            else if constexpr (std::is_same_v<rapidjson::Value*, value_type>)
            {
                return value;
            }
            else
            {
                return dv;
            }
        }
    }

    template<typename ValueType>
    inline ValueType get_value(json_value_pointer value, std::string_view path, const ValueType& dv = ValueType())
    {
        auto vecs = moon::split<std::string_view>(path, ".");
        for (const auto& v : vecs)
        {
            auto iter = value->FindMember(rapidjson::Value{ v.data(), static_cast<SizeType>(v.size()) });
            if (iter != value->MemberEnd())
            {
                value = &iter->value;
            }
            else
            {
                return dv;
            }
        }
        return detail::get_value(value, dv);
    }
}
