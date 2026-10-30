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
#include <cstdint>

#include <array>
#include <optional>
#include <string_view>

namespace openblack::server
{

/// The bytes of the random salt of a session hash
constexpr size_t k_SessionSaltBytes = 16;
/// The bytes of a SHA-256 digest
constexpr size_t k_SessionDigestBytes = 32;

/// A salted hash of the secret of a session. The server snapshots keep this hash and not the secret. Thus a person who
/// reads the files cannot come back into a god with the secret.
struct SessionHash
{
	std::array<uint8_t, k_SessionSaltBytes> salt {};
	/// The SHA-256 digest of the salt, then the secret
	std::array<uint8_t, k_SessionDigestBytes> digest {};

	bool operator==(const SessionHash&) const = default;
};

/// Makes and checks the hashes of the secrets of sessions. The desktop server uses the cryptography library of the
/// desktop. The tests use a fake.
class SessionHasherInterface
{
public:
	SessionHasherInterface() = default;
	virtual ~SessionHasherInterface() = default;
	SessionHasherInterface(const SessionHasherInterface&) = delete;
	SessionHasherInterface& operator=(const SessionHasherInterface&) = delete;
	SessionHasherInterface(SessionHasherInterface&&) = delete;
	SessionHasherInterface& operator=(SessionHasherInterface&&) = delete;

	/// The hash of the secret with a new random salt. None when the library fails.
	[[nodiscard]] virtual std::optional<SessionHash> Hash(std::string_view secret) const = 0;
	/// True when the secret has the hash. The compare of the digests takes the same time for all secrets.
	[[nodiscard]] virtual bool Matches(const SessionHash& hash, std::string_view secret) const = 0;
};

} // namespace openblack::server
