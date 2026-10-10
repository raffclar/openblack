/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "WalkArrival.h"

#include "ECS/WallHugRules.h"

namespace walk_arrival = openblack::ecs::walk_arrival;
using openblack::ecs::components::MoveState;

bool walk_arrival::WithinAStep(glm::ivec2 position, glm::ivec2 point, float metresPerSecond)
{
	return wall_hug::WithinStep(position, point, wall_hug::WholeSpeed(metresPerSecond));
}

bool walk_arrival::WalkIsOver(std::optional<MoveState> state, glm::ivec2 position, glm::ivec2 goal)
{
	if (!state.has_value())
	{
		return true;
	}
	switch (*state)
	{
	case MoveState::FinalStep:
	case MoveState::Arrived:
		return position == goal;
	case MoveState::Linear:
	case MoveState::Orbit:
	case MoveState::ExitCircle:
	case MoveState::StepThrough:
		return false;
	}
	return false;
}
