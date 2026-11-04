/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "VillagerFootball.h"

#include <glm/gtx/vec_swizzle.hpp>

#include "3D/MapCoords.h"
#include "Common/GUtilsDistance.h"
#include "ECS/Components/Ball.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WallHug.h"
#include "ECS/FootballRules.h"
#include "ECS/Registry.h"
#include "ECS/Systems/FootballSystemInterface.h"
#include "ECS/Systems/LivingActionSystemInterface.h"
#include "ECS/WallHugRules.h"
#include "Locator.h"
#include "VillagerHome.h"
#include "VillagerMoveToObject.h"
#include "VillagerReactions.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;
namespace football = openblack::ecs::villager_football;
using football_rules::PlayState;

namespace
{

Registry& Entities()
{
	return Locator::entitiesRegistry::value();
}

entt::entity VillagerOf(const LivingAction& action)
{
	return Entities().ToEntity(action);
}

const systems::LivingActionSystemInterface& Living()
{
	return Locator::livingActionSystem::value();
}

/// The pitch of a villager, none when the game has no football or the villager has no pitch
entt::entity PitchOf(entt::entity villager)
{
	return Locator::footballSystem::has_value() ? Locator::footballSystem::value().PitchOf(villager) : entt::null;
}

entt::entity BallOf(entt::entity pitch)
{
	return pitch != entt::null ? Locator::footballSystem::value().BallOf(pitch) : entt::null;
}

std::optional<PlayState> PlayStateOf(entt::entity pitch)
{
	return pitch != entt::null ? Locator::footballSystem::value().PlayStateOf(pitch) : std::nullopt;
}

glm::vec3 PositionOf(entt::entity thing)
{
	return Entities().Get<const Transform>(thing).position;
}

bool HasTown(entt::entity villager)
{
	const auto* person = Entities().TryGet<const Villager>(villager);
	return person != nullptr && person->town != entt::null && Entities().Valid(person->town);
}

} // namespace

uint32_t football::WalkToPosition(LivingAction& action)
{
	return villager_home::MoveToPos(action);
}

uint32_t football::MoveToPitchConstruction(LivingAction& /*action*/)
{
	return 1;
}

uint32_t football::WaitForKickOff(LivingAction& action)
{
	if (PlayStateOf(PitchOf(VillagerOf(action))) == PlayState::Playing)
	{
		Living().VillagerChangeTopState(action, VillagerStates::FootballAttacker);
	}
	return 1;
}

uint32_t football::PlayRole(LivingAction& action)
{
	if (!Locator::footballSystem::has_value())
	{
		return villager_home::DecideWhatToDo(action);
	}
	return Locator::footballSystem::value().PlayRole(action);
}

uint32_t football::AfterGoal(LivingAction& /*action*/)
{
	return 1;
}

uint32_t football::StartMoveToPickUpBall(LivingAction& action)
{
	const auto ball = BallOf(PitchOf(VillagerOf(action)));
	if (ball == entt::null)
	{
		return 0;
	}
	villager_move_to_object::SetupMoveToObject(action, ball, VillagerStates::ArrivedAtPickUpBallForDeadBall);
	return 1;
}

uint32_t football::ArrivedAtPickUpBall(LivingAction& action)
{
	const auto villager = VillagerOf(action);
	if (const auto pitch = PitchOf(villager); pitch != entt::null)
	{
		systems::villager_reactions::LookAt(villager, PositionOf(pitch), systems::villager_reactions::LookSpeed::Quick);
	}
	Living().VillagerChangeTopState(action, VillagerStates::ArrivedAtPutDownBallForDeadBallStart);
	return 1;
}

uint32_t football::PutDownBallStart(LivingAction& action)
{
	Living().VillagerPlayAnimThenSetState(action, VillagerStates::ArrivedAtPutDownBallForDeadBallEnd);
	return 1;
}

uint32_t football::PutDownBallEnd(LivingAction& action)
{
	if (Locator::footballSystem::has_value())
	{
		Locator::footballSystem::value().RestartDeadBall(VillagerOf(action));
	}
	return 1;
}

uint32_t football::MatchPaused(LivingAction& action)
{
	const auto villager = VillagerOf(action);
	if (const auto ball = BallOf(PitchOf(villager)); ball != entt::null)
	{
		systems::villager_reactions::LookAt(villager, PositionOf(ball));
	}
	return 1;
}

uint32_t football::WatchMatch(LivingAction& action)
{
	const auto villager = VillagerOf(action);
	const auto pitch = PitchOf(villager);
	const auto ball = BallOf(pitch);
	if (!HasTown(villager) || ball == entt::null)
	{
		return villager_home::DecideWhatToDo(action);
	}
	switch (PlayStateOf(pitch).value_or(PlayState::Ended))
	{
	case PlayState::WaitingForPlayers:
		systems::villager_reactions::LookAt(villager, PositionOf(pitch));
		break;
	case PlayState::Playing:
		systems::villager_reactions::LookAt(villager, PositionOf(ball));
		break;
	case PlayState::Ended:
		if (villager_home::DecideWhatToDo(action) != 1)
		{
			villager_home::GoHome(action);
		}
		break;
	case PlayState::Paused:
	case PlayState::Scored:
		break;
	}
	return 1;
}

uint32_t football::MexicanWave(LivingAction& action)
{
	const auto villager = VillagerOf(action);
	const auto pitch = PitchOf(villager);
	if (!HasTown(villager) || pitch == entt::null)
	{
		return villager_home::DecideWhatToDo(action);
	}
	systems::villager_reactions::LookAt(villager, PositionOf(pitch), systems::villager_reactions::LookSpeed::Quick);
	Living().VillagerPlayAnimThenSetState(action, VillagerStates::FootballWatchMatch);
	return 1;
}

uint32_t football::MoveToBall(LivingAction& action)
{
	const auto villager = VillagerOf(action);
	const auto ball = BallOf(PitchOf(villager));
	if (ball == entt::null)
	{
		return villager_home::DecideWhatToDo(action);
	}
	auto& registry = Entities();
	// The player heads for the new point when the ball's point moved more than a metre
	if (const auto& destination = registry.Get<const Ball>(ball).destination; destination.has_value())
	{
		auto& walk = registry.Get<WallHug>(villager);
		const auto goal = map_coords::FromMetres(glm::xz(*destination));
		const map_coords::MapCoords current {walk.goalPlace.x, walk.goalPlace.y, 0.0f};
		if (gutils::GetDistanceInMetres(goal, current) > 1.0f)
		{
			walk.goalPlace = {goal.x, goal.z};
			walk.goalHeight = goal.altitude;
		}
	}
	return villager_home::MoveToPos(action);
}

bool football::ExitFootball(LivingAction& action, VillagerStates next)
{
	// The villager can always go. It stays in the match when it goes into a walk or into a different state of the match.
	if (!Living().VillagerHasSameExitAsFinal(action, next) && next != VillagerStates::MoveToPos &&
	    next != VillagerStates::MoveToObject && Locator::footballSystem::has_value())
	{
		Locator::footballSystem::value().Leave(VillagerOf(action));
	}
	return false;
}
