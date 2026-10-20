/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <entt/entity/entity.hpp>
#include <glm/vec2.hpp>

namespace openblack::ecs::systems
{
class PathfindingSystemInterface
{
public:
	/// Every walker takes its turn's walk
	virtual void Update() = 0;
	/// At the end of the turn, one walker that came up against water, the land's edge or a fence while going round
	/// tries its walk both ways round, and is turned round if the other way is better
	virtual void ProcessLookahead() = 0;
	/// A walker goes straight on to its goal again, with no side to keep, and looks along its step from where it stands
	/// for what is in its way
	virtual void LookAgain(entt::entity walker) = 0;
	/// A walker starts to walk straight to a new goal (in whole map units), facing it, and looks along its step for what is in
	/// its way
	virtual void WalkTo(entt::entity walker, glm::ivec2 goal) = 0;
	/// A walker that walks in its own turn starts to walk round what is in its way to a new goal (in whole map units).
	/// It aims at the goal and looks along its step at once, or is there already.
	virtual void SetupWalkRound(entt::entity walker, glm::ivec2 goal) = 0;
	/// A walker that walks in its own turn takes its turn's walk; true when it has got there, on its goal
	virtual bool WalkOne(entt::entity walker) = 0;
};
} // namespace openblack::ecs::systems
