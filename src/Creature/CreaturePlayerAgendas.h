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

#include <optional>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "Creature/CreatureIdleMind.h"

/// The agendas of the actions a creature does with its player when it wants attention or wants to tell the player what it
/// thinks. Pure, tested on made-up values.
namespace openblack::creature_player_attention
{
/// Going to the hand: it follows the hand, looking at it, and one time in two it then plays the happy clip
[[nodiscard]] std::vector<creature_mind::Step> GoToHand(const creature_mind::Random& random);
/// Pointing at the hand: it turns to the camera, looking at it, then points at where the hand is now for a second,
/// looking at that point and amazed
[[nodiscard]] std::vector<creature_mind::Step> PointAtHand(glm::vec2 camera, glm::vec3 hand);
/// Showing the player how nice it thinks the player is: it turns to the camera, then waves when its attitude to the
/// player is more than 0, else plays the sad clip. It looks at the camera all the time.
[[nodiscard]] std::vector<creature_mind::Step> ShowHowNice(glm::vec2 camera, float attitudeToPlayer);
/// Praying to the player: it turns to the camera, looking at it, then plays the prayer clip
[[nodiscard]] std::vector<creature_mind::Step> PrayToPlayer(glm::vec2 camera);
/// Throwing a thing at the camera: it picks the thing up when its hand is empty, looking at it while it goes, then
/// throws it at the camera, looking at the camera
[[nodiscard]] std::vector<creature_mind::Step> ThrowAtCamera(std::optional<uint32_t> pickUp, glm::vec3 camera);
/// Watching the player while it waits for orders
[[nodiscard]] std::vector<creature_mind::Step> WatchPlayer();
} // namespace openblack::creature_player_attention
