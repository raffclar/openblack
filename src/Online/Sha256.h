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

/// The SHA-256 of bytes that come in parts, for example the parts of a large file. The hash of all the parts is the hash
/// of the bytes in one part.
class Hasher
{
public:
	/// Adds the bytes after the bytes of the parts before
	void Add(std::span<const uint8_t> bytes) noexcept;
	/// The SHA-256 of all the bytes that the hasher got
	[[nodiscard]] Digest Finish() const noexcept;

private:
	static constexpr size_t k_BlockBytes = 64;

	/// The state starts with the first 32 bits of the fractional parts of the square roots of the first 8 primes
	std::array<uint32_t, 8> _state {
	    0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19,
	};
	/// The bytes after the last whole block
	std::array<uint8_t, k_BlockBytes> _rest {};
	size_t _restBytes {0};
	uint64_t _totalBytes {0};
};

/// The SHA-256 of the bytes
[[nodiscard]] Digest Of(std::span<const uint8_t> bytes) noexcept;

/// The SHA-256 of the bytes as 64 hex digits in lower case, as the map list of the lobby writes it
[[nodiscard]] std::string HexOf(std::span<const uint8_t> bytes);

/// A digest as 64 hex digits in lower case
[[nodiscard]] std::string Hex(const Digest& digest);

} // namespace openblack::online::sha256
