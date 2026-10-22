/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CurlHttpClient.h"

#include <cctype>

#include <algorithm>
#include <string>
#include <utility>

#include <curl/curl.h>
#include <spdlog/spdlog.h>

namespace openblack::online
{
namespace
{

struct EasyDeleter
{
	void operator()(CURL* easy) const { curl_easy_cleanup(easy); }
};

struct ListDeleter
{
	void operator()(curl_slist* list) const { curl_slist_free_all(list); }
};

const char* MethodName(HttpMethod method)
{
	switch (method)
	{
	case HttpMethod::Get:
		return "GET";
	case HttpMethod::Post:
		return "POST";
	case HttpMethod::Put:
		return "PUT";
	case HttpMethod::Patch:
		return "PATCH";
	case HttpMethod::Delete:
		return "DELETE";
	}
	return "GET";
}

} // namespace

struct CurlHttpClient::Multi
{
	CURLM* handle {nullptr};

	Multi()
	    : handle(curl_multi_init())
	{
	}
	~Multi()
	{
		if (handle != nullptr)
		{
			curl_multi_cleanup(handle);
		}
	}
	Multi(const Multi&) = delete;
	Multi& operator=(const Multi&) = delete;
	Multi(Multi&&) = delete;
	Multi& operator=(Multi&&) = delete;
};

struct CurlHttpClient::Transfer
{
	HttpRequestId id {0};
	HttpCallback callback;
	HttpRequest request;
	std::unique_ptr<CURL, EasyDeleter> easy;
	std::unique_ptr<curl_slist, ListDeleter> headers;
	HttpResponse response;
	bool tooLarge {false};
	/// The transfer is in the multi handle of libcurl
	bool active {false};
	/// The answer is ready for the callback in the next Poll
	std::optional<HttpResponse> finished;

	static size_t Write(char* data, size_t size, size_t count, void* user)
	{
		auto& transfer = *static_cast<Transfer*>(user);
		const auto bytes = size * count;
		if (transfer.response.body.size() + bytes > tls_settings::k_MostAnswerBytes)
		{
			transfer.tooLarge = true;
			// A value other than the number of bytes stops the transfer
			return 0;
		}
		transfer.response.body.insert(transfer.response.body.end(), data, data + bytes);
		return bytes;
	}

