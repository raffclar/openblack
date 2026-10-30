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

#include <array>
#include <optional>
#include <span>
#include <string_view>

#include "Server/SessionHash.h"

namespace openblack::server
{

/// The SHA-256 digest of the salt, then the secret, from the cryptography library of the desktop. None when the library
/// fails.
[[nodiscard]] std::optional<std::array<uint8_t, k_SessionDigestBytes>> SaltedSha256(std::span<const uint8_t> salt,
                                                                                    std::string_view secret);

/// The hashes of the secrets of sessions on the desktop server: a random salt from the cryptography library and SHA-256
class Sha256SessionHasher final: public SessionHasherInterface
{
public:
	[[nodiscard]] std::optional<SessionHash> Hash(std::string_view secret) const override;
	[[nodiscard]] bool Matches(const SessionHash& hash, std::string_view secret) const override;
};

} // namespace openblack::server
