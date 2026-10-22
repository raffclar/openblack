/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>

#include <string_view>

#include <curl/curl.h>

#include "Online/HttpClientInterface.h"

/// The TLS rules of the desktop clients. The HTTPS client and the WSS client use the same rules.
namespace openblack::online::tls_settings
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

/// Starts the global state of libcurl one time for the program
void StartLibrary();

/// Sets the TLS rules on a transfer: TLS 1.2 at the oldest, the AEAD cipher suites above only, and a check of the
/// certificate and the host name each time. On Windows the certificates of the authorities come from the certificate
/// store of the system. A file of more authorities is for the tests on this computer only, and it does not remove the
/// checks.
void Apply(CURL* easy, const char* extraAuthorities = nullptr);

/// The error of the HTTPS client for a result of libcurl
[[nodiscard]] HttpError ErrorOf(CURLcode code) noexcept;

} // namespace openblack::online::tls_settings
