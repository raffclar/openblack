/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureClearWay.h"

#include <algorithm>

#include "Creature/CreatureDesires.h"

using namespace openblack;

std::optional<uint32_t> creature_clear_way::ClearingAction(const Obstacle& obstacle, std::span<const Miracle> miracles,
                                                           const Actions& actions)
{
	if (obstacle.templeHeart || obstacle.storagePit || obstacle.townCentre)
	{
		return std::nullopt;
	}
	if (obstacle.canPickUp)
	{
		return actions.throwAround;
	}
	if (obstacle.canKick)
	{
		return actions.kick;
	}
	const auto anger = static_cast<uint32_t>(creature_desires::Desire::Anger);
	const auto miracle =
	    std::ranges::find_if(miracles, [anger](const Miracle& m) { return m.known && m.action != 0 && m.desire == anger; });
	if (miracle == miracles.end())
	{
		return std::nullopt;
	}
	return miracle->action;
}

std::vector<uint32_t> creature_clear_way::ThingsToClear(std::span<const uint32_t> met,
                                                        const std::function<bool(uint32_t)>& clearable)
{
	std::vector<uint32_t> kept;
	for (const auto thing : met)
	{
		if (kept.size() >= k_MostToClear)
		{
			break;
		}
		if (std::ranges::find(kept, thing) == kept.end() && clearable(thing))
		{
			kept.push_back(thing);
		}
	}
	return kept;
}

std::optional<size_t> creature_clear_way::Choose(size_t count, const std::function<uint32_t(uint32_t)>& random)
{
	if (count == 0)
	{
		return std::nullopt;
	}
	const auto from = random(k_AnyOneIn) == 0 ? count : std::min(count, k_FirstFew);
	return static_cast<size_t>(random(static_cast<uint32_t>(from)));
}

float creature_clear_way::WalkOnDistance(float height)
{
	return std::max(height, k_LeastWalkOnDistance);
}

std::vector<creature_mind::Step> creature_clear_way::WalkOn(glm::vec2 destination, float height)
{
	using creature_mind::Movement;
	using creature_mind::Step;
	return {Step {.kind = Step::Kind::Move,
	              .movement = {.kind = Movement::Kind::ToPoint, .point = destination, .maxDistance = WalkOnDistance(height)},
	              .gaze = creature_mind::GoingLook()}};
}
