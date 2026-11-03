/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The world's side of the football actions of a creature

#define LOCATOR_IMPLEMENTATIONS

#include "CreatureMindFootball.h"

#include <glm/gtx/vec_swizzle.hpp>

#include "Creature/CreatureFootball.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/Villager.h"
#include "ECS/Registry.h"
#include "ECS/Systems/FootballSystemInterface.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::components;
using creature_plan_actions::Build;
using creature_plan_actions::Target;

namespace
{
/// Whether the creature plays for the home team: the creature of a player with an even number does
bool HomeTeamOf(const ecs::Registry& registry, entt::entity creature)
{
	const auto* body = registry.TryGet<const Creature>(creature);
	return body != nullptr && creature_football::IsOnHomeTeam(static_cast<uint32_t>(body->owner));
}
} // namespace

bool ecs::systems::mind_football::IsPlaying(const ecs::Registry& registry, entt::entity thing)
{
	return registry.Valid(thing) && registry.AllOf<Villager>(thing) && Locator::footballSystem::has_value() &&
	       Locator::footballSystem::value().IsPlaying(thing);
}

bool ecs::systems::mind_football::Accepts(const ecs::Registry& registry, Target target, entt::entity thing, entt::entity self)
{
	if (!IsPlaying(registry, thing))
	{
		return false;
	}
	if (target == Target::Footballer)
	{
		return true;
	}
	const auto& football = Locator::footballSystem::value();
	const auto pitch = football.PitchOf(thing);
	const auto homeMourns = pitch != entt::null ? football.HomeMournsGoal(pitch) : std::nullopt;
	const bool homeTeam = HomeTeamOf(registry, self);
	switch (target)
	{
	case Target::FootballerMySideScored:
		return creature_football::MySideJustScored(homeMourns.has_value(), homeMourns.value_or(false), homeTeam);
	case Target::FootballerOtherSideScored:
		return creature_football::OtherSideJustScored(homeMourns.has_value(), homeMourns.value_or(false), homeTeam);
	default:
		return false;
	}
}

void ecs::systems::mind_football::Situate(const ecs::Registry& registry, entt::entity creature,
                                          const creature_plan_actions::Executor& executor, std::optional<uint32_t> object,
                                          creature_plan_actions::Situation& situation)
{
	if (executor.build != Build::ThrowBallAtGoal && executor.build != Build::StompOnBall &&
	    executor.build != Build::GoalkeeperFoul)
	{
		return;
	}
	const auto thing = object.has_value() ? static_cast<entt::entity>(*object) : entt::null;
	if (!registry.Valid(thing) || !Locator::footballSystem::has_value())
	{
		return;
	}
	auto& football = Locator::footballSystem::value();
	const auto pitch = football.PitchOf(thing);
	if (pitch == entt::null)
	{
		return;
	}
	// The pitch makes a new ball when it has none
	const auto ball = football.BallOf(pitch);
	if (ball == entt::null)
	{
		return;
	}
	const bool homeTeam = HomeTeamOf(registry, creature);
	const auto other = football.FirstPlayer(pitch, !homeTeam);
	situation.football = creature_plan_actions::Situation::Football {
	    .ball = entt::to_integral(ball),
	    .goal = glm::xz(football.AttackedGoalPosition(pitch, homeTeam)),
	    .otherFirstPlayer = other != entt::null ? std::optional(entt::to_integral(other)) : std::nullopt,
	};
}
