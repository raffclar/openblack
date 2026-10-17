/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "GraveyardRules.h"

#include <algorithm>
#include <iterator>

#include "Common/X87.h"

namespace openblack::graveyard
{

uint32_t GravesShown(uint32_t burials)
{
	if (burials == 0)
	{
		return 0;
	}
	// The share rounds to a float, then it is cut down to whole graves
	const auto graves = static_cast<uint32_t>(static_cast<int64_t>(x87::Mul(static_cast<double>(burials), k_GravePerBurial)));
	return std::max(graves, 1u);
}

std::optional<StandInBurial> BurialOnStandIn(uint32_t count)
{
	if (count >= k_MostBurials)
	{
		return std::nullopt;
	}
	const auto counted = count + 1;
	return StandInBurial {.count = counted, .partsShown = GravesShown(counted)};
}

std::optional<size_t> GraveyardAfterLoss(bool lostWasTheTowns, std::span<const OtherBuilding> others)
{
	if (!lostWasTheTowns)
	{
		return std::nullopt;
	}
	const auto found =
	    std::ranges::find_if(others, [](const OtherBuilding& other) { return other.working && other.takesOver; });
	if (found == others.end())
	{
		return std::nullopt;
	}
	return static_cast<size_t>(std::distance(others.begin(), found));
}

} // namespace openblack::graveyard
