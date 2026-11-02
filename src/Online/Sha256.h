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
#include <span>
#include <string>

/// The SHA-256 hash (FIPS 180-4) of bytes. The map list of the lobby gives the SHA-256 of each map, and the game checks a
/// map that it gets against it. Every build has this code, the web build too, so no library is necessary.
namespace openblack::online::sha256
{

using Digest = std::array<uint8_t, 32>;

/// The SHA-256 of the bytes
[[nodiscard]] Digest Of(std::span<const uint8_t> bytes) noexcept;

/// The SHA-256 of the bytes as 64 hex digits in lower case, as the map list of the lobby writes it
[[nodiscard]] std::string HexOf(std::span<const uint8_t> bytes);

} // namespace openblack::online::sha256
