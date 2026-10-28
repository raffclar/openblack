/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "CreatureAnimationLogSystem.h"

#include <format>
#include <string>
#include <utility>

#include <entt/entity/entity.hpp>

#include "ECS/Systems/TimeSystemInterface.h"
#include "Locator.h"

using namespace openblack::ecs::systems;
using namespace openblack::creature_animation_log;

Log& CreatureAnimationLogSystem::LogOf(entt::entity creature)
{
	return _logs.try_emplace(creature).first->second;
}

void CreatureAnimationLogSystem::Posed(entt::entity creature, uint32_t turn, std::span<const Clip> clips, const Cause& cause)
{
	LogOf(creature).Posed(turn, clips, cause);
}

void CreatureAnimationLogSystem::Note(entt::entity creature, ProblemEntry problem)
{
	LogOf(creature).Note(std::move(problem));
}

void CreatureAnimationLogSystem::Refused(entt::entity creature, Layer layer, size_t animation, std::string_view by,
                                         size_t playing)
{
	const auto turn = Locator::time::has_value() ? Locator::time::value().GetTurn() : 0u;
	LogOf(creature).Note({.problem = Problem::Refused,
	                      .layer = layer,
	                      .animation = animation,
	                      .turn = turn,
	                      .by = std::string(by),
	                      .detail = std::format("the body played clip {} and kept it", playing)});
}

const Log* CreatureAnimationLogSystem::Find(entt::entity creature) const
{
	const auto found = _logs.find(creature);
	return found != _logs.end() ? &found->second : nullptr;
}

void CreatureAnimationLogSystem::Clear()
{
	_logs.clear();
}
