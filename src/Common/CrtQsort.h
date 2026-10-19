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

#include <array>
#include <span>
#include <utility>

/// The sort of the C library that the game links: a quicksort that is not stable. Things that compare as equal come out
/// in the order that this sort gives them, so a port that must draw or step things in the game's order sorts with it.
namespace openblack::crt_qsort
{

/// Sorts the items in place as the game's C library does. compare(a, b) gives a negative number, 0 or a positive number,
/// as the C library's comparison does.
template <typename T, size_t Extent, typename Compare>
void Sort(std::span<T, Extent> items, Compare compare)
{
	// Parts of this many items or fewer get a selection sort
	constexpr std::ptrdiff_t k_Cutoff = 8;
	if (items.size() < 2)
	{
		return;
	}
	const auto shortSort = [&items, &compare](std::ptrdiff_t lo, std::ptrdiff_t hi) {
		// Put the largest item last, then the next largest before it, and so on
		while (hi > lo)
		{
			auto max = lo;
			for (auto p = lo + 1; p <= hi; ++p)
			{
				if (compare(items[p], items[max]) > 0)
				{
					max = p;
				}
			}
			std::swap(items[max], items[hi]);
			--hi;
		}
	};
	// The parts that wait to be sorted. The smaller part is always sorted first, so 30 places are sufficient.
	std::array<std::ptrdiff_t, 30> loStack {};
	std::array<std::ptrdiff_t, 30> hiStack {};
	std::ptrdiff_t stackSize = 0;
	std::ptrdiff_t lo = 0;
	std::ptrdiff_t hi = static_cast<std::ptrdiff_t>(items.size()) - 1;
	while (true)
	{
		const auto size = hi - lo + 1;
		if (size <= k_Cutoff)
		{
			shortSort(lo, hi);
		}
		else
		{
			// The middle item becomes the pivot, at the start of the part
			std::swap(items[lo + (size / 2)], items[lo]);
			auto loGuy = lo;
			auto hiGuy = hi + 1;
			while (true)
			{
				do
				{
					++loGuy;
				} while (loGuy <= hi && compare(items[loGuy], items[lo]) <= 0);
				do
				{
					--hiGuy;
				} while (hiGuy > lo && compare(items[hiGuy], items[lo]) >= 0);
				if (hiGuy < loGuy)
				{
					break;
				}
				std::swap(items[loGuy], items[hiGuy]);
			}
			std::swap(items[lo], items[hiGuy]);
			// The two parts on each side of the pivot: the larger part waits on the stack
			if (hiGuy - 1 - lo >= hi - loGuy)
			{
				if (lo + 1 < hiGuy)
				{
					loStack.at(static_cast<size_t>(stackSize)) = lo;
					hiStack.at(static_cast<size_t>(stackSize)) = hiGuy - 1;
					++stackSize;
				}
				if (loGuy < hi)
				{
					lo = loGuy;
					continue;
				}
			}
			else
			{
				if (loGuy < hi)
				{
					loStack.at(static_cast<size_t>(stackSize)) = loGuy;
					hiStack.at(static_cast<size_t>(stackSize)) = hi;
					++stackSize;
				}
				if (lo + 1 < hiGuy)
				{
					hi = hiGuy - 1;
					continue;
				}
			}
		}
		--stackSize;
		if (stackSize < 0)
		{
			return;
		}
		lo = loStack.at(static_cast<size_t>(stackSize));
		hi = hiStack.at(static_cast<size_t>(stackSize));
	}
}

} // namespace openblack::crt_qsort
