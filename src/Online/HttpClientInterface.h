/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

/// The client of the web calls of the game. The game speaks only HTTPS. The desktop client accepts only https://
/// addresses, checks the certificate of the server each time, and speaks TLS 1.3, or TLS 1.2 at the oldest, with AEAD
/// cipher suites only. The browser client accepts only paths on the origin of the page, which is an HTTPS origin.
namespace openblack::online
{

enum class HttpMethod : uint8_t
{
	Get,
	Post,
	Put,
	Patch,
	Delete,
};

/// A header of a request or of an answer
using HttpHeader = std::pair<std::string, std::string>;

struct HttpRequest
{
	HttpMethod method {HttpMethod::Get};
	/// The full https:// address on the desktop, or a path that starts with "/" in the browser
	std::string url;
	std::vector<HttpHeader> headers;
	std::vector<uint8_t> body;
	/// A transfer of a large file, such as a land of a network game. The answer has the larger limit
	/// k_MostLargeAnswerBytes, and the request has no limit on its full time. The request stops when no bytes come for the
	/// time of the stall limit. The client asks for the file as it is, with no compression, so that a small compressed
	/// answer cannot expand to a very large body.
	bool large {false};
};

/// The most bytes of the answer to a transfer of a large file. The lobby keeps maps of at most 1 MiB.
inline constexpr size_t k_MostLargeAnswerBytes = 64 * 1024 * 1024;

/// Why a request got no answer from the server
enum class HttpError : uint8_t
{
	/// The server answered. The status tells the result.
	None,
	/// The address is not one that the client can call: not https://, or in the browser not a path of the origin
	AddressRefused,
	/// The client could not connect to the server, or the connection stopped
	Network,
	/// The TLS connection failed: a bad certificate, a wrong host name, or no version or cipher suite that both sides have
	Tls,
	/// The server did not answer in time
	Timeout,
	/// The game cancelled the request
	Cancelled,
};

struct HttpResponse
{
	HttpError error {HttpError::None};
	/// The HTTP status, 0 when there is an error
	int32_t status {0};
	/// The headers, with their names in lower case
	std::vector<HttpHeader> headers;
	std::vector<uint8_t> body;

	/// The value of a header, by its name in lower case
	[[nodiscard]] std::optional<std::string_view> Header(std::string_view lowerName) const
	{
		for (const auto& [name, value] : headers)
		{
			if (name == lowerName)
			{
				return value;
			}
		}
		return std::nullopt;
	}

	[[nodiscard]] std::string_view Text() const { return {reinterpret_cast<const char*>(body.data()), body.size()}; }
};

using HttpCallback = std::function<void(HttpResponse&&)>;
using HttpRequestId = uint32_t;

class HttpClientInterface
{
public:
	virtual ~HttpClientInterface() = default;

	/// Starts a request. The callback runs later, from Poll, on the thread that calls Poll. It never runs in Send.
	virtual HttpRequestId Send(HttpRequest request, HttpCallback callback) = 0;
	/// Stops a request. Its callback runs with the error Cancelled.
	virtual void Cancel(HttpRequestId id) = 0;
	/// Once a frame: moves the transfers on without a wait, and runs the callbacks of the requests that are done
	virtual void Poll() = 0;
	/// The number of requests that did not run their callback yet
	[[nodiscard]] virtual size_t Pending() const = 0;
	/// The number of bytes of the body of a request that went to the server. None when the client cannot tell, or the
	/// request ran its callback.
	[[nodiscard]] virtual std::optional<uint64_t> SentBytes(HttpRequestId /*id*/) const { return std::nullopt; }
};

/// The rules of the addresses
namespace http_policy
{

/// True when the address starts with https:// and has a host. The comparison of the scheme ignores case.
[[nodiscard]] bool IsHttpsAddress(std::string_view url) noexcept;
/// True when the address is a path on the origin of the page: it starts with one "/" and has no scheme or host
[[nodiscard]] bool IsSameOriginPath(std::string_view url) noexcept;
/// True when the address starts with wss:// and has a host
[[nodiscard]] bool IsWssAddress(std::string_view url) noexcept;
/// True when the host of an https:// or wss:// address is this computer: localhost or an IPv4 address 127.x.x.x. The
/// test authorities of the tools are only for such addresses.
[[nodiscard]] bool IsLoopbackAddress(std::string_view url) noexcept;

} // namespace http_policy

} // namespace openblack::online
