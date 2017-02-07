/****************************************************************************

Git <https://github.com/sniper00/MoonNetLua>
E-Mail <hanyongtao@live.com>
Copyright (c) 2015-2016 moon
Licensed under the MIT License <http://opensource.org/licenses/MIT>.

****************************************************************************/
#include "components/http/http_client.h"
#include "client_http.hpp"

namespace moon
{
	///////////////////////http_request/////////////////////////
	struct http_request::http_request_imp
	{
		http_request_imp()
			:path_("/")
		{

		}

		std::string request_type_;
		std::string path_;
		std::string content_;
		std::map<std::string, std::string> header_;
	};

	http_request::http_request()
		:imp_(nullptr)
	{
		imp_ = new http_request_imp;
	}

	http_request::~http_request()
	{
		SAFE_DELETE(imp_);
	}

	void http_request::set_request_type(const std::string & v)
	{
		imp_->request_type_ = v;
	}

	void http_request::set_path(const std::string & v)
	{
		imp_->path_ = v;
	}

	void http_request::set_content(const std::string & v)
	{
		imp_->content_ = v;
	}

	void http_request::push_header(const std::string & key, const std::string & value)
	{
		imp_->header_.emplace(key, value);
	}

	const std::string& http_request::request_type()
	{
		return imp_->request_type_;
	}

	const std::string& http_request::path()
	{
		return imp_->path_;
	}

	const std::string& http_request::content()
	{
		return imp_->content_;
	}

	const std::map<std::string, std::string>& http_request::header()
	{
		return imp_->header_;
	}

	/////////////////////////////http_response////////////////////////////////
	struct http_response::http_response_imp
	{
		using response_t = std::shared_ptr<moon::ClientBase<HTTP>::Response>;
		response_t response_;
	};

	http_response::http_response()
		:imp_(nullptr)
	{
		imp_ = new http_response_imp;
	}

	http_response::~http_response()
	{
		SAFE_DELETE(imp_);
	}

	const char* http_response::content()
	{
		if (nullptr != imp_->response_)
		{
			std::stringstream output;
			auto stm = imp_->response_->content.rdbuf();
			output << stm;
			return output.str().data();
		}
		return "";
	}

	const char* http_response::status_code()
	{
		if (nullptr != imp_->response_)
		{
			return imp_->response_->status_code.data();
		}
		return "";
	}

	const char* http_response::http_version()
	{
		if (nullptr != imp_->response_)
		{
			return imp_->response_->http_version.data();
		}
		return "";
	}

	const char* http_response::header(const std::string & key)
	{
		if (nullptr != imp_->response_)
		{
			auto iter = imp_->response_->header.find(key);
			if (iter != imp_->response_->header.end())
			{
				return iter->second.data();
			}
		}
		return "";
	}

	///////////////////////////////////////////////////////////
	struct http_client::http_client_imp
	{
		http_client_imp(const std::string& v)
			:client_(v)
		{
		}

		using HttpClient = Client<HTTP>;
		std::shared_ptr<http_request> current_request_;
		std::shared_ptr<http_response> current_response_;
		HttpClient client_;
	};

	http_client::http_client(const std::string& server_port_path)
		:imp_(nullptr)
	{
		imp_ = new http_client_imp(server_port_path);
	}

	http_client::~http_client()
	{
		SAFE_DELETE(imp_);
	}

	http_request * http_client::make_request()
	{
		imp_->current_request_ = std::make_shared<http_request>();
		return imp_->current_request_.get();
	}

	http_response* http_client::request(http_request * r)
	{
		auto res  = imp_->client_.request(r->request_type(), r->path(), r->content(), r->header());
		if (res != nullptr)
		{
			imp_->current_response_ = std::make_shared<http_response>();
			imp_->current_response_->imp_->response_ = res;
		}
		return nullptr;
	}
}