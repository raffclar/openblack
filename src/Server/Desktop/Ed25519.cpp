/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Server/Ed25519.h"

#include <memory>

#include <openssl/evp.h>

namespace openblack::ed25519
{
namespace
{

struct KeyDeleter
{
	void operator()(EVP_PKEY* key) const { EVP_PKEY_free(key); }
};
struct ContextDeleter
{
	void operator()(EVP_MD_CTX* context) const { EVP_MD_CTX_free(context); }
};

using Key = std::unique_ptr<EVP_PKEY, KeyDeleter>;
using Context = std::unique_ptr<EVP_MD_CTX, ContextDeleter>;

Key SecretKey(const Seed& seed)
{
	return Key(EVP_PKEY_new_raw_private_key(EVP_PKEY_ED25519, nullptr, seed.data(), seed.size()));
}

} // namespace

bool Verify(const join_ticket::PublicKey& key, std::span<const uint8_t> bytes, const join_ticket::Signature& signature)
{
	const Key publicKey(EVP_PKEY_new_raw_public_key(EVP_PKEY_ED25519, nullptr, key.data(), key.size()));
	const Context context(EVP_MD_CTX_new());
	if (!publicKey || !context || EVP_DigestVerifyInit(context.get(), nullptr, nullptr, nullptr, publicKey.get()) != 1)
	{
		return false;
	}
	return EVP_DigestVerify(context.get(), signature.data(), signature.size(), bytes.data(), bytes.size()) == 1;
}

std::optional<join_ticket::Signature> Sign(const Seed& seed, std::span<const uint8_t> bytes)
{
	const auto key = SecretKey(seed);
	const Context context(EVP_MD_CTX_new());
	if (!key || !context || EVP_DigestSignInit(context.get(), nullptr, nullptr, nullptr, key.get()) != 1)
	{
		return std::nullopt;
	}
	join_ticket::Signature signature {};
	size_t length = signature.size();
	if (EVP_DigestSign(context.get(), signature.data(), &length, bytes.data(), bytes.size()) != 1 || length != signature.size())
	{
		return std::nullopt;
	}
	return signature;
}

std::optional<join_ticket::PublicKey> PublicKeyOf(const Seed& seed)
{
	const auto key = SecretKey(seed);
	join_ticket::PublicKey publicKey {};
	size_t length = publicKey.size();
	if (!key || EVP_PKEY_get_raw_public_key(key.get(), publicKey.data(), &length) != 1 || length != publicKey.size())
	{
		return std::nullopt;
	}
	return publicKey;
}

} // namespace openblack::ed25519
