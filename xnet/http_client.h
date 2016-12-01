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
	class XNET_DLL http_request
	{
	public:
		http_request();
		~http_request();

		void set_request_type(const std::string& v);

		void set_path(const std::string& v);

		void set_content(const std::string& v);

		void push_header(const std::string& key, const std::string& value);

		const std::string& request_type();

		const std::string& path();

		const std::string& content();

		const std::map<std::string, std::string>& header();

	private:
		struct http_request_imp;
		http_request_imp* imp_;
	};

	class XNET_DLL http_client
	{
	public:
		http_client(const std::string& server_port_path);
		~http_client();

		http_request* make_request();

		int request(http_request* r);

		std::string get_content(int requestid);
	private:
		struct http_client_imp;
		http_client_imp* imp_;
	};

}


