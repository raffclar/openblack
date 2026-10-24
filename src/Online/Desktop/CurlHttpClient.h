/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <filesystem>
#include <memory>
#include <optional>
#include <string_view>
#include <vector>

#include "Online/Desktop/CurlTls.h"
#include "Online/HttpClientInterface.h"

namespace openblack::online
{

/// The certificate and the key that a client shows to a server that asks for mutual TLS, as PEM files
struct ClientIdentity
{
	std::filesystem::path certificate;
	std::filesystem::path key;
};

/// The HTTPS client of the desktop, with libcurl and OpenSSL. The transfers move on in Poll, without a wait, so a request
/// does not stop a frame. Names of hosts resolve on a thread of libcurl.
class CurlHttpClient final: public HttpClientInterface
{
public:
	CurlHttpClient();
	/// A client that shows its certificate on each request, for a server that asks for mutual TLS
	explicit CurlHttpClient(ClientIdentity identity);
	/// A client that also trusts a PEM file of more certificate authorities, for the tests on this computer only. The
	/// client still checks each certificate and host name. The callers give the file only for a loopback address.
	CurlHttpClient(std::optional<ClientIdentity> identity, std::filesystem::path testAuthorities);
	~CurlHttpClient() override;
	CurlHttpClient(const CurlHttpClient&) = delete;
	CurlHttpClient& operator=(const CurlHttpClient&) = delete;
	CurlHttpClient(CurlHttpClient&&) = delete;
	CurlHttpClient& operator=(CurlHttpClient&&) = delete;

	HttpRequestId Send(HttpRequest request, HttpCallback callback) override;
	void Cancel(HttpRequestId id) override;
	void Poll() override;
	[[nodiscard]] size_t Pending() const override;

private:
	struct Transfer;
	struct Multi;

	void Finish(Transfer& transfer, HttpResponse&& response);

	std::unique_ptr<Multi> _multi;
	std::vector<std::unique_ptr<Transfer>> _transfers;
	HttpRequestId _nextId {1};
	std::optional<ClientIdentity> _identity;
	std::optional<std::string> _testAuthorities;
};

} // namespace openblack::online
