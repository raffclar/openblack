/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ThingWrites.h"

#include <bit>

namespace openblack::ecs
{

ThingWrites::ThingWrites() = default;

ThingWrites::~ThingWrites() = default;

std::optional<std::vector<uint32_t>> ThingWrites::Take()
{
	auto* words = _words.load(std::memory_order_acquire);
	if (words == nullptr)
	{
		// The marks start now: the changes before are not known
		_ownWords = std::make_unique<Words>();
		_words.store(_ownWords.get(), std::memory_order_release);
		_all.store(false, std::memory_order_release);
		return std::nullopt;
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
	if (_all.exchange(false, std::memory_order_acq_rel))
	{
		return std::nullopt;
	}
	return marked;
}

} // namespace openblack::ecs
