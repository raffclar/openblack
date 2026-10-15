/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <array>
#include <optional>
#include <vector>

#include <glm/vec2.hpp>

#include "Creature/CreatureIdleMind.h"

/// The agendas of the actions that use the creature's map of the land (see creature_exploration): it goes to a hill, a
/// coast or a town, walks along a ridge, sits on a beach, looks at the mountains or out to sea, and throws a thing into
/// the sea. Each place comes from the map before the agenda is made.
namespace openblack::creature_exploration
{
using creature_mind::Random;
using creature_mind::Step;

/// How near the creature goes to a hill or a coast when it looks, as a number of its heights
constexpr float k_LookFromHeights = 2.0f;
/// How near the creature goes to a hill, a coast or a town when it explores, as a number of its heights
constexpr float k_ExploreHeights = 3.0f;
/// The points of a walk along a ridge: eight points on a circle round the hill, 15 metres out
constexpr uint32_t k_RidgePoints = 8;
constexpr float k_RidgeRadius = 15.0f;
/// The creature points at the mountains one time in three, and out to sea one time in two, for 3 seconds
constexpr uint32_t k_PointAtMountainsLots = 3;
constexpr uint32_t k_PointOutToSeaLots = 2;
constexpr float k_PointSeconds = 3.0f;
/// On a beach the creature sits for 10 seconds and up to 15 seconds more
constexpr float k_BeachSitSeconds = 10.0f;
constexpr float k_BeachSitExtraSeconds = 15.0f;
/// The clear place to sit on a beach is as wide as four times the radius of the creature
constexpr float k_BeachClearRadii = 4.0f;

/// The points of a walk along the ridge of a hill, in order
[[nodiscard]] std::array<glm::vec2, k_RidgePoints> RidgePoints(glm::vec2 hill);

/// Going to a hill and looking about from it
[[nodiscard]] std::vector<Step> GoToHillAndLook(glm::vec2 hill, float height);
/// Going to a hill. The creature does not sit down there.
[[nodiscard]] std::vector<Step> GoToHillAndSit(glm::vec2 hill, float height);
/// Going to a hill, then walking along its ridge. The action is complete when the creature is at the hill.
[[nodiscard]] std::vector<Step> GoToHillAndWalkAlongRidge(glm::vec2 hill, float height);
/// Going to a coast or a town that the creature explores
[[nodiscard]] std::vector<Step> Explore(glm::vec2 place, float height);
/// Going to a clear place near the sea, turning to the sea and sitting a while
[[nodiscard]] std::vector<Step> SitDownOnBeach(glm::vec2 place, glm::vec2 sea, float height, const Random& random);
/// Turning to the mountains, pointing at them one time in three, and turning to them again
[[nodiscard]] std::vector<Step> LookAtMountains(glm::vec2 hill, const Random& random);
/// Going to the coast, turning to the sea and pointing at it one time in two
[[nodiscard]] std::vector<Step> LookOutToSea(glm::vec2 coast, glm::vec2 sea, float height, const Random& random);
/// Picking the thing up when the hand holds nothing, going to the coast and throwing the thing into the sea
[[nodiscard]] std::vector<Step> ThrowInTheSea(std::optional<uint32_t> pickUp, glm::vec2 coast, glm::vec2 sea, float height);
/// The creature goes to the coast with a smile, turns to the sea and dances alone with a playful face. The action is
/// complete when the dance ends.
[[nodiscard]] std::vector<Step> DanceByTheSea(glm::vec2 coast, glm::vec2 sea, float height);

} // namespace openblack::creature_exploration
