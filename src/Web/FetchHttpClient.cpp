/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "FetchHttpClient.h"

#include <cctype>
#include <cstring>

#include <algorithm>
#include <optional>
#include <string>

#include <emscripten/emscripten.h>
#include <emscripten/fetch.h>

namespace openblack::web
{

using online::HttpError;
using online::HttpMethod;
using online::HttpResponse;

struct FetchHttpClient::Transfer
{
	online::HttpRequestId id {0};
	online::HttpCallback callback;
	online::HttpRequest request;
	/// The headers as the fetch takes them: name, value, ..., null
	std::vector<const char*> headerTexts;
	emscripten_fetch_t* fetch {nullptr};
	bool cancelled {false};
	/// The client stopped the transfer: the answer was over its limit, too slow, or took too long
	bool stopped {false};
	/// The time of the start, and of the last stall check, in milliseconds
	double startMs {0.0};
	double stallCheckMs {0.0};
	/// The bytes that came, and the bytes at the last stall check
	uint64_t bytes {0};
	uint64_t stallCheckBytes {0};
	std::optional<HttpResponse> finished;
};

namespace
{

/// The largest answer that the client keeps, as on the desktop
constexpr uint64_t k_MostAnswerBytes = 16 * 1024 * 1024;

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

void Done(emscripten_fetch_t* fetch)
{
	auto& transfer = *static_cast<FetchHttpClient::Transfer*>(fetch->userData);
	HttpResponse response;
	if (transfer.stopped)
	{
		// The game keeps no part of an answer that it stopped
		response.error = HttpError::Network;
	}
	else if (transfer.cancelled)
	{
		response.error = HttpError::Cancelled;
	}
	else if (fetch->status == 0)
	{
		// The browser gives no status when the connection or its TLS failed
		response.error = HttpError::Network;
	}
	else
	{
		response.status = static_cast<int32_t>(fetch->status);
		const auto most = online::MostAnswerBytes(transfer.request,
		                                          transfer.request.large ? online::k_MostLargeAnswerBytes : k_MostAnswerBytes);
		if (fetch->numBytes > most)
		{
			// The game keeps no answer that is larger than its limit
			response.error = HttpError::Network;
			response.status = 0;
		}
		else if (fetch->data != nullptr && fetch->numBytes > 0)
		{
			const auto* bytes = reinterpret_cast<const uint8_t*>(fetch->data);
			response.body.assign(bytes, bytes + fetch->numBytes);
		}
		const auto length = emscripten_fetch_get_response_headers_length(fetch);
		if (length > 0)
		{
			std::string text(length + 1, '\0');
			emscripten_fetch_get_response_headers(fetch, text.data(), text.size());
			char** pairs = emscripten_fetch_unpack_response_headers(text.c_str());
			for (size_t i = 0; pairs != nullptr && pairs[i] != nullptr && pairs[i + 1] != nullptr; i += 2)
			{
				std::string name(pairs[i]);
				std::ranges::transform(name, name.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
				response.headers.emplace_back(std::move(name), std::string(pairs[i + 1]));
			}
			if (pairs != nullptr)
			{
				emscripten_fetch_free_unpacked_response_headers(pairs);
			}
		}
	}
	transfer.finished = std::move(response);
	transfer.fetch = nullptr;
	// A cancel closes the fetch itself, and the close runs this handler
	if (!transfer.cancelled)
	{
		emscripten_fetch_close(fetch);
	}
}

/// Stops a transfer before its end. The close runs the error handler at once, which records the stop.
void Stop(FetchHttpClient::Transfer& transfer)
{
	if (transfer.fetch == nullptr || transfer.finished.has_value())
	{
		return;
	}
	transfer.stopped = true;
	transfer.cancelled = true;
	auto* fetch = transfer.fetch;
	transfer.fetch = nullptr;
	emscripten_fetch_close(fetch);
	if (!transfer.finished.has_value())
	{
		transfer.finished = HttpResponse {.error = HttpError::Network};
	}
}

[[nodiscard]] uint64_t MostBytesOf(const FetchHttpClient::Transfer& transfer)
{
	return online::MostAnswerBytes(transfer.request,
	                               transfer.request.large ? online::k_MostLargeAnswerBytes : k_MostAnswerBytes);
}

/// The browser gives the bytes that came so far. An answer over its limit stops at once, so that an answer with no end
/// cannot fill the memory of the page.
void Progress(emscripten_fetch_t* fetch)
{
	auto& transfer = *static_cast<FetchHttpClient::Transfer*>(fetch->userData);
	transfer.bytes = fetch->dataOffset + fetch->numBytes;
	if (transfer.bytes > MostBytesOf(transfer))
	{
		Stop(transfer);
	}
}

} // namespace

FetchHttpClient::FetchHttpClient() = default;

FetchHttpClient::~FetchHttpClient()
{
	for (auto& transfer : _transfers)
	{
		if (transfer->fetch != nullptr)
		{
			transfer->cancelled = true;
			emscripten_fetch_close(transfer->fetch);
		}
	}
}

online::HttpRequestId FetchHttpClient::Send(online::HttpRequest request, online::HttpCallback callback)
{
	auto transfer = std::make_unique<Transfer>();
	transfer->id = _nextId++;
	transfer->callback = std::move(callback);
	transfer->request = std::move(request);
	const auto id = transfer->id;
	if (!online::http_policy::IsSameOriginPath(transfer->request.url))
	{
		transfer->finished = HttpResponse {.error = HttpError::AddressRefused};
		_transfers.push_back(std::move(transfer));
		return id;
	}

	for (const auto& [name, value] : transfer->request.headers)
	{
		transfer->headerTexts.push_back(name.c_str());
		transfer->headerTexts.push_back(value.c_str());
	}
	transfer->headerTexts.push_back(nullptr);

	emscripten_fetch_attr_t attributes;
	emscripten_fetch_attr_init(&attributes);
	std::strncpy(attributes.requestMethod, MethodName(transfer->request.method), sizeof(attributes.requestMethod) - 1);
	attributes.attributes = EMSCRIPTEN_FETCH_LOAD_TO_MEMORY;
	attributes.onsuccess = Done;
	attributes.onerror = Done;
	attributes.onprogress = Progress;
	attributes.userData = transfer.get();
	attributes.requestHeaders = transfer->headerTexts.data();
	if (!transfer->request.body.empty())
	{
		attributes.requestData = reinterpret_cast<const char*>(transfer->request.body.data());
		attributes.requestDataSize = transfer->request.body.size();
	}
	// The browser expands a compressed answer. Poll checks the stall and the full time of a large file.
	attributes.timeoutMSecs = transfer->request.large ? 0 : 30'000;
	auto* raw = transfer.get();
	raw->startMs = emscripten_get_now();
	raw->stallCheckMs = raw->startMs;
	_transfers.push_back(std::move(transfer));
	raw->fetch = emscripten_fetch(&attributes, raw->request.url.c_str());
	if (raw->fetch == nullptr && !raw->finished.has_value())
	{
		raw->finished = HttpResponse {.error = HttpError::Network};
	}
	return id;
}

void FetchHttpClient::Cancel(online::HttpRequestId id)
{
	const auto found = std::ranges::find_if(_transfers, [id](const auto& transfer) { return transfer->id == id; });
	if (found == _transfers.end() || (*found)->finished.has_value())
	{
		return;
	}
	auto& transfer = **found;
	transfer.cancelled = true;
	if (transfer.fetch != nullptr)
	{
		// The close runs the error handler at once, which records the cancel
		emscripten_fetch_close(transfer.fetch);
		transfer.fetch = nullptr;
	}
	if (!transfer.finished.has_value())
	{
		transfer.finished = HttpResponse {.error = HttpError::Cancelled};
	}
}

void FetchHttpClient::Poll()
{
	// A large transfer stops when too few bytes came in the time of the stall limit, or when it takes longer than its
	// limit of bytes allows, as on the desktop
	const double now = emscripten_get_now();
	for (auto& transfer : _transfers)
	{
		if (!transfer->request.large || transfer->fetch == nullptr || transfer->finished.has_value())
		{
			continue;
		}
		if (now - transfer->startMs > static_cast<double>(online::LargeTransferTimeoutMs(MostBytesOf(*transfer))))
		{
			Stop(*transfer);
			continue;
		}
		constexpr double k_StallMs = online::k_LargeStallSeconds * 1000.0;
		if (now - transfer->stallCheckMs >= k_StallMs)
		{
			const auto least = static_cast<uint64_t>(online::k_LargeStallBytesPerSecond) * online::k_LargeStallSeconds;
			if (transfer->bytes - transfer->stallCheckBytes < least)
			{
				Stop(*transfer);
				continue;
			}
			transfer->stallCheckMs = now;
			transfer->stallCheckBytes = transfer->bytes;
		}
	}

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

size_t FetchHttpClient::Pending() const
{
	return _transfers.size();
}

} // namespace openblack::web
