/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CameraBookmarks.h"

#include <cmath>

#include "Common/X87.h"

namespace openblack::camera_bookmarks
{

int32_t StepPulseClock(int32_t clockMs, int32_t frameMs)
{
	const int32_t clock = clockMs + frameMs;
	return clock > k_PulsePeriodMs ? clock % k_PulsePeriodMs : clock;
}

namespace
{

/// One side of the throb: the size of the cosine of the clock's angle, plus the least size
float Swing(int32_t clockMs)
{
	const float turns = static_cast<float>(clockMs) * k_PulseTurnsPerMs;
	const float angle = turns * k_PulseTurnAngle;
	return x87::Add(std::fabs(x87::Cos(angle)), k_PulseLeast);
}

} // namespace

glm::vec2 MarkerHalfSize(int32_t clockMs)
{
	return {Swing(clockMs), Swing((clockMs + k_PulseLagMs) % k_PulsePeriodMs)};
}

glm::vec3 MarkerPlace(glm::vec3 point)
{
	return point + glm::vec3(0.0f, k_MarkerLift, 0.0f);
}

std::optional<size_t> NextSet(size_t current, const std::array<bool, k_SteppedSlots>& set)
{
	for (size_t step = 1; step <= k_AllSlots; ++step)
	{
		const size_t slot = (current + step) % k_AllSlots;
		if (slot < k_SteppedSlots && set.at(slot))
		{
			return slot;
		}
	}
	return std::nullopt;
}

std::optional<size_t> PreviousSet(size_t current, const std::array<bool, k_SteppedSlots>& set)
{
	for (size_t step = k_AllSlots - 1; step >= 1; --step)
	{
		const size_t slot = (current + step) % k_AllSlots;
		if (slot < k_SteppedSlots && set.at(slot))
		{
			return slot;
		}
	}
	return std::nullopt;
}

FollowKey ShiftKey(bool onThing, std::optional<size_t> followed, size_t slot)
{
	if (!onThing)
	{
		return FollowKey::Jump;
	}
	return followed == slot ? FollowKey::Stop : FollowKey::Follow;
}

std::optional<size_t> NextTown(std::span<const TownStop> towns, std::optional<uint32_t> lastTown)
{
	if (towns.empty())
	{
		return std::nullopt;
	}
	// The first search looks only after the town of the last flight. The second search looks after every town.
	bool anyTown = !lastTown.has_value();
	while (true)
	{
		for (size_t i = towns.size(); i-- > 0;)
		{
			if (anyTown || towns[i].id == *lastTown)
			{
				const size_t before = i == 0 ? towns.size() - 1 : i - 1;
				if (towns[before].hasPlace)
				{
					return before;
				}
			}
		}
		if (anyTown)
		{
			return std::nullopt;
		}
		anyTown = true;
	}
}

glm::vec3 TownViewFrom(glm::vec3 place)
{
	return {place.x - k_TownViewX, place.y + k_TownViewY, place.z + k_TownViewZ};
}

} // namespace openblack::camera_bookmarks
