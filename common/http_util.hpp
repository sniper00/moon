#pragma once
#include <cstdint>
#include <unordered_map>
#include "macro_define.hpp"
#include "buffer_view.hpp"
#include "utils.hpp"

namespace moon
{
    namespace http
    {
        using  case_insensitive_multimap = std::unordered_multimap<std::string, std::string, moon::ihash_string_view_functor_t, moon::iequal_string_view_functor_t>;
        using  case_insensitive_multimap_view = std::unordered_multimap<string_view_t, string_view_t, moon::ihash_string_view_functor_t, moon::iequal_string_view_functor_t>;

        class percent {
        public:
            /// Returns percent-encoded string
            static std::string encode(const std::string &value) noexcept {
                static auto hex_chars = "0123456789ABCDEF";

                std::string result;
                result.reserve(value.size()); // Minimum size of result

                for (auto &chr : value) {
                    if (!((chr >= '0' && chr <= '9') || (chr >= 'A' && chr <= 'Z') || (chr >= 'a' && chr <= 'z') || chr == '-' || chr == '.' || chr == '_' || chr == '~'))
                        result += std::string("%") + hex_chars[static_cast<unsigned char>(chr) >> 4] + hex_chars[static_cast<unsigned char>(chr) & 15];
                    else
                        result += chr;
                }

                return result;
            }

            /// Returns percent-decoded string
            static std::string decode(const std::string &value) noexcept {
                std::string result;
                result.reserve(value.size() / 3 + (value.size() % 3)); // Minimum size of result

                for (std::size_t i = 0; i < value.size(); ++i) {
                    auto &chr = value[i];
                    if (chr == '%' && i + 2 < value.size()) {
                        auto hex = value.substr(i + 1, 2);
                        auto decoded_chr = static_cast<char>(std::strtol(hex.c_str(), nullptr, 16));
                        result += decoded_chr;
                        i += 2;
                    }
                    else if (chr == '+')
                        result += ' ';
                    else
                        result += chr;
                }

                return result;
            }
        };

        class http_header {
        public:
            /// Parse header fields
            static case_insensitive_multimap_view parse(std::string_view sv) noexcept {
                case_insensitive_multimap_view result;
                buffer_view br(sv.data(), sv.size());
                std::string_view line = br.readline();
                size_t param_end = 0;
                while ((param_end = line.find(':')) != string_view_t::npos) {
                    size_t value_start = param_end + 1;
                    if (value_start < line.size()) {
                        if (line[value_start] == ' ')
                            value_start++;
                        if (value_start < line.size())
                            result.emplace(line.substr(0, param_end), line.substr(value_start, line.size() - value_start));
                    }
                    line = br.readline();
                }
                return result;
            }
        };

        class request_parser
        {
        public:
            int parse_string(const std::string& data) noexcept
            {
                data_ = std::move(data);
                return parse(data_);
            }

            /// Parse request line and header fields
            int parse(string_view_t sv) noexcept
            {
                header_.clear();

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
                            http_version = line.substr(protocol_end + 1, line.size() - protocol_end - 1);
                        }
                        else
                            return -1;

                        header_ = http_header::parse(string_view_t{ br.data(),br.size() });
                    }
                    else
                        return -1;
                }
                else
                    return -1;
                return static_cast<int>(sv.size() - br.size());
            }

            string_view_t header(const string_view_t& key) const
            {
                auto iter = header_.find(key);
                if (iter != header_.end())
                {
                    return iter->second;
                }
                return string_view_t();
            }

            bool has_header(const string_view_t& key) const
            {
                auto iter = header_.find(key);
                if (iter != header_.end())
                {
                    return true;
                }
                return false;
            }

            string_view_t method;
            string_view_t path;
            string_view_t query_string;
            string_view_t http_version;
        private:
            std::string data_;
            case_insensitive_multimap_view header_;
        };

        class response_parser
        {
        public:
            bool parse_string(const std::string& data) noexcept
            {
                data_ = std::move(data);
                return parse(data_);
            }

            /// Parse status line and header fields
            bool parse(string_view_t sv) noexcept
            {
                buffer_view br(sv.data(), sv.size());
                auto line = br.readline();
                std::size_t version_end;
                if (!line.empty() && (version_end = line.find(' ')) != string_view_t::npos) {
                    if (5 < line.size())
                        version = line.substr(5, version_end - 5);
                    else
                        return false;
                    if ((version_end + 1) < line.size())
                        status_code = line.substr(version_end + 1, line.size() - (version_end + 1));
                    else
                        return false;

                    header_ = http_header::parse(string_view_t{ br.data(),br.size() });
                }
                else
                    return false;
                return true;
            }

            string_view_t header(const string_view_t& key) const
            {
                auto iter = header_.find(key);
                if (iter != header_.end())
                {
                    return iter->second;
                }
                return string_view_t();
            }

            bool has_header(const string_view_t& key) const
            {
                auto iter = header_.find(key);
                if (iter != header_.end())
                {
                    return true;
                }
                return false;
            }

            string_view_t version;
            string_view_t status_code;
        private:
            std::string data_;
            case_insensitive_multimap_view header_;
        };
    }
   
}

