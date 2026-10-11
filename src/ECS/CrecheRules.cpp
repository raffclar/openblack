/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CrecheRules.h"

#include <algorithm>
#include <iterator>

namespace openblack::creche
{
namespace
{
/// A path drawn at random, the first when the drawing falls outside the paths there are
int32_t RandomPath(uint32_t paths, const IntRandom& random)
{
	const auto drawn = static_cast<int32_t>(random(paths));
	return drawn < 0 || drawn >= static_cast<int32_t>(paths) ? 0 : drawn;
}
} // namespace

NextLeg NextStrollLeg(Stroll stroll, uint32_t paths, bool atDoor, const IntRandom& random)
{
	const auto count = static_cast<int32_t>(paths);
	if (atDoor)
	{
		stroll = {.path = RandomPath(paths, random), .leg = 0};
	}
	else
	{
		// Past the paths there are, the last one (one before the first when there are none, which only ever leads to
		// the door)
		if (stroll.path < 0)
		{
			stroll.path = 0;
		}
		else if (stroll.path >= count)
		{
			stroll.path = count - 1;
		}
		++stroll.leg;
		if (stroll.leg < 0)
		{
			stroll.leg = 0;
		}
		else if (stroll.leg >= k_LegsPerStroll)
		{
			stroll = {.path = RandomPath(paths, random), .leg = 0};
		}
	}
	if (paths == 0)
	{
		return {.stroll = stroll, .point = std::nullopt};
	}
	// Out along the path's points, then back along them
	const int32_t along = stroll.leg < static_cast<int32_t>(k_PointsPerPath) ? stroll.leg : k_LegsPerStroll - 1 - stroll.leg;
	const auto point = static_cast<size_t>(stroll.path) * k_PointsPerPath + static_cast<size_t>(along);
	return {.stroll = stroll, .point = point};
}

glm::vec2 Strayed(glm::vec2 point, const FloatRandom& random)
{
	const float second = random(k_Stray) - k_Stray * 0.5f;
	const float first = random(k_Stray) - k_Stray * 0.5f;
	return {point.x + first, point.y + second};
}

std::optional<size_t> CrecheAfterLoss(bool lostWasTheTowns, std::span<const OtherBuilding> others)
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

} // namespace openblack::creche
