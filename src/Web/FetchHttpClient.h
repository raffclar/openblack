/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#if !defined(__EMSCRIPTEN__)
#error "The web platform code is only for the web build"
#endif

#include <memory>
#include <vector>

#include "Online/HttpClientInterface.h"

namespace openblack::web
{

/// The HTTPS client of the web build: the fetch of the browser, through Emscripten. It calls only paths on the origin of
/// the page, so the browser does the TLS and the checks of the certificate, and sends the cookies of the origin.
class FetchHttpClient final: public online::HttpClientInterface
{
public:
	FetchHttpClient();
	~FetchHttpClient() override;
	FetchHttpClient(const FetchHttpClient&) = delete;
	FetchHttpClient& operator=(const FetchHttpClient&) = delete;
	FetchHttpClient(FetchHttpClient&&) = delete;
	FetchHttpClient& operator=(FetchHttpClient&&) = delete;

	online::HttpRequestId Send(online::HttpRequest request, online::HttpCallback callback) override;
	void Cancel(online::HttpRequestId id) override;
	void Poll() override;
	[[nodiscard]] size_t Pending() const override;

	struct Transfer;

private:
	std::vector<std::unique_ptr<Transfer>> _transfers;
	online::HttpRequestId _nextId {1};
};

} // namespace openblack::web
