/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "VillagerAnimate.h"

#include <glm/vec2.hpp>

#include "3D/LandIslandInterface.h"
#include "Common/GameRandom.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/LivingPhysics.h"
#include "ECS/Components/Poisoned.h"
#include "ECS/Components/ScriptControl.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/VillagerPose.h"
#include "ECS/Components/WallHug.h"
#include "ECS/Dances.h"
#include "ECS/Registry.h"
#include "ECS/VillagerAnimation.h"
#include "ECS/VillagerClips.h"
#include "ECS/WorldObjects.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "VillagerDance.h"
#include "VillagerScript.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;
namespace animation = openblack::ecs::villager_animation;

namespace
{
auto& Entities()
{
	return Locator::entitiesRegistry::value();
}

VillagerStates TopOf(const LivingAction& action)
{
	return static_cast<VillagerStates>(action.states.at(static_cast<size_t>(LivingAction::Index::Top)));
}

const GVillagerStateTableInfo& StateInfo(VillagerStates state)
{
	return Locator::infoConstants::value().villagerStateTable.at(static_cast<size_t>(state));
}

const GVillagerInfo& InfoOf(const Villager& villager)
{
	const auto& infos = Locator::infoConstants::value().villager;
	return infos.at(static_cast<size_t>(GVillagerInfo::Find(villager.tribe, villager.number)));
}

/// The state its top state works towards: the top state itself when that is a final one
VillagerStates FinalStateOf(const LivingAction& action)
{
	const auto top = TopOf(action);
	if (StateInfo(top).isFinalState != 0)
	{
		return top;
	}
	return static_cast<VillagerStates>(action.states.at(static_cast<size_t>(LivingAction::Index::Final)));
}

animation::IntRandom GameRandom()
{
	return [](uint32_t limit) { return Locator::gameRandom::value().GameRand(limit); };
}

bool IsWoman(const Villager& villager)
{
	return villager.sex == Villager::Sex::FEMALE;
}

bool IsWomanOrChild(const Villager& villager)
{
	return IsWoman(villager) || villager.lifeStage == Villager::LifeStage::Child;
}

/// Works out what it carries for its states and load, as the game does whenever it looks at its clip
void SetStateCarriedObject(entt::entity entity, const LivingAction& action, Villager& villager)
{
	const auto top = TopOf(action);
	const auto final = FinalStateOf(action);
	const auto& info = InfoOf(villager);
	// TODO(villagers): wood and food carried come with the jobs' carrying
	const animation::CarryInputs in {
	    .scriptHeld = final == VillagerStates::InScript || top == VillagerStates::ScriptPlayAnim,
	    .life = world_objects::LifeOf(entity),
	    .lifeWhenCrawlsWounded = info.lifeWhenCrawlsWounded,
	    .minWoodToShowGraphic = info.minWoodToShowGraphic,
	    .minFoodToShowGraphic = info.minFoodToShowGraphic,
	    .building = final == VillagerStates::Building,
	    .finalStateCarries = StateInfo(final).carriedObject,
	    .topStateCarries = StateInfo(top).carriedObject,
	};
	if (const auto carried = animation::CarriedObjectOf(in); carried.has_value())
	{
		villager.carried = *carried;
	}
}

AnimId WalkClipOf(entt::entity entity, const Villager& villager)
{
	auto& registry = Entities();
	const auto& info = InfoOf(villager);
	const auto& thresholds = Locator::infoConstants::value().speedThreshold;
	const auto& threshold =
	    thresholds.at(static_cast<size_t>(IsWoman(villager) ? SpeedThreshold::VillagerNormal : SpeedThreshold::VillagerMan));
	const auto* wallHug = registry.TryGet<const WallHug>(entity);
	return animation::WalkClip({
	    .carried = villager.carried,
	    .scriptControlled = registry.AllOf<ScriptControlled>(entity),
	    .life = world_objects::LifeOf(entity),
	    .lifeWhenCrawlsWounded = info.lifeWhenCrawlsWounded,
	    .lifeWhenWalksWounded = info.lifeWhenWalksWounded,
	    .female = IsWoman(villager),
	    .speed = wallHug != nullptr ? wallHug->speed : 0.0f,
	    .walkMax = GetSpeedStateSpeed(threshold.speedMaxWalk),
	    .runMax = GetSpeedStateSpeed(threshold.speedMaxRun),
	});
}

float WatchedHeightAboveLand(entt::entity entity)
{
	auto& registry = Entities();
	const auto* watched = registry.TryGet<const WatchedFlyingObject>(entity);
	if (watched == nullptr || !registry.Valid(watched->object) || !Locator::terrainSystem::has_value())
	{
		return 0.0f;
	}
	const auto* transform = registry.TryGet<const Transform>(watched->object);
	if (transform == nullptr)
	{
		return 0.0f;
	}
	const auto& position = transform->position;
	return position.y - Locator::terrainSystem::value().GetHeightAt(glm::vec2(position.x, position.z));
}

int32_t Clip(AnimId clip)
{
	return static_cast<int32_t>(clip);
}
} // namespace

