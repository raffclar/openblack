/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <glm/vec3.hpp>

#include "Enums.h"

/// The camera of a player as the turns of the world read it. The game keeps in the interface of each player where its
/// camera is and what it looks at, which the network game sends once a turn. The creatures read that camera of their
/// player, so that every computer of a network game has the same answers. A game with one player reads the camera of
/// this computer.
namespace openblack::ecs::player_camera
{

struct View
{
	glm::vec3 origin {0.0f};
	glm::vec3 focus {0.0f};
};

/// The camera of the player: in a network game the last report of the player's hand, else the camera of this computer
[[nodiscard]] View Of(PlayerNames player = PlayerNames::PLAYER_ONE);

} // namespace openblack::ecs::player_camera
