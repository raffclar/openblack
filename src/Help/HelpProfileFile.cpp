/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "HelpProfileFile.h"

namespace openblack::help::profile
{

static_assert(helpstats::k_EventCount == k_EventCount);
static_assert(helpstats::k_TimesKept == k_TimesKept);

helpstats::HelpStats ToHelpStats(const HelpProfile& profile)
{
	helpstats::HelpStats stats {};
	for (uint32_t event = 0; event < k_EventCount; ++event)
	{
		const auto kept = profile.Count(event).GetKept();
		stats.at(event) = {
		    .total = kept.total,
		    .smoothedRate = kept.smoothedRate,
		    .timeHead = static_cast<uint8_t>(kept.head),
		    .timeCount = static_cast<uint8_t>(kept.kept),
		    .triggeredThisTurn = static_cast<uint8_t>(kept.triggeredThisTurn ? 1 : 0),
		    .padding = 0,
		    .times = kept.times,
		};
	}
	return stats;
}

void FromHelpStats(HelpProfile& profile, const helpstats::HelpStats& stats)
{
	for (uint32_t event = 0; event < k_EventCount; ++event)
	{
		const auto& record = stats.at(event);
		profile.Count(event).SetKept({
		    .total = record.total,
		    .smoothedRate = record.smoothedRate,
		    .head = record.timeHead,
		    .kept = record.timeCount,
		    .triggeredThisTurn = record.triggeredThisTurn != 0,
		    .times = record.times,
		});
	}
}

} // namespace openblack::help::profile