int32_t villager_animate::StateClip(entt::entity entity)
{
	auto& registry = Entities();
	const auto& action = registry.Get<const LivingAction>(entity);
	auto& villager = registry.Get<Villager>(entity);
	const auto state = TopOf(action);
	if (state == VillagerStates::InvalidState || state == VillagerStates::LastState)
	{
		return Clip(AnimId::PStand);
	}
	SetStateCarriedObject(entity, action, villager);
	const auto random = GameRandom();
	switch (animation::ClipChoiceOf(state))
	{
	case animation::ClipChoice::Walk:
		return Clip(WalkClipOf(entity, villager));
	case animation::ClipChoice::Thrown:
	case animation::ClipChoice::Landed:
	case animation::ClipChoice::PointAtFlyingObject:
		// Chosen by the physics as it puts the villager in these states
		return -1;
	case animation::ClipChoice::Dying:
		// TODO(villagers): the way it landed decides whether it falls on its back
		return Clip(animation::DyingClip(villager_clips::IsOnWater(registry.Get<const Transform>(entity).position), false));
	case animation::ClipChoice::Dead:
		return Clip(animation::DeadClip(villager_clips::IsOnWater(registry.Get<const Transform>(entity).position), false));
	case animation::ClipChoice::Kissing:
		return Clip(animation::KissingClip(IsWoman(villager)));
	case animation::ClipChoice::Forestering:
		return Clip(AnimId::PChoppingTree);
	case animation::ClipChoice::Yawn:
		return Clip(animation::YawnClip(random));
	case animation::ClipChoice::PauseForASecond:
		return Clip(animation::PauseForASecondClip(registry.AllOf<Poisoned>(entity), random));
	case animation::ClipChoice::LookAtLargeObject:
		return Clip(animation::LookAtLargeObjectClip(action.turnsUntilStateChange));
	case animation::ClipChoice::LookAtFlyingObject:
	{
		const auto previous = static_cast<VillagerStates>(action.states.at(static_cast<size_t>(LivingAction::Index::Previous)));
		return animation::LookAtFlyingObjectClip(StateInfo(previous).animation, WatchedHeightAboveLand(entity));
	}
	case animation::ClipChoice::InspectCreature:
		// TODO(villagers): how near the creature it inspects is, once creature reactions are ported
		return Clip(animation::InspectCreatureClip(IsWomanOrChild(villager), std::nullopt, random));
	case animation::ClipChoice::RespectCreature:
		return Clip(animation::RespectCreatureClip(random));
	case animation::ClipChoice::ControlledByCreature:
		return Clip(animation::ControlledByCreatureClip(random));
	case animation::ClipChoice::AmazedByShield:
		return Clip(animation::AmazedByShieldClip(random));
	case animation::ClipChoice::TownEmergency:
		return Clip(animation::TownEmergencyClip(IsWoman(villager), random));
	case animation::ClipChoice::RandomCrowd:
		return Clip(animation::RandomCrowdClip(random));
	case animation::ClipChoice::SitDown:
		return Clip(animation::SitDownClip(villager.transitionPlaying, CurrentClip(entity), random));
	case animation::ClipChoice::Building:
	{
		const auto choice = animation::BuildingClip(villager.transitionPlaying, CurrentClip(entity), random);
		villager.carried = choice.tool;
		return Clip(choice.clip);
	}
	case animation::ClipChoice::FootballWaitForKickOff:
		return Clip(animation::FootballWaitForKickOffClip(random));
	case animation::ClipChoice::FootballMatchPaused:
		return Clip(animation::FootballMatchPausedClip(random));
	case animation::ClipChoice::FootballGoalKeeper:
		return Clip(AnimId::PGoalkeeper);
	case animation::ClipChoice::FootballOutfield:
		return Clip(AnimId::PStand);
	case animation::ClipChoice::FootballWatchMatch:
		// TODO(villagers): the state of the town's match, once villagers play football
		return Clip(animation::FootballWatchMatchClip(std::nullopt, random));
	case animation::ClipChoice::WatchFight:
		// TODO(villagers): watching fights; with no fight the game stands them
		return Clip(AnimId::PStand);
	case animation::ClipChoice::Dance:
		// Its group's part of the dance; standing in none
		return villager_dance::DanceClip(entity, Clip(WalkClipOf(entity, villager)));
	case animation::ClipChoice::Script:
		// The clip a script asked it to play; none keeps the one it has
		return villager_script::ScriptClip(entity);
	case animation::ClipChoice::Table:
	default:
		return StateInfo(state).animation;
	}
}

