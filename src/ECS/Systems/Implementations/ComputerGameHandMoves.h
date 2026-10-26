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

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

#include "3D/MapCoords.h"
#include "ECS/Components/ComputerPlayer.h"
#include "ECS/ComputerHandMovesWorld.h"
#include "Enums.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "Locator interface implementations should only be included in Locator.cpp, use interface instead."
#endif

namespace openblack::ecs::systems::computer_world
{

/// The places and the heights that the moves of the god's hand read in the game's world
class GameHandMoves final: public computer_hand::MovesWorld
{
public:
	GameHandMoves(PlayerNames player, components::ComputerPlayer& state);

	[[nodiscard]] std::optional<map_coords::MapCoords> CoordsOf(entt::entity thing) const override;
	[[nodiscard]] float HeightOf(entt::entity thing) const override;
	[[nodiscard]] float LandHeight(const map_coords::MapCoords& coords) const override;

private:
	[[maybe_unused]] PlayerNames _player;
	[[maybe_unused]] components::ComputerPlayer& _state;
};

} // namespace openblack::ecs::systems::computer_world
