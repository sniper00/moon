/****************************************************************************

Git <https://github.com/sniper00/moon_net>
E-Mail <hanyongtao@live.com>
Copyright (c) 2015-2017 moon
Licensed under the MIT License <http://opensource.org/licenses/MIT>.

****************************************************************************/
#include <locale>
#include <codecvt>
#include <iostream>

namespace moon
{
	class iconv
	{
	public:
		static std::wstring string_to_wstring(const std::string& s)
		{
			std::locale sys_loc("");

			const size_t BUFFER_SIZE = s.size() + 1;

			std::wstring intern_buffer(BUFFER_SIZE, 0);

			const char* extern_from_next = 0;
			wchar_t* intern_to_next = 0;

			typedef std::codecvt<wchar_t, char, mbstate_t> CodecvtFacet;

			mbstate_t in_cvt_state = { 0 };

			CodecvtFacet::result cvt_rst =
				std::use_facet<CodecvtFacet>(sys_loc).in(
					in_cvt_state,
					s.c_str(),
					s.c_str() + s.size(),
					extern_from_next,
					&intern_buffer[0],
					&intern_buffer[0] + intern_buffer.size(),
					intern_to_next);

			if (cvt_rst != CodecvtFacet::ok) {
				switch (cvt_rst) {
				case CodecvtFacet::partial:
					std::cerr << "partial";
					break;
				case CodecvtFacet::error:
					std::cerr << "error";
					break;
				case CodecvtFacet::noconv:
					std::cerr << "noconv";
					break;
				default:
					std::cerr << "unknown";
				}
				std::cerr << ", please check in_cvt_state."
					<< std::endl;
			}
			std::wstring result = intern_buffer;
			return std::move(intern_buffer);
		}

		static std::string wstring_to_string(const std::wstring& ws)
		{
			std::locale sys_loc("");

			const wchar_t* src_wstr = ws.c_str();
			const size_t MAX_UNICODE_BYTES = 4;
			const size_t BUFFER_SIZE =
				ws.size() * MAX_UNICODE_BYTES + 1;

			std::string extern_buffer(BUFFER_SIZE, 0);

			const wchar_t* intern_from = src_wstr;
			const wchar_t* intern_from_end = intern_from + ws.size();
			const wchar_t* intern_from_next = 0;
			char* extern_to = &extern_buffer[0];
			char* extern_to_end = extern_to + BUFFER_SIZE;
			char* extern_to_next = 0;

			typedef std::codecvt<wchar_t, char, mbstate_t> CodecvtFacet;

			mbstate_t out_cvt_state = { 0 };

			CodecvtFacet::result cvt_rst =
				std::use_facet<CodecvtFacet>(sys_loc).out(
					out_cvt_state,
					intern_from, intern_from_end, intern_from_next,
					extern_to, extern_to_end, extern_to_next);
			if (cvt_rst != CodecvtFacet::ok) {
				switch (cvt_rst) {
				case CodecvtFacet::partial:
					std::cerr << "partial";
					break;
				case CodecvtFacet::error:
					std::cerr << "error";
					break;
				case CodecvtFacet::noconv:
					std::cerr << "noconv";
					break;
				default:
					std::cerr << "unknown";
				}
				std::cerr << ", please check out_cvt_state."
					<< std::endl;
			}

			return std::move(extern_buffer);
		}

		static std::string wstring_to_utf8(const std::wstring& src)
		{
			std::wstring_convert<std::codecvt_utf8<wchar_t>> conv;
			return conv.to_bytes(src);
		}

		static std::wstring utf8_to_wstring(const std::string& src)
		{
			std::wstring_convert<std::codecvt_utf8<wchar_t> > conv;
			return conv.from_bytes(src);
		}

		static std::string string_to_utf8(const std::string& src)
		{
			return wstring_to_utf8(string_to_wstring(src));
		}

		static std::string utf8_to_string(const std::string& src)
		{
			return wstring_to_string(utf8_to_wstring(src));
		}
	};
}