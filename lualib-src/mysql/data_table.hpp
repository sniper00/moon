#pragma once
#include <vector>
#include <variant>
#include <cassert>
#include <string>
#include <tuple>
#include <string_view>

namespace db
{
	template <class T>
	struct type_id_map
	{
		static const int value = 0;
	};

	template<int>
	struct id_type_map
	{
		using type = void;
	};

#define REGISTER_TYPE(T, Index)\
	template<>\
	struct type_id_map<T>\
	{\
		static const int value = Index;\
	}; \
\
	template<>\
	struct id_type_map<Index>\
	{\
		using type = T;\
	};\


	REGISTER_TYPE(nullptr_t, MYSQL_TYPE_NULL)
	REGISTER_TYPE(char, MYSQL_TYPE_TINY)
	REGISTER_TYPE(short, MYSQL_TYPE_SHORT)
	REGISTER_TYPE(int, MYSQL_TYPE_LONG)
	REGISTER_TYPE(float, MYSQL_TYPE_FLOAT)
	REGISTER_TYPE(double, MYSQL_TYPE_DOUBLE)
	REGISTER_TYPE(int64_t, MYSQL_TYPE_LONGLONG)
	REGISTER_TYPE(std::string, MYSQL_TYPE_VAR_STRING)
	

	template<class T>
	constexpr bool is_char_array_v = std::is_array_v<T>&&std::is_same_v<char, std::remove_pointer_t<std::decay_t<T>>>;

	using db_data_value_t = std::variant<nullptr_t, char, int, float, double, int64_t, std::string>;

	class data_table
	{
	public:
		using row_t = std::vector<db_data_value_t>;
		using rows_t = std::vector<row_t>;
		using data_type = rows_t;
		using iterator = data_type::iterator;
		using const_iterator = data_type::const_iterator;
		using difference_type = data_type::difference_type;
		using size_type = data_type::size_type;

		const_iterator begin() const noexcept
		{
			return rows_.begin();
		}

		iterator begin() noexcept
		{
			return rows_.begin();
		}

		const_iterator end() const noexcept
		{
			return rows_.end();
		}

		iterator end() noexcept
		{
			return rows_.end();
		}

		template<typename Name, typename Type>
		void add_column(Name&& colname, Type&& data_type)
		{
			cols_.emplace_back(std::forward<Name>(colname), std::forward<Type>(data_type));
		}

		template<typename Row>
		void add_row(Row&& rowdata)
		{
			row_t row;
			row.reserve(cols_.size());
			int i = 0;
			for (auto& col : cols_)
			{
				switch (col.second)
				{
				case MYSQL_TYPE_TINY:
				case MYSQL_TYPE_SHORT:
				case MYSQL_TYPE_LONG:
					row.emplace_back(std::stoi(rowdata[i]));
					break;
				case MYSQL_TYPE_FLOAT:
					row.emplace_back(std::stof(rowdata[i]));
					break;
				case MYSQL_TYPE_DOUBLE:
					row.emplace_back(std::stod(rowdata[i]));
					break;
				case MYSQL_TYPE_LONGLONG:
					row.emplace_back(static_cast<int64_t>(std::stoll(rowdata[i])));
					break;
				case MYSQL_TYPE_VAR_STRING:
					row.emplace_back(std::string(rowdata[i]));
					break;
				default:
					break;
				}
				i++;
			}
			rows_.emplace_back(std::move(row));
		}

		template<typename T>
		T& get(size_t row, size_t col)
		{
			assert(row < rows_.size() && col < cols_.size());
			return std::get<T>(rows_.at(row).at(col));
		}

		template<typename T>
		const T& get(size_t row, size_t col) const
		{
			assert(row < rows_.size() && col < cols_.size());
			return std::get<T>(rows_.at(row).at(col));
		}

		const size_t column_size() const
		{
			return cols_.size();
		}

		const size_t row_size() const
		{
			return rows_.size();
		}
	private:
		std::vector<std::pair<std::string, int>> cols_;
		rows_t rows_;
	};
}