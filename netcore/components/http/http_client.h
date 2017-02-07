/****************************************************************************

Git <https://github.com/sniper00/MoonNetLua>
E-Mail <hanyongtao@live.com>
Copyright (c) 2015-2016 moon
Licensed under the MIT License <http://opensource.org/licenses/MIT>.

****************************************************************************/
#pragma once
#include "macro_define.h"

namespace moon
{
	class http_client;

	class MOON_EXPORT http_request
	{
	public:
		http_request();
		~http_request();

		friend class http_client;

		void set_request_type(const std::string& v);

		void set_path(const std::string& v);

		void set_content(const std::string& v);

		void push_header(const std::string& key, const std::string& value);

	private:
		const std::string& request_type();

		const std::string& path();

		const std::string& content();

		const std::map<std::string, std::string>& header();

	private:
		struct http_request_imp;
		http_request_imp* imp_;
	};

	class MOON_EXPORT http_response
	{
	public:

		friend class http_client;

		http_response();
		~http_response();

		const char* content();

		const char* status_code();

		const char* http_version();

		const char* header(const std::string& key);

	private:
		struct http_response_imp;
		http_response_imp* imp_;
	};

	class MOON_EXPORT http_client
	{
	public:
		http_client(const std::string& server_port_path);

		~http_client();

		http_request* make_request();

		http_response* request(http_request* r);
	private:
		struct http_client_imp;
		http_client_imp* imp_;
	};

}


