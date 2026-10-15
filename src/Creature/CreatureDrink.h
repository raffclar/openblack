/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <optional>
#include <span>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "Creature/CreatureDesires.h"
#include "Creature/CreatureIdleMind.h"

/// Where the creature drinks, and the agenda of the action that drinks from the sea. The land script marks the places
/// to drink with water points. The creature drinks at the nearest water point. With no water point near, it drinks at
/// the nearest coast of its map of the land.
namespace openblack::creature_drink
{
using creature_mind::Step;

/// The creature drinks only at a water point nearer than this, in metres
constexpr float k_WaterPointRange = 1000.0f;
/// When the creature cannot stand at the place to drink, it looks for the nearest place where it can stand. That place
/// must be nearer than this, in metres.
constexpr float k_StandSearchReach = 30.0f;
/// When the creature finds no place to drink, it does not want water or to get colder for this time, in seconds
constexpr float k_NoPlaceSuppressSeconds = 30.0f;

/// Where the creature drinks
struct Place
{
	/// The point that the creature walks to. None when it drinks at a coast and stands in the water already.
	std::optional<glm::vec2> walkTo;
	/// The sea that the creature turns to face before it drinks at a coast. None at a water point.
	std::optional<glm::vec2> sea;
};

/// The nearest water point to a place, nearer than k_WaterPointRange. The points are in the order that the land made
/// them. Of points at equal distance, the point that the land made last counts. None when no point is near.
[[nodiscard]] std::optional<glm::vec2> NearestWaterPoint(glm::vec2 from, std::span<const glm::vec2> points);

/// The point where the creature stands to drink:
/// - the wanted point, when the creature can stand there;
/// - else the point that the search found, when the creature can stand there and it is nearer than k_StandSearchReach.
/// The distance uses the height of the ground at the wanted point and a height of 0 at the found point, as the game
/// does. None when there is no such point.
[[nodiscard]] std::optional<glm::vec2> StandingPoint(glm::vec3 wanted, bool wantedValid, std::optional<glm::vec2> found);

/// The agenda of the action that drinks from the sea. The agenda has these steps:
/// 1. When the place has a point to walk to, the creature walks to within its height of that point.
/// 2. At a coast, it turns to face the sea.
/// 3. It plays its drink animation, and drinks.
[[nodiscard]] std::vector<Step> Drink(const Place& place, float height);

/// The creature found no place to drink, so the action fails. For k_NoPlaceSuppressSeconds it does not want water or to
/// get colder.
void NoPlaceFound(creature_desires::Desires& desires, float turnsPerSecond);

} // namespace openblack::creature_drink
