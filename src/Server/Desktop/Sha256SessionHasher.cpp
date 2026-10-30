/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Server/Desktop/Sha256SessionHasher.h"

#include <memory>

#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/rand.h>

namespace openblack::server
{
namespace
{

struct ContextDeleter
{
	void operator()(EVP_MD_CTX* context) const { EVP_MD_CTX_free(context); }
};

using Context = std::unique_ptr<EVP_MD_CTX, ContextDeleter>;

} // namespace

std::optional<std::array<uint8_t, k_SessionDigestBytes>> SaltedSha256(std::span<const uint8_t> salt, std::string_view secret)
{
	const Context context(EVP_MD_CTX_new());
	std::array<uint8_t, k_SessionDigestBytes> digest {};
	unsigned int length = 0;
	if (!context || EVP_DigestInit_ex(context.get(), EVP_sha256(), nullptr) != 1 ||
	    EVP_DigestUpdate(context.get(), salt.data(), salt.size()) != 1 ||
	    EVP_DigestUpdate(context.get(), secret.data(), secret.size()) != 1 ||
	    EVP_DigestFinal_ex(context.get(), digest.data(), &length) != 1 || length != digest.size())
	{
		return std::nullopt;
	}
	return digest;
}

std::optional<SessionHash> Sha256SessionHasher::Hash(std::string_view secret) const
{
	SessionHash hash;
	if (RAND_bytes(hash.salt.data(), static_cast<int>(hash.salt.size())) != 1)
	{
		return std::nullopt;
	}
	const auto digest = SaltedSha256(hash.salt, secret);
	if (!digest.has_value())
	{
		return std::nullopt;
	}
	hash.digest = *digest;
	return hash;
}

bool Sha256SessionHasher::Matches(const SessionHash& hash, std::string_view secret) const
{
	const auto digest = SaltedSha256(hash.salt, secret);
	return digest.has_value() && CRYPTO_memcmp(digest->data(), hash.digest.data(), hash.digest.size()) == 0;
}

} // namespace openblack::server
