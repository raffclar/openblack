/*******************************************************************************
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
#include <bit>
#include <span>
#include <type_traits>

/// Compares values by their bits, as 32-bit words. The differences of all the words are joined first, not compared one
/// by one: the byte compare of the C library is slow in WebAssembly, and the compiler can change a chain of compares into
/// a call to it.
namespace openblack::same_bits
{

/// Whether two runs of words are equal
[[nodiscard]] inline bool SameWords(std::span<const uint32_t> a, std::span<const uint32_t> b)
{
	if (a.size() != b.size())
	{
		return false;
	}
	uint32_t difference = 0;
	for (size_t i = 0; i < a.size(); ++i)
	{
		difference |= a[i] ^ b[i];
	}
	return difference == 0;
}

/// Whether two values have the same bits
template <typename Value>
[[nodiscard]] bool SameBits(const Value& a, const Value& b)
{
	static_assert(std::is_trivially_copyable_v<Value> && sizeof(Value) % sizeof(uint32_t) == 0);
	using Words = std::array<uint32_t, sizeof(Value) / sizeof(uint32_t)>;
	const auto wordsA = std::bit_cast<Words>(a);
	const auto wordsB = std::bit_cast<Words>(b);
	return SameWords(wordsA, wordsB);
}

} // namespace openblack::same_bits
