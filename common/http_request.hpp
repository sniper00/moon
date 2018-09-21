#pragma once
#include <cstdint>
#include <unordered_map>
#include "macro_define.hpp"
#include "buffer_view.hpp"
#include "utils.hpp"

namespace moon
{
	using  case_insensitive_multimap = std::unordered_multimap<string_view_t, string_view_t, moon::ihash_string_view_functor_t, moon::iequal_string_view_functor_t>;

	class http_request
	{
	public:
		http_request(const string_view_t &remote_endpoint_address = string_view_t(), uint16_t remote_endpoint_port = 0)
			:remote_endpoint_port(remote_endpoint_port)
			, remote_endpoint_address(remote_endpoint_address)
		{
		}

		int parse_string(const std::string& data) noexcept
		{
			data_ = data;
			return parse(data_);
		}

		/// Parse request line and header fields
		int parse(string_view_t sv) noexcept
		{
			header.clear();

			buffer_view br(sv.data(), sv.size());
			auto line = br.readline();

			size_t method_end;
			if ((method_end = line.find(' ')) != string_view_t::npos)
			{
				method = line.substr(0, method_end);
				size_t query_start = string_view_t::npos;
				size_t path_and_query_string_end = string_view_t::npos;
				for (size_t i = method_end + 1; i < line.size(); ++i)
				{
					if (line[i] == '?' && (i + 1) < line.size())
					{
						query_start = i + 1;
					}
					else if (line[i] == ' ')
					{
						path_and_query_string_end = i;
						break;
					}
				}
				if (path_and_query_string_end != string_view_t::npos)
				{
					if (query_start != string_view_t::npos) {
						path = line.substr(method_end + 1, query_start - method_end - 2);
						query_string = line.substr(query_start, path_and_query_string_end - query_start);
					}
					else
						path = line.substr(method_end + 1, path_and_query_string_end - method_end - 1);
					size_t protocol_end;
					if ((protocol_end = line.find('/', path_and_query_string_end + 1)) != string_view_t::npos) {
						if (line.compare(path_and_query_string_end + 1, protocol_end - path_and_query_string_end - 1, "HTTP") != 0)
							return -1;
						http_version = line.substr(protocol_end + 1, line.size() - protocol_end - 2);
					}
					else
						return -1;

					line = br.readline();
					size_t param_end;
					while ((param_end = line.find(':')) != string_view_t::npos) {
						size_t value_start = param_end + 1;
						if (value_start < line.size()) {
							if (line[value_start] == ' ')
								value_start++;
							if (value_start < line.size())
								header.emplace(line.substr(0, param_end), line.substr(value_start, line.size() - value_start));
						}
						line = br.readline();
					}
				}
				else
					return -1;
			}
			else
				return -1;
			return static_cast<int>(sv.size() - br.size());
		}

		string_view_t get_header(const string_view_t& key) const
		{
			auto iter = header.find(key);
			if (iter != header.end())
			{
				return iter->second;
			}
			return string_view_t();
		}

		bool keep_alive() const
		{
			string_view_t v;
			if (try_get_value(header, "Connection"sv, v))
			{
				return iequal_string(v, "keep-alive"sv);
			}
			return false;
		}

		uint16_t remote_endpoint_port;
		string_view_t method;
		string_view_t path;
		string_view_t query_string;
		string_view_t http_version;
		string_view_t remote_endpoint_address;
	private:
		std::string data_;
		case_insensitive_multimap header;
	};
}

