/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureDrink.h"

#include <limits>
#include <ranges>

#include <glm/geometric.hpp>

#include "Creature/CreatureLayers.h"

using namespace openblack;
using creature_mind::Movement;

std::optional<glm::vec2> creature_drink::NearestWaterPoint(glm::vec2 from, std::span<const glm::vec2> points)
{
	std::optional<glm::vec2> nearest;
	float best = std::numeric_limits<float>::max();
	// The game looks at the point that the land made last first, and keeps a point only when it is strictly nearer
	for (const auto& point : points | std::views::reverse)
	{
		const auto distance = glm::distance(from, point);
		if (distance < k_WaterPointRange && distance < best)
		{
			best = distance;
			nearest = point;
		}
	}
	return nearest;
}

std::optional<glm::vec2> creature_drink::StandingPoint(glm::vec3 wanted, bool wantedValid, std::optional<glm::vec2> found)
{
	if (wantedValid)
	{
		return glm::vec2(wanted.x, wanted.z);
	}
	if (found.has_value() && glm::distance(wanted, glm::vec3(found->x, 0.0f, found->y)) < k_StandSearchReach)
	{
		return found;
	}
	return std::nullopt;
}

std::vector<creature_mind::Step> creature_drink::Drink(const Place& place, float height)
{
	std::vector<Step> agenda;
	if (place.walkTo.has_value())
	{
		agenda.push_back({.kind = Step::Kind::Move,
		                  .movement = {.kind = Movement::Kind::ToPoint, .point = *place.walkTo, .maxDistance = height},
		                  .gaze = creature_mind::GoingLook(*place.walkTo)});
	}
	if (place.sea.has_value())
	{
		agenda.push_back({.kind = Step::Kind::Move,
		                  .movement = {.kind = Movement::Kind::TurnToFace, .point = *place.sea},
		                  .gaze = creature_mind::GoingLook(*place.sea)});
	}
	agenda.push_back({.kind = Step::Kind::Action,
	                  .animation = creature_layers::animations::k_Drink,
	                  .effect = creature_mind::Effect::Drink});
	return agenda;
}

void creature_drink::NoPlaceFound(creature_desires::Desires& desires, float turnsPerSecond)
{
	using creature_desires::Desire;
	creature_desires::Suppress(desires, Desire::Water, k_NoPlaceSuppressSeconds, turnsPerSecond);
	creature_desires::Suppress(desires, Desire::GetColder, k_NoPlaceSuppressSeconds, turnsPerSecond);
}
