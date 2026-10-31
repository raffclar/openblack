/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <optional>

#include <entt/entity/fwd.hpp>

#include "ECS/Systems/HandSystemInterface.h"
#include "Enums.h"

/// The cameras of the players as the turns of the world read them. Each player reports where its camera is and what it
/// looks at once a turn, and all the computers of a network game read the same reports.
namespace openblack::ecs
{

/// Where the world has the camera of a player, from the last report of the player that a turn did. None before the
/// first report, or for a player with no hands.
[[nodiscard]] std::optional<systems::HandSystemInterface::ReportedCamera> ReportedPlayerCamera(PlayerNames player);

/// The player whose camera and hand a creature reads. In a network game it is the creature's player, and a creature with
/// no player reads no player. A game with one player has one camera and one hand, which every creature reads.
[[nodiscard]] std::optional<PlayerNames> ReportedCreaturePlayer(entt::entity creature);

/// The reported camera that a creature reads: the camera of the player that ReportedCreaturePlayer gives
[[nodiscard]] std::optional<systems::HandSystemInterface::ReportedCamera> ReportedCreatureCamera(entt::entity creature);

/// The camera of the creature's own player, which looks at the creature. A person's player has its reported camera. Each
/// other god has the camera of its citadel and its computer mind (see hand_start::GodCameraOf), in a game alone and in a
/// network game. A creature of the neutral player has none.
[[nodiscard]] std::optional<systems::HandSystemInterface::ReportedCamera> ReportedCameraOfItsPlayer(entt::entity creature);

} // namespace openblack::ecs