	static size_t Header(char* data, size_t size, size_t count, void* user)
	{
		auto& transfer = *static_cast<Transfer*>(user);
		const auto bytes = size * count;
		std::string_view line(data, bytes);
		while (!line.empty() && (line.back() == '\r' || line.back() == '\n'))
		{
			line.remove_suffix(1);
		}
		if (line.starts_with("HTTP/"))
		{
			// A new status line: the headers of an answer before it do not count
			transfer.response.headers.clear();
			return bytes;
		}
		const auto colon = line.find(':');
		if (colon == std::string_view::npos)
		{
			return bytes;
		}
		std::string name(line.substr(0, colon));
		std::ranges::transform(name, name.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		auto value = line.substr(colon + 1);
		while (!value.empty() && (value.front() == ' ' || value.front() == '\t'))
		{
			value.remove_prefix(1);
		}
		transfer.response.headers.emplace_back(std::move(name), std::string(value));
		return bytes;
	}
};

CurlHttpClient::CurlHttpClient()
{
	tls_settings::StartLibrary();
	_multi = std::make_unique<Multi>();
}

CurlHttpClient::~CurlHttpClient()
{
	for (auto& transfer : _transfers)
	{
		if (transfer->active)
		{
			curl_multi_remove_handle(_multi->handle, transfer->easy.get());
		}
	}
	_transfers.clear();
}

HttpRequestId CurlHttpClient::Send(HttpRequest request, HttpCallback callback)
{
	auto transfer = std::make_unique<Transfer>();
	transfer->id = _nextId++;
	transfer->callback = std::move(callback);
	transfer->request = std::move(request);
	const auto id = transfer->id;

	if (!http_policy::IsHttpsAddress(transfer->request.url))
	{
		transfer->finished = HttpResponse {.error = HttpError::AddressRefused};
		_transfers.push_back(std::move(transfer));
		return id;
	}
	transfer->easy.reset(curl_easy_init());
	if (!transfer->easy || _multi->handle == nullptr)
	{
		transfer->finished = HttpResponse {.error = HttpError::Network};
		_transfers.push_back(std::move(transfer));
		return id;
	}

	auto* easy = transfer->easy.get();
	auto& sent = transfer->request;
	curl_easy_setopt(easy, CURLOPT_URL, sent.url.c_str());
	// HTTPS only, and no redirection to another address
	curl_easy_setopt(easy, CURLOPT_PROTOCOLS_STR, "https");
	curl_easy_setopt(easy, CURLOPT_REDIR_PROTOCOLS_STR, "https");
	curl_easy_setopt(easy, CURLOPT_FOLLOWLOCATION, 0L);
	// The TLS rules: TLS 1.2 at the oldest, AEAD cipher suites only, and a check of the certificate and the host name
	tls_settings::Apply(easy);
	curl_easy_setopt(easy, CURLOPT_CONNECTTIMEOUT_MS, tls_settings::k_ConnectTimeoutMs);
	curl_easy_setopt(easy, CURLOPT_TIMEOUT_MS, tls_settings::k_RequestTimeoutMs);
	curl_easy_setopt(easy, CURLOPT_NOSIGNAL, 1L);
	curl_easy_setopt(easy, CURLOPT_USERAGENT, "openblack");
	curl_easy_setopt(easy, CURLOPT_WRITEFUNCTION, &Transfer::Write);
	curl_easy_setopt(easy, CURLOPT_WRITEDATA, transfer.get());
	curl_easy_setopt(easy, CURLOPT_HEADERFUNCTION, &Transfer::Header);
	curl_easy_setopt(easy, CURLOPT_HEADERDATA, transfer.get());
	curl_easy_setopt(easy, CURLOPT_PRIVATE, transfer.get());

	if (sent.method == HttpMethod::Get)
	{
		curl_easy_setopt(easy, CURLOPT_HTTPGET, 1L);
	}
	else
	{
		if (sent.method != HttpMethod::Post)
		{
			curl_easy_setopt(easy, CURLOPT_CUSTOMREQUEST, MethodName(sent.method));
		}
		if (sent.method == HttpMethod::Post || !sent.body.empty())
		{
			curl_easy_setopt(easy, CURLOPT_POSTFIELDSIZE_LARGE, static_cast<curl_off_t>(sent.body.size()));
			curl_easy_setopt(easy, CURLOPT_POSTFIELDS, sent.body.data());
		}
	}

	curl_slist* list = nullptr;
	for (const auto& [name, value] : sent.headers)
	{
		list = curl_slist_append(list, (name + ": " + value).c_str());
	}
	// The client sends the body at once, without a wait for "100 Continue"
	list = curl_slist_append(list, "Expect:");
	transfer->headers.reset(list);
	curl_easy_setopt(easy, CURLOPT_HTTPHEADER, list);

	if (curl_multi_add_handle(_multi->handle, easy) != CURLM_OK)
	{
		transfer->finished = HttpResponse {.error = HttpError::Network};
	}
	else
	{
		transfer->active = true;
	}
	_transfers.push_back(std::move(transfer));
	return id;
}

void CurlHttpClient::Cancel(HttpRequestId id)
{
	const auto found = std::ranges::find_if(_transfers, [id](const auto& transfer) { return transfer->id == id; });
	if (found == _transfers.end() || (*found)->finished.has_value())
	{
		return;
	}
	auto& transfer = **found;
	if (transfer.active)
	{
		curl_multi_remove_handle(_multi->handle, transfer.easy.get());
		transfer.active = false;
	}
	transfer.finished = HttpResponse {.error = HttpError::Cancelled};
}

void CurlHttpClient::Poll()
{
	if (_transfers.empty())
	{
		return;
	}
	if (std::ranges::any_of(_transfers, [](const auto& transfer) { return transfer->active; }))
	{
		int running = 0;
		curl_multi_perform(_multi->handle, &running);
		int queued = 0;
		while (CURLMsg* message = curl_multi_info_read(_multi->handle, &queued))
		{
			if (message->msg != CURLMSG_DONE)
			{
				continue;
			}
			Transfer* transfer = nullptr;
			curl_easy_getinfo(message->easy_handle, CURLINFO_PRIVATE, &transfer);
			if (transfer == nullptr)
			{
				continue;
			}
			const auto code = message->data.result;
			curl_multi_remove_handle(_multi->handle, message->easy_handle);
			transfer->active = false;
			Finish(*transfer, std::move(transfer->response));
			if (code != CURLE_OK)
			{
				auto& response = *transfer->finished;
				response.error = transfer->tooLarge ? HttpError::Network : tls_settings::ErrorOf(code);
				response.status = 0;
				if (const auto logger = spdlog::get("game"))
				{
					SPDLOG_LOGGER_DEBUG(logger, "A web request failed: {}", curl_easy_strerror(code));
				}
			}
		}
	}

	// The callbacks run after the list is changed, because a callback can send a new request
	std::vector<std::unique_ptr<Transfer>> done;
	for (auto& transfer : _transfers)
	{
		if (transfer->finished.has_value())
		{
			done.push_back(std::move(transfer));
		}
	}
	std::erase(_transfers, nullptr);
	for (auto& transfer : done)
	{
		if (transfer->callback)
		{
			transfer->callback(std::move(*transfer->finished));
		}
	}
}

size_t CurlHttpClient::Pending() const
{
	return _transfers.size();
}

void CurlHttpClient::Finish(Transfer& transfer, HttpResponse&& response)
{
	long status = 0;
	curl_easy_getinfo(transfer.easy.get(), CURLINFO_RESPONSE_CODE, &status);
	response.status = static_cast<int32_t>(status);
	response.error = HttpError::None;
	transfer.finished = std::move(response);
}

} // namespace openblack::online
