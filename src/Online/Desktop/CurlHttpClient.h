/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <memory>
#include <string_view>
#include <vector>

#include "Online/HttpClientInterface.h"

namespace openblack::online
{

/// The TLS settings of the desktop client
namespace tls_settings
{
/// The TLS 1.2 cipher suites: ECDHE key exchange with AES-GCM or ChaCha20-Poly1305 only, so each connection has forward
/// secrecy and authenticated encryption. The names are the names of OpenSSL.
constexpr std::string_view k_Tls12Ciphers = "ECDHE-ECDSA-AES128-GCM-SHA256:ECDHE-RSA-AES128-GCM-SHA256:"
                                            "ECDHE-ECDSA-AES256-GCM-SHA384:ECDHE-RSA-AES256-GCM-SHA384:"
                                            "ECDHE-ECDSA-CHACHA20-POLY1305:ECDHE-RSA-CHACHA20-POLY1305";
/// The TLS 1.3 cipher suites, which are all AEAD suites
constexpr std::string_view k_Tls13Ciphers = "TLS_AES_128_GCM_SHA256:TLS_AES_256_GCM_SHA384:TLS_CHACHA20_POLY1305_SHA256";
/// The time to connect, and the time for all of a request
constexpr long k_ConnectTimeoutMs = 10'000;
constexpr long k_RequestTimeoutMs = 30'000;
/// The largest answer that the client keeps
constexpr size_t k_MostAnswerBytes = 16 * 1024 * 1024;
} // namespace tls_settings

/// The HTTPS client of the desktop, with libcurl and OpenSSL. The transfers move on in Poll, without a wait, so a request
/// does not stop a frame. Names of hosts resolve on a thread of libcurl.
class CurlHttpClient final: public HttpClientInterface
{
public:
	CurlHttpClient();
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
};

} // namespace openblack::online
