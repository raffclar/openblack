/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <array>
#include <span>
#include <utility>

namespace openblack
{

/// The quicksort of the C runtime the original game was built with, which is not stable: entries that compare equal end
/// up in the order its splits and swaps leave them, which decides which of equals the game takes first. `compare(a, b)`
/// is below nothing when a goes first, above when b does.
template <typename T, typename Compare>
void RuntimeQsort(std::span<T> entries, Compare compare)
{
	if (entries.size() < 2)
	{
		return;
	}
	const auto at = [&entries](int i) -> T& { return entries[static_cast<size_t>(i)]; };
	// A short stretch: the last of the greatest of [lo, hi] goes to hi each time
	const auto shortSort = [&](int lo, int hi) {
		while (hi > lo)
		{
			int max = lo;
			for (int p = lo + 1; p <= hi; ++p)
			{
				if (compare(at(p), at(max)) > 0)
				{
					max = p;
				}
			}
			std::swap(at(max), at(hi));
			--hi;
		}
	};
	// Short stretches are sorted directly; longer ones split about their middle, the smaller side first
	constexpr int k_Cutoff = 8;
	std::array<int, 30> loStack {};
	std::array<int, 30> hiStack {};
	int stack = 0;
	int lo = 0;
	int hi = static_cast<int>(entries.size()) - 1;
	for (;;)
	{
		const int size = hi - lo + 1;
		if (size <= k_Cutoff)
		{
			shortSort(lo, hi);
		}
		else
		{
			const int mid = lo + (size / 2);
			std::swap(at(mid), at(lo));
			int loGuy = lo;
			int hiGuy = hi + 1;
			for (;;)
			{
				do
				{
					++loGuy;
				} while (loGuy <= hi && compare(at(loGuy), at(lo)) <= 0);
				do
				{
					--hiGuy;
				} while (hiGuy > lo && compare(at(hiGuy), at(lo)) >= 0);
				if (hiGuy < loGuy)
				{
					break;
				}
				std::swap(at(loGuy), at(hiGuy));
			}
			std::swap(at(lo), at(hiGuy));
			if (hiGuy - lo > hi - loGuy)
			{
				if (lo + 1 < hiGuy)
				{
					loStack.at(static_cast<size_t>(stack)) = lo;
					hiStack.at(static_cast<size_t>(stack)) = hiGuy - 1;
					++stack;
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
					loStack.at(static_cast<size_t>(stack)) = loGuy;
					hiStack.at(static_cast<size_t>(stack)) = hi;
					++stack;
				}
				if (lo + 1 < hiGuy)
				{
					hi = hiGuy - 1;
					continue;
				}
			}
		}
		--stack;
		if (stack < 0)
		{
			return;
		}
		lo = loStack.at(static_cast<size_t>(stack));
		hi = hiStack.at(static_cast<size_t>(stack));
	}
}

} // namespace openblack
