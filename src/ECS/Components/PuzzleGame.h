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

#include <array>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

#include "ScriptHeaders/ScriptEnums.h"

namespace openblack::ecs::components
{

/// A PuzzleGame, made by the CHL CREATE / CREATE_WITH_ANGLE_AND_SCALE of type 32 (position, sub type, angle x 2048 /
/// 2 pi, scale). Only the fish puzzle (type 14) is ported (ecs/PuzzleGames.h).
struct PuzzleGame
{
	script::PuzzleGameType type {script::PuzzleGameType::None};
	glm::vec3 position {0.0f}; ///< as a point: (x, altitude + y, z)
	int32_t angle {0};         ///< the 2048-step angle
	float scale {1.0f};
	bool played {false};                                         ///< once set the puzzle is not processed again
	entt::entity bait {entt::null};                              ///< type 14
	std::array<entt::entity, 2> shoals {entt::null, entt::null}; ///< type 14
};

} // namespace openblack::ecs::components