AnimId villager_animate::CurrentClip(entt::entity villager)
{
	const auto* pose = Entities().TryGet<const VillagerPose>(villager);
	return pose != nullptr ? pose->clip : AnimId::Invalid;
}

void villager_animate::SetAnim(entt::entity villager, int32_t clip, bool restart)
{
	auto* pose = Entities().TryGet<VillagerPose>(villager);
	if (clip < 0 || pose == nullptr)
	{
		return;
	}
	const bool sameClip = static_cast<AnimId>(clip) == pose->clip;
	pose->clip = static_cast<AnimId>(clip);
	const bool dancing = dances::DanceOf(Entities(), villager) != entt::null;
	if (villager_clips::PlaceOnSetClip(sameClip, restart, dancing) == villager_clips::ClipPlace::Restart)
	{
		pose->place = 0;
	}
}

void villager_animate::SetStateAnim(entt::entity villager)
{
	auto* pose = Entities().TryGet<VillagerPose>(villager);
	if (pose == nullptr)
	{
		return;
	}
	const auto clip = StateClip(villager);
	if (clip < 0)
	{
		return;
	}
	// The same clip again plays on
	if (static_cast<AnimId>(clip) != pose->clip)
	{
		pose->clip = static_cast<AnimId>(clip);
		pose->place = 0;
	}
}

std::optional<AnimId> villager_animate::OutOfClip(LivingAction& action, VillagerStates next)
{
	auto& registry = Entities();
	const auto entity = registry.ToEntity(action);
	if (StateInfo(next).skipsOutOfClip != 0)
	{
		return std::nullopt;
	}
	const auto top = TopOf(action);
	const auto transition = animation::TransitionOf(top);
	if (transition == animation::Transition::None)
	{
		return std::nullopt;
	}
	const auto clip =
	    animation::TransitionClip(transition, false, next == top, animation::IsDeathState(next), CurrentClip(entity));
	if (clip.has_value())
	{
		auto& villager = registry.Get<Villager>(entity);
		villager.transitionPlaying = true;
		villager.intoClipDue = true;
	}
	return clip;
}

std::optional<AnimId> villager_animate::IntoClip(LivingAction& action, VillagerStates state)
{
	auto& registry = Entities();
	const auto entity = registry.ToEntity(action);
	const auto transition = animation::TransitionOf(TopOf(action));
	if (transition == animation::Transition::None)
	{
		return std::nullopt;
	}
	const auto clip = animation::TransitionClip(transition, true, state == TopOf(action), animation::IsDeathState(state),
	                                            CurrentClip(entity));
	if (clip.has_value())
	{
		auto& villager = registry.Get<Villager>(entity);
		villager.transitionPlaying = true;
		villager.intoClipDue = false;
	}
	return clip;
}

bool villager_animate::IsReadyForNewAnimation(const LivingAction& action, uint32_t times)
{
	return villager_clips::ClipPlayed(action, CurrentClip(Entities().ToEntity(action)), times);
}

void villager_animate::FinishedIntoOutOfAnimation(LivingAction& action)
{
	auto& registry = Entities();
	const auto entity = registry.ToEntity(action);
	auto& villager = registry.Get<Villager>(entity);
	if (villager.intoClipDue && animation::TransitionOf(TopOf(action)) != animation::Transition::None)
	{
		if (const auto into = IntoClip(action, TopOf(action)); into.has_value())
		{
			// As in the game, the clip into the new state isn't played after one out of the last: the state's own clip
			// starts over and is waited for instead
			SetAnim(entity, StateClip(entity), true);
			villager.intoClipDue = false;
			action.turnsSinceStateChange = 0;
			return;
		}
	}
	SetStateAnim(entity);
	villager.transitionPlaying = false;
	action.turnsSinceStateChange = 0;
}

bool villager_animate::IsHiddenByState(VillagerStates state)
{
	return StateInfo(state).animation == animation::k_HiddenClip;
}
