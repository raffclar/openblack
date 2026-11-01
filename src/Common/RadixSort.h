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

#include <algorithm>
#include <array>
#include <utility>
#include <vector>

/// A sort of unsigned 64-bit keys by their bytes, from the lowest byte to the highest. It keeps the order of equal keys,
/// so it gives the same order as any sort of the values. It takes a time in proportion to the count of keys, which is
/// less than a sort by comparisons for a large list.
namespace openblack::radix_sort
{

/// Lists with fewer keys than this are sorted by comparisons, which are faster for few keys
inline constexpr size_t k_SmallList = 64;

/// Sorts the keys from the least to the greatest. The scratch list keeps its memory for the next call.
inline void Sort(std::vector<uint64_t>& keys, std::vector<uint64_t>& scratch)
{
	const size_t count = keys.size();
	if (count < k_SmallList)
	{
		std::ranges::sort(keys);
		return;
	}
	constexpr size_t k_Bytes = sizeof(uint64_t);
	constexpr size_t k_Digits = 256;
	constexpr uint32_t k_BitsPerByte = 8;
	// The count of each value of each byte, all in one walk
	std::array<std::array<size_t, k_Digits>, k_Bytes> counts {};
	for (const auto key : keys)
	{
		for (size_t byte = 0; byte < k_Bytes; ++byte)
		{
			++counts.at(byte).at((key >> (byte * k_BitsPerByte)) & 0xFFu);
		}
	}
	scratch.resize(count);
	auto* from = &keys;
	auto* to = &scratch;
	for (size_t byte = 0; byte < k_Bytes; ++byte)
	{
		auto& digits = counts.at(byte);
		const auto shift = byte * k_BitsPerByte;
		// A byte that all the keys share does not change the order
		if (digits.at((from->front() >> shift) & 0xFFu) == count)
		{
			continue;
		}
		size_t start = 0;
		for (auto& digit : digits)
		{
			start += std::exchange(digit, start);
		}
		for (const auto key : *from)
		{
			(*to)[digits.at((key >> shift) & 0xFFu)++] = key;
		}
		std::swap(from, to);
	}
	if (from != &keys)
	{
		keys.swap(scratch);
	}
}

} // namespace openblack::radix_sort
