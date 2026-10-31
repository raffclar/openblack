/*******************************************************************************
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

#include <entt/entity/fwd.hpp>

#include "ECS/Systems/HandSystemInterface.h"
#include "Enums.h"
#include "Hand/GodHand.h"

/// The cameras of the players as the turns of the world read them. Each player reports where its camera is and what it
/// looks at once a turn, and all the computers of a network game read the same reports.
namespace openblack::ecs
{

/// Rules for the camera that a creature reads, free of the world so that tests can use them
namespace reported_camera
{

/// The player whose camera a creature reads when it asks for the camera nearest to it. It is the creature's own player.
/// A creature with no player reads no camera in a network game. In a game alone it reads the camera of the player of
/// this computer.
[[nodiscard]] constexpr std::optional<PlayerNames> NearestCameraPlayer(std::optional<PlayerNames> creaturePlayer,
                                                                       bool networkGame, PlayerNames localPlayer) noexcept
{
	if (creaturePlayer.has_value())
	{
		return creaturePlayer;
	}
	return networkGame ? std::nullopt : std::optional(localPlayer);
}

} // namespace reported_camera

/// Where the world has the camera of a player, from the last report of the player that a turn did. None before the
/// first report, or for a player with no hands.
[[nodiscard]] std::optional<systems::HandSystemInterface::ReportedCamera> ReportedPlayerCamera(PlayerNames player);

/// The camera of a player as the creatures read it. A person's player has its reported camera. Each other god has the
/// camera of its citadel and its computer mind (see hand_start::GodCameraOf), in a game alone and in a network game.
/// None for a player that the world does not have.
[[nodiscard]] std::optional<systems::HandSystemInterface::ReportedCamera> PlayerCameraOf(PlayerNames player);

/// The player whose hand a creature reads: the creature's own player, in a game alone and in a network game. A creature
/// with no player (the neutral player) reads no hand.
[[nodiscard]] std::optional<PlayerNames> ReportedCreaturePlayer(entt::entity creature);

/// The hands of a player as the creatures read them (see god_hand::HandsOf). A person's player has its reported hands.
/// Every other god has one hand, which can be at the origin of the world.
[[nodiscard]] god_hand::Hands PlayerHandsOf(PlayerNames player);

/// The hand of a player that is nearest to a point (see god_hand::NearestOf). None for a player with no hands.
[[nodiscard]] std::optional<glm::vec3> NearestHandOf(PlayerNames player, const glm::vec3& point);

/// The game turns since the hand of a player last moved, as the creatures read them (see hand_stillness::Step)
[[nodiscard]] uint32_t PlayerHandStillTurnsOf(PlayerNames player);

/// The velocity of the hand of a player, as the creatures read it (see hand_stillness::Step). Zero for a player with no
/// hands.
[[nodiscard]] glm::vec3 PlayerHandVelocityOf(PlayerNames player);

/// The camera nearest to a creature: the camera of its own player (see PlayerCameraOf). A creature with no player reads
/// the camera of the player of this computer in a game alone, and no camera in a network game.
[[nodiscard]] std::optional<systems::HandSystemInterface::ReportedCamera> ReportedCreatureCamera(entt::entity creature);

/// The camera of the creature's own player (see PlayerCameraOf). A creature with no player (the neutral player) has
/// none, in a game alone too. The creature looks, turns and walks to this camera, and the test whether the player looks
/// at the creature uses it.
[[nodiscard]] std::optional<systems::HandSystemInterface::ReportedCamera> ReportedCameraOfItsPlayer(entt::entity creature);

} // namespace openblack::ecs
