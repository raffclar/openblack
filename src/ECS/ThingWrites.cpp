/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ThingWrites.h"

#include <algorithm>
#include <bit>

namespace openblack::ecs
{

ThingWrites::ThingWrites() = default;

ThingWrites::~ThingWrites() = default;

std::optional<std::vector<uint32_t>> ThingWrites::Take(Reader reader)
{
	const auto readerIndex = static_cast<size_t>(reader);
	const std::scoped_lock lock(_keptMutex);
	auto* words = _words.load(std::memory_order_acquire);
	if (words == nullptr)
	{
		// The marks start now
		_ownWords = std::make_unique<Words>();
		words = _ownWords.get();
		_words.store(words, std::memory_order_release);
	}
	std::vector<uint32_t> marked;
	for (size_t summaryWord = 0; summaryWord < k_SummaryWords; ++summaryWord)
	{
		auto summary = _summary.at(summaryWord).exchange(0, std::memory_order_acquire);
		while (summary != 0)
		{
			const auto wordIndex = summaryWord * 64 + static_cast<size_t>(std::countr_zero(summary));
			summary &= summary - 1;
			auto word = words->at(wordIndex).exchange(0, std::memory_order_relaxed);
			while (word != 0)
			{
				marked.push_back(static_cast<uint32_t>(wordIndex * 64 + static_cast<size_t>(std::countr_zero(word))));
				word &= word - 1;
			}
		}
	}
	// The other readers that started get these numbers at their next takes
	for (size_t other = 0; other < k_Readers; ++other)
	{
		auto& kept = _kept.at(other);
		if (other == readerIndex || !kept.started || kept.tooMany)
		{
			continue;
		}
		if (kept.numbers.size() + marked.size() > k_MostKept)
		{
			kept.tooMany = true;
			kept.numbers = {};
			continue;
		}
		kept.numbers.insert(kept.numbers.end(), marked.begin(), marked.end());
	}

	auto& own = _kept.at(readerIndex);
	const bool all = _all.at(readerIndex).exchange(false, std::memory_order_acq_rel);
	// The changes before the first take of the reader are not known
	if (!std::exchange(own.started, true) || std::exchange(own.tooMany, false) || all)
	{
		own.numbers = {};
		return std::nullopt;
	}
	if (own.numbers.empty())
	{
		return marked;
	}
	auto numbers = std::exchange(own.numbers, {});
	numbers.insert(numbers.end(), marked.begin(), marked.end());
	std::ranges::sort(numbers);
	const auto [repeatsFirst, repeatsLast] = std::ranges::unique(numbers);
	numbers.erase(repeatsFirst, repeatsLast);
	return numbers;
}

} // namespace openblack::ecs
