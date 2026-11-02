/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Sha256.h"

#include <algorithm>
#include <bit>
#include <string_view>

namespace openblack::online::sha256
{
namespace
{

/// The bytes in one block of the hash
constexpr size_t k_BlockBytes = 64;

/// The first 32 bits of the fractional parts of the cube roots of the first 64 primes
constexpr std::array<uint32_t, 64> k_Rounds {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01,
    0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
    0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da, 0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147,
    0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070, 0x19a4c116, 0x1e376c08,
    0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
    0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2,
};

/// The first 32 bits of the fractional parts of the square roots of the first 8 primes
constexpr std::array<uint32_t, 8> k_Start {
    0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19,
};

/// Add one block of 64 bytes to the state
void AddBlock(std::array<uint32_t, 8>& state, std::span<const uint8_t, k_BlockBytes> block) noexcept
{
	std::array<uint32_t, 64> words {};
	for (size_t i = 0; i < 16; ++i)
	{
		words.at(i) = (static_cast<uint32_t>(block[i * 4]) << 24) | (static_cast<uint32_t>(block[i * 4 + 1]) << 16) |
		              (static_cast<uint32_t>(block[i * 4 + 2]) << 8) | static_cast<uint32_t>(block[i * 4 + 3]);
	}
	for (size_t i = 16; i < words.size(); ++i)
	{
		const auto before15 = words.at(i - 15);
		const auto before2 = words.at(i - 2);
		const auto s0 = std::rotr(before15, 7) ^ std::rotr(before15, 18) ^ (before15 >> 3);
		const auto s1 = std::rotr(before2, 17) ^ std::rotr(before2, 19) ^ (before2 >> 10);
		words.at(i) = words.at(i - 16) + s0 + words.at(i - 7) + s1;
	}
	auto [a, b, c, d, e, f, g, h] = state;
	for (size_t i = 0; i < words.size(); ++i)
	{
		const auto s1 = std::rotr(e, 6) ^ std::rotr(e, 11) ^ std::rotr(e, 25);
		const auto choose = (e & f) ^ (~e & g);
		const auto first = h + s1 + choose + k_Rounds.at(i) + words.at(i);
		const auto s0 = std::rotr(a, 2) ^ std::rotr(a, 13) ^ std::rotr(a, 22);
		const auto majority = (a & b) ^ (a & c) ^ (b & c);
		const auto second = s0 + majority;
		h = g;
		g = f;
		f = e;
		e = d + first;
		d = c;
		c = b;
		b = a;
		a = first + second;
	}
	const std::array<uint32_t, 8> added {a, b, c, d, e, f, g, h};
	for (size_t i = 0; i < state.size(); ++i)
	{
		state.at(i) += added.at(i);
	}
}

} // namespace

Digest Of(std::span<const uint8_t> bytes) noexcept
{
	auto state = k_Start;
	const auto whole = bytes.size() / k_BlockBytes;
	for (size_t i = 0; i < whole; ++i)
	{
		AddBlock(state, bytes.subspan(i * k_BlockBytes).first<k_BlockBytes>());
	}
	// The last bytes, then the byte 0x80, then zeros, then the length in bits as 8 bytes, high byte first. This makes one
	// or two blocks.
	const auto rest = bytes.subspan(whole * k_BlockBytes);
	std::array<uint8_t, k_BlockBytes * 2> tail {};
	std::ranges::copy(rest, tail.begin());
	tail.at(rest.size()) = 0x80;
	const size_t tailBytes = rest.size() + 1 + 8 <= k_BlockBytes ? k_BlockBytes : k_BlockBytes * 2;
	const uint64_t bits = static_cast<uint64_t>(bytes.size()) * 8;
	for (size_t i = 0; i < 8; ++i)
	{
		tail.at(tailBytes - 1 - i) = static_cast<uint8_t>(bits >> (i * 8));
	}
	for (size_t offset = 0; offset < tailBytes; offset += k_BlockBytes)
	{
		AddBlock(state, std::span<const uint8_t>(tail).subspan(offset).first<k_BlockBytes>());
	}
	Digest digest {};
	for (size_t i = 0; i < state.size(); ++i)
	{
		for (size_t j = 0; j < 4; ++j)
		{
			digest.at(i * 4 + j) = static_cast<uint8_t>(state.at(i) >> (24 - j * 8));
		}
	}
	return digest;
}

std::string HexOf(std::span<const uint8_t> bytes)
{
	constexpr std::string_view k_Digits = "0123456789abcdef";
	std::string text;
	text.reserve(64);
	for (const auto byte : Of(bytes))
	{
		text.push_back(k_Digits[byte >> 4]);
		text.push_back(k_Digits[byte & 0x0F]);
	}
	return text;
}

} // namespace openblack::online::sha256
