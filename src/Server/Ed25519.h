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

#include <JoinTicket.h>

/// Ed25519 signatures, from the cryptography library of the desktop. The game server checks the lobby's join tickets
/// with them. The tests and the test tools also sign with a key that they make.
namespace openblack::ed25519
{

using Seed = std::array<uint8_t, 32>;

/// True when the signature of the bytes is valid for the public key
[[nodiscard]] bool Verify(const join_ticket::PublicKey& key, std::span<const uint8_t> bytes,
                          const join_ticket::Signature& signature);
/// The signature of the bytes with the secret seed. None when the library fails.
[[nodiscard]] std::optional<join_ticket::Signature> Sign(const Seed& seed, std::span<const uint8_t> bytes);
/// The public key of the secret seed. None when the library fails.
[[nodiscard]] std::optional<join_ticket::PublicKey> PublicKeyOf(const Seed& seed);

} // namespace openblack::ed25519
