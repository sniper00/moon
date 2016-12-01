/****************************************************************************

Git <https://github.com/sniper00/MoonNetLua>
E-Mail <hanyongtao@live.com>
Copyright (c) 2015-2016 moon
Licensed under the MIT License <http://opensource.org/licenses/MIT>.

****************************************************************************/
#include "http_client.h"
#include "detail/client_http.hpp"

namespace moon
{
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

	struct http_client::http_client_imp
	{
		http_client_imp(const std::string& v)
			:client_(v)
			, request_uid_(1)
		{
		}

		std::shared_ptr<http_request> current_request_;
		typedef Client<HTTP> HttpClient;
		HttpClient client_;
		int  request_uid_;

		using response_t = std::shared_ptr<moon::ClientBase<HTTP>::Response>;

		std::unordered_map<int, response_t> responses_;
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

	int http_client::request(http_request * r)
	{
		auto id = imp_->request_uid_++;
		auto tmp = imp_->client_.request(r->request_type(), r->path(), r->content(), r->header());
		imp_->responses_.emplace(id, tmp);
		return id;
	}

	std::string  http_client::get_content(int requestid)
	{
		auto iter = imp_->responses_.find(requestid);
		if (iter != imp_->responses_.end())
		{
			std::stringstream output;
			auto stm = iter->second->content.rdbuf();
			output << stm;
			return output.str();
		}
		return nullptr;
	}

}