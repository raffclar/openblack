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

#include <vector>

#include "Creature/CreatureIdleMind.h"

/// The two dances that a creature starts away from the villagers: the dance outside its player's temple and the dance
/// around a thing. Neither makes a dance of its own. The creature goes near the place, starts the last dance it made and
/// joins that dance. When the creature has made no dance before, nothing starts and the action fails after the walk.
namespace openblack::creature_temple_dance
{

/// The creature goes near the heart of its player's temple to this many times its height, and near the thing of the
/// dance around a thing to this many times its height, never further than k_FurthestApproach
constexpr float k_TempleApproachHeights = 6.0f;
constexpr float k_ThingApproachHeights = 4.0f;
constexpr float k_FurthestApproach = 50.0f;
/// It faces the heart of the temple for this many seconds before the dance
constexpr float k_FaceHeartSeconds = 2.0f;
/// It dances for this many seconds
constexpr float k_DanceSeconds = 60.0f;

/// How near the creature goes, for its height and the number of heights
[[nodiscard]] float ApproachDistance(float height, float heights);

/// The agenda of the dance outside the temple: go near the heart (the temple, by its entity's number), start the last
/// dance, face the heart, and dance
[[nodiscard]] std::vector<creature_mind::Step> DanceOutsideTemple(uint32_t temple, float height, float turnsPerSecond);
/// The agenda of the dance around a thing: go near the thing, start the last dance, and dance
[[nodiscard]] std::vector<creature_mind::Step> DanceAroundThing(uint32_t thing, float height, float turnsPerSecond);

} // namespace openblack::creature_temple_dance
