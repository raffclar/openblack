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
/// The agenda to go to the hand: the creature follows the hand and looks at it. One time in two it then plays the
/// happy clip.
[[nodiscard]] std::vector<creature_mind::Step> GoToHand(const creature_mind::Random& random);
/// The agenda to point at the hand: the creature turns to the camera and looks at it. Then it points for one second at
/// the position of the hand now. It looks at that position and is amazed.
[[nodiscard]] std::vector<creature_mind::Step> PointAtHand(glm::vec2 camera, glm::vec3 hand);
/// The agenda to show the player how nice the creature thinks the player is: the creature turns to the camera. Then it
/// waves if its attitude to the player is more than 0, else it plays the sad clip. It looks at the camera all the time.
[[nodiscard]] std::vector<creature_mind::Step> ShowHowNice(glm::vec2 camera, float attitudeToPlayer);
/// The agenda to pray to the player: the creature turns to the camera and looks at it. Then it plays the prayer clip.
[[nodiscard]] std::vector<creature_mind::Step> PrayToPlayer(glm::vec2 camera);
/// The agenda to throw a thing at the camera: if its hand is empty, the creature picks the thing up and looks at it.
/// Then it throws the thing at the camera and looks at the camera.
[[nodiscard]] std::vector<creature_mind::Step> ThrowAtCamera(std::optional<uint32_t> pickUp, glm::vec3 camera);
/// The agenda to go to the middle of the screen: the creature follows the point of focus of the camera. Then it turns to
/// the camera. It looks at the camera all the time.
[[nodiscard]] std::vector<creature_mind::Step> GoToMiddleOfScreen(glm::vec2 camera, float height);
/// The agenda to follow the player: the creature follows the point of focus of the camera and turns to the camera. It
/// looks at the camera. Then it plays the clip that asks the player to look at it.
[[nodiscard]] std::vector<creature_mind::Step> FollowPlayer(glm::vec2 camera, float height);
/// The agenda to be cross with the player: the creature follows the point of focus of the camera and turns to the
/// camera. It looks at the camera. Then one time in two it makes a rude sign, else it taunts the player.
[[nodiscard]] std::vector<creature_mind::Step> BeCrossWithPlayer(glm::vec2 camera, float height,
                                                                 const creature_mind::Random& random);
/// The agenda to show the player a thing: if its hand is empty, the creature picks the thing up. It follows the point of
/// focus of the camera, turns to the camera and puts the thing down. When it turns and puts the thing down, its face
/// shows its attitude to the player.
[[nodiscard]] std::vector<creature_mind::Step> ShowPlayerAnObject(std::optional<uint32_t> pickUp, glm::vec2 camera,
                                                                  float height);
/// The agenda to be silly with the player. The creature does these steps four times:
/// - It turns to the camera and talks. Its eyelids flutter.
/// - Three times in four, it shows that it feels playful.
/// - One time in two, it laughs and talks again.
/// - It pulls one of the ten faces for five seconds.
[[nodiscard]] std::vector<creature_mind::Step> BeSillyWithPlayer(glm::vec2 camera, const creature_mind::Random& random);
/// The agenda to watch the player while the creature waits for orders
[[nodiscard]] std::vector<creature_mind::Step> WatchPlayer();
} // namespace openblack::creature_player_attention
