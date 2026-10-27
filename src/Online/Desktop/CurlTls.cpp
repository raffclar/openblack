/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CurlTls.h"

#include <array>
#include <filesystem>
#include <mutex>
#include <string>
#include <system_error>

namespace openblack::online::tls_settings
{

void StartLibrary()
{
	static std::once_flag started;
	std::call_once(started, []() { curl_global_init(CURL_GLOBAL_DEFAULT); });
}

void Apply(CURL* easy, const char* extraAuthorities)
{
	curl_easy_setopt(easy, CURLOPT_SSLVERSION, static_cast<long>(CURL_SSLVERSION_TLSv1_2 | CURL_SSLVERSION_MAX_DEFAULT));
	curl_easy_setopt(easy, CURLOPT_SSL_CIPHER_LIST, std::string(k_Tls12Ciphers).c_str());
	curl_easy_setopt(easy, CURLOPT_TLS13_CIPHERS, std::string(k_Tls13Ciphers).c_str());
	curl_easy_setopt(easy, CURLOPT_SSL_VERIFYPEER, 1L);
	curl_easy_setopt(easy, CURLOPT_SSL_VERIFYHOST, 2L);
	if (extraAuthorities != nullptr)
	{
		curl_easy_setopt(easy, CURLOPT_CAINFO, extraAuthorities);
	}
#if defined(_WIN32)
	// The certificates of the authorities come from the certificate store of Windows
	curl_easy_setopt(easy, CURLOPT_SSL_OPTIONS, static_cast<long>(CURLSSLOPT_NATIVE_CA));
#elif defined(__linux__)
	// The certificates of the authorities come from the bundle of the system, at the place that the distribution uses
	if (extraAuthorities == nullptr)
	{
		constexpr std::array<const char*, 3> k_SystemBundles {"/etc/ssl/certs/ca-certificates.crt",
		                                                      "/etc/pki/tls/certs/ca-bundle.crt", "/etc/ssl/cert.pem"};
		std::error_code error;
		for (const auto* bundle : k_SystemBundles)
		{
			if (std::filesystem::is_regular_file(bundle, error))
			{
				curl_easy_setopt(easy, CURLOPT_CAINFO, bundle);
				break;
			}
		}
	}
#endif
}

HttpError ErrorOf(CURLcode code) noexcept
{
	switch (code)
	{
	case CURLE_OK:
		return HttpError::None;
	case CURLE_UNSUPPORTED_PROTOCOL:
	case CURLE_URL_MALFORMAT:
		return HttpError::AddressRefused;
	case CURLE_OPERATION_TIMEDOUT:
		return HttpError::Timeout;
	case CURLE_SSL_CONNECT_ERROR:
	case CURLE_PEER_FAILED_VERIFICATION:
	case CURLE_SSL_CERTPROBLEM:
	case CURLE_SSL_CIPHER:
	case CURLE_SSL_CACERT_BADFILE:
	case CURLE_SSL_ENGINE_NOTFOUND:
	case CURLE_SSL_ENGINE_SETFAILED:
	case CURLE_SSL_ENGINE_INITFAILED:
	case CURLE_SSL_SHUTDOWN_FAILED:
	case CURLE_SSL_CRL_BADFILE:
	case CURLE_SSL_ISSUER_ERROR:
	case CURLE_SSL_PINNEDPUBKEYNOTMATCH:
	case CURLE_SSL_INVALIDCERTSTATUS:
	case CURLE_USE_SSL_FAILED:
		return HttpError::Tls;
	default:
		return HttpError::Network;
	}
}

} // namespace openblack::online::tls_settings
