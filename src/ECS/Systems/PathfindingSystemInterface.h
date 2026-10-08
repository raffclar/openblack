/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <entt/entity/fwd.hpp>

namespace openblack::ecs::systems
{
class PathfindingSystemInterface
{
public:
	virtual ~PathfindingSystemInterface() = default;

	/// This turn's step of one villager's wall-hugging walk. The state functions that walk call it
	/// (living_turn::MoveToStep, ECS/LivingTurn.h)
	virtual void Step(entt::entity entity) = 0;
};
} // namespace openblack::ecs::systems
