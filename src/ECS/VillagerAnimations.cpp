/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "VillagerAnimations.h"

#include <algorithm>
#include <vector>

#include <glm/geometric.hpp>

#include "3D/L3DAnim.h"
#include "Common/GameRandom.h"
#include "ECS/Animations.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/SkeletalAnimation.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Unavailable.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WallHug.h"
#include "ECS/MobileDrawing.h"
#include "ECS/Registry.h"
#include "ECS/SeaCells.h"
#include "ECS/Systems/LivingActionSystemInterface.h"
#include "ECS/Villager/VillagerDeath.h"
#include "ECS/Villager/VillagerResources.h"
#include "ECS/VillagerAnimationTable.h"
#include "ECS/VillagerSpeed.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"

namespace openblack::ecs
{
using namespace components;
using namespace villager_animation;

namespace
{
// ANM_ indices of Data\AllMeshes.h (= AllAnims.anm)
constexpr int32_t k_DontDraw = -4;
constexpr int32_t k_AttractYourAttention = 207;
constexpr int32_t k_Beckon = 210;
constexpr int32_t k_CarryAxe = 215;
constexpr int32_t k_CarryObjectRun = 216;
constexpr int32_t k_ChoppingTree = 217;
constexpr int32_t k_ConductMeeting = 222;
constexpr int32_t k_CoupleKissMan = 223;
constexpr int32_t k_CoupleKissWoman = 224;
constexpr int32_t k_CrawlInjured = 225;
constexpr int32_t k_CrawlInjuredInto = 226;
constexpr int32_t k_CrowdImpressed1 = 227;
constexpr int32_t k_CrowdWon = 234;
constexpr int32_t k_Dead1 = 243;
constexpr int32_t k_Dead2 = 246;
constexpr int32_t k_DeadDrowned = 249;
constexpr int32_t k_Dying = 253;
constexpr int32_t k_Hammering = 276;
constexpr int32_t k_IntoDeadDrowned = 283;
constexpr int32_t k_IntoHammering = 284;
constexpr int32_t k_IntoMourning = 285;
constexpr int32_t k_IntoPointing = 286;
constexpr int32_t k_IntoPray = 287;
constexpr int32_t k_IntoSawWood = 292;
constexpr int32_t k_IntoSledgehammer = 293;
constexpr int32_t k_IntoSleep = 294;
constexpr int32_t k_Landed = 306;
constexpr int32_t k_LandedFromBack = 307;
constexpr int32_t k_LandedFromFeet = 308;
constexpr int32_t k_LandedFromFeetCarryObject = 309;
constexpr int32_t k_LookingForSomething = 310;
constexpr int32_t k_LookAtHand = 311;
constexpr int32_t k_OutOfHammering = 320;
constexpr int32_t k_OutOfMourning = 321;
constexpr int32_t k_OutOfPray = 323;
constexpr int32_t k_OutOfSawWood = 328;
constexpr int32_t k_OutOfSledgehammer = 329;
constexpr int32_t k_OutOfSleep = 330;
constexpr int32_t k_Overworked1 = 332;
constexpr int32_t k_Overworked2 = 333;
constexpr int32_t k_PanicMan = 334;
constexpr int32_t k_PanicWoman = 335;
constexpr int32_t k_PickUpSticks = 340;
constexpr int32_t k_Pray = 343;
constexpr int32_t k_RunMan = 351;
constexpr int32_t k_RunWoman = 353;
constexpr int32_t k_SawWood = 354;
constexpr int32_t k_ScaredStiff = 355;
constexpr int32_t k_ScaredStiff2 = 356;
constexpr int32_t k_SittingDown1Into = 367;
constexpr int32_t k_SittingDown1Out = 368;
constexpr int32_t k_SittingDown1 = 369;
constexpr int32_t k_SittingDown2Into = 370;
constexpr int32_t k_SittingDown2Out = 371;
constexpr int32_t k_SittingDown2 = 372;
constexpr int32_t k_Sledgehammer = 380;
constexpr int32_t k_SprintRunMan = 383;
constexpr int32_t k_SprintRunWoman = 384;
constexpr int32_t k_Stand = 385;
constexpr int32_t k_StandDespair1 = 386;
constexpr int32_t k_TalkingAndPointing = 395;
constexpr int32_t k_Thrown = 399;
constexpr int32_t k_ThrownDead = 400;
constexpr int32_t k_WaitingImpatiently = 418;
constexpr int32_t k_WalkInjured = 426;
constexpr int32_t k_WalkMan = 427;
constexpr int32_t k_WalkWoman = 431;
constexpr int32_t k_Yawn = 437;
constexpr int32_t k_Yawn2 = 438;

// The carried object ids written into SkeletalAnimation::carriedObject
constexpr int32_t k_CarriedNoChange = 0;
constexpr int32_t k_CarriedNone = 1;
constexpr int32_t k_CarriedAxe = 2;
constexpr int32_t k_CarriedSaw = 5;
constexpr int32_t k_CarriedBall = 7;
constexpr int32_t k_CarriedHammer = 8;
constexpr int32_t k_CarriedMalletHeavy = 9;

const GVillagerStateTableInfo& StateInfo(VillagerStates state)
{
	return Locator::infoConstants::value().villagerStateTable.at(static_cast<size_t>(state));
}

VillagerStates TopState(entt::entity villager)
{
	const auto* action = Locator::entitiesRegistry::value().TryGet<const LivingAction>(villager);
	return action != nullptr ? static_cast<VillagerStates>(action->states[static_cast<size_t>(LivingAction::Index::Top)])
	                         : VillagerStates::InvalidState;
}

int32_t CurrentClip(entt::entity villager)
{
	const auto* animation = Locator::entitiesRegistry::value().TryGet<const SkeletalAnimation>(villager);
	return animation != nullptr && animation->hasClip ? animation->clipIndex : -1;
}

bool IsMale(const Villager& villager)
{
	return villager.sex == Villager::Sex::MALE;
}

bool IsWomanOrChild(const Villager& villager)
{
	return !IsMale(villager) || villager.lifeStage == Villager::LifeStage::Child;
}

/// Whether the villager's position is in the water
bool IsInWater(entt::entity villager)
{
	const auto* transform = Locator::entitiesRegistry::value().TryGet<const Transform>(villager);
	return transform != nullptr && sea_cells::IsWater(transform->position);
}

/// The landType of the last landing (set at the end of its physics; dying ors in 3)
uint8_t LandType(const Villager& villager)
{
	return static_cast<uint8_t>((villager.status & Villager::k_StatusLandTypeMask) >> Villager::k_LandTypeShift);
}

float Life(const Villager& villager)
{
	return villager.life;
}

/// The carried object: the load (wood log / bag), the life, the builders' final states and what
/// the state rows force (ecs::villager::SetStateCarriedObject), from the current one (kept by the script states)
int32_t CarriedObject(entt::entity villager)
{
	const auto* animation = Locator::entitiesRegistry::value().TryGet<const SkeletalAnimation>(villager);
	const int32_t previous = animation != nullptr ? animation->carriedObject : k_CarriedNone;
	return villager::SetStateCarriedObject(villager, previous);
}

/// The move clip: walk, run or sprint by the speed; carry clips; wounded
int32_t MoveToPosAnimation(entt::entity entity, const Villager& villager)
{
	const auto& info = Locator::infoConstants::value();
	const auto& villagerInfo = info.villager.at(0);
	if (Life(villager) <= villagerInfo.lifeWhenCrawlsWounded)
	{
		return k_CrawlInjured;
	}
	if (Life(villager) <= villagerInfo.lifeWhenWalksWounded)
	{
		return k_WalkInjured;
	}
	const auto carried = CarriedObject(entity);
	const bool carrying = carried != k_CarriedNone && carried != k_CarriedSaw && carried != k_CarriedBall &&
	                      carried != k_CarriedHammer && carried > k_CarriedNoChange;
	// SPEED_THRESHOLD_VILLAGER_MAN / _NORMAL
	const auto& threshold = info.speedThreshold.at(IsMale(villager) ? 1 : 0);
	const auto* wallHug = Locator::entitiesRegistry::value().TryGet<const WallHug>(entity);
	// WallHug::speed is per turn (0.1 s)
	const float speed = wallHug != nullptr ? wallHug->speed * 10.0f : 0.0f;
	if (speed <= GetSpeedStateSpeed(threshold.speedMaxWalk))
	{
		return carrying ? k_CarryAxe : IsMale(villager) ? k_WalkMan : k_WalkWoman;
	}
	if (speed <= GetSpeedStateSpeed(threshold.speedMaxRun))
	{
		return carrying ? k_CarryObjectRun : IsMale(villager) ? k_RunMan : k_RunWoman;
	}
	return carrying ? k_CarryObjectRun : IsMale(villager) ? k_SprintRunMan : k_SprintRunWoman;
}

/// The state's animation function. Functions that need what openblack doesn't have yet (landing type,
/// water, vortices, dance groups, fights, football, creatures) take the branch of the original for its absence.
int32_t StateFunctionAnim(AnimFn function, entt::entity entity, const Villager& villager, int32_t fallback)
{
	switch (function)
	{
	case AnimFn::None:
		return fallback;
	case AnimFn::MoveToPos:
		return MoveToPosAnimation(entity, villager);
	case AnimFn::FootballAttacker: // STAND; the move clip comes from the distance sync
	case AnimFn::FootballDefender:
		return k_Stand;
	case AnimFn::Landed: // by the landType of the last landing
		return VillagerLandedClip(LandType(villager), CarriedObject(entity) == k_CarriedNone);
	case AnimFn::Dying: // in the water P_INTO_DEAD_DROWNED; landType 2 P_DEAD2
		return villager::DyingClip(IsInWater(entity), LandType(villager));
	case AnimFn::Dead: // in the water P_DEAD_DROWNED; landType 2 P_DEAD2; else P_DEAD1
		return villager::DeadClip(IsInWater(entity), LandType(villager));
	case AnimFn::Thrown: // not in a vortex
		return Life(villager) <= 0.0f ? k_ThrownDead : k_Thrown;
	case AnimFn::Kissing:
		return IsMale(villager) ? k_CoupleKissMan : k_CoupleKissWoman;
	case AnimFn::Forestering:
		return k_ChoppingTree;
	case AnimFn::Building:
	{
		// the working clip after its into clip, else one of the three at random
		// and it sets the carried object: hammer, saw or heavy mallet (GameRand, as every draw here)
		const auto current = CurrentClip(entity);
		int32_t clip = k_Hammering;
		if (current == k_IntoHammering)
		{
			clip = k_Hammering;
		}
		else if (current == k_IntoSawWood)
		{
			clip = k_SawWood;
		}
		else if (current == k_IntoSledgehammer)
		{
			clip = k_Sledgehammer;
		}
		else
		{
			const std::array<int32_t, 3> clips = {k_Hammering, k_SawWood, k_Sledgehammer};
			clip = clips.at(static_cast<size_t>(game_random::GameRand(3)));
		}
		if (auto* animation = Locator::entitiesRegistry::value().TryGet<SkeletalAnimation>(entity);
		    animation != nullptr && !animation->carriedLocked)
		{
			animation->carriedObject = clip == k_Hammering ? k_CarriedHammer
			                           : clip == k_SawWood ? k_CarriedSaw
			                                               : k_CarriedMalletHeavy;
		}
		return clip;
	}
	case AnimFn::Script: // the clip SET_SCRIPT_ULONG gave
		return static_cast<int32_t>(villager.scriptAnim);
	case AnimFn::Dance:      // no dance group
	case AnimFn::WatchFight: // no arena
	case AnimFn::LookAtFlyingObject:
		return k_Stand;
	case AnimFn::LookAtLargeObject:
		return k_LookingForSomething;
	case AnimFn::InspectCreature:
		if (IsWomanOrChild(villager) && static_cast<int32_t>(game_random::GameRand(3)) == 0)
		{
			return k_ScaredStiff;
		}
		return static_cast<int32_t>(game_random::GameRand(8)) > 2 ? k_TalkingAndPointing : k_Stand;
	case AnimFn::RespectCreature:
	{
		const auto r = static_cast<int32_t>(game_random::GameRand(5));
		return r == 0 ? k_CrowdImpressed1 : r <= 2 ? k_Stand : k_Pray;
	}
	case AnimFn::ControlledByCreature:
		return static_cast<int32_t>(game_random::GameRand(3)) == 2 ? k_WaitingImpatiently : k_Stand;
	case AnimFn::PointAtFlyingObject:
	{
		if (IsWomanOrChild(villager) && static_cast<int32_t>(game_random::GameRand(3)) == 0)
		{
			return k_ScaredStiff;
		}
		const std::array<int32_t, 3> clips = {k_LookingForSomething, k_Stand, k_TalkingAndPointing};
		return clips.at(static_cast<size_t>(game_random::GameRand(3)));
	}
	case AnimFn::FootballWaitForKickOff:
	{
		const auto r = static_cast<int32_t>(game_random::GameRand(100));
		return r < 25 ? 414 : r < 50 ? 415 : r < 75 ? 416 : 417;
	}
	case AnimFn::FootballGoalKeeper:
		return 269;
	case AnimFn::FootballWatchMatch:
	{
		const std::array<int32_t, 3> clips = {k_TalkingAndPointing, 396, 272};
		return clips.at(static_cast<size_t>(game_random::GameRand(3)));
	}
	case AnimFn::FootballMatchPaused:
	{
		const auto r = static_cast<int32_t>(game_random::GameRand(100));
		return r < 25 ? k_LookingForSomething : r < 50 ? k_Stand : r < 75 ? 412 : 413;
	}
	case AnimFn::Yawn:
		return static_cast<int32_t>(game_random::GameRand(2)) == 0 ? k_Yawn : k_Yawn2;
	case AnimFn::PauseForASecond: // not poisoned
		return static_cast<int32_t>(game_random::GameRand(2)) == 0 ? k_Overworked1 : k_Overworked2;
	case AnimFn::AmazedByShield:
	{
		const auto r = static_cast<int32_t>(game_random::GameRand(5));
		return r == 0 ? k_IntoPointing : r <= 2 ? k_LookAtHand : k_Stand;
	}
	case AnimFn::TownEmergency:
	{
		const std::array<int32_t, 10> clips = {k_AttractYourAttention, k_Beckon,
		                                       k_ConductMeeting,       IsMale(villager) ? k_PanicMan : k_PanicWoman,
		                                       k_ScaredStiff,          k_ScaredStiff2,
		                                       k_StandDespair1,        k_StandDespair1 + 1,
		                                       k_StandDespair1 + 2,    k_TalkingAndPointing};
		return clips.at(static_cast<size_t>(game_random::GameRand(10)));
	}
	case AnimFn::RandomCrowd:
	{
		const auto r = static_cast<int32_t>(game_random::GameRand(25));
		return r == 0   ? 204
		       : r == 1 ? 203
		       : r == 2 ? k_WaitingImpatiently
		       : r == 3 ? k_CrowdImpressed1
		       : r <= 5 ? k_CrowdWon
		                : k_Stand;
	}
	case AnimFn::SitDown:
	{
		// only while an into / out-of clip plays (flag 0x800) the current clip decides (367 -> 369, 370 -> 372);
		// else GameRand(2): 0 -> 369, 1 -> 372
		const auto* animation = Locator::entitiesRegistry::value().TryGet<const SkeletalAnimation>(entity);
		if (animation != nullptr && (animation->transitionFlags & 0x800) != 0)
		{
			const auto current = CurrentClip(entity);
			if (current == k_SittingDown1Into)
			{
				return k_SittingDown1;
			}
			if (current == k_SittingDown2Into)
			{
				return k_SittingDown2;
			}
		}
		return static_cast<int32_t>(game_random::GameRand(2)) == 0 ? k_SittingDown1 : k_SittingDown2;
	}
	}
	return fallback;
}

/// The into / out-of functions: into = entering the state; -1 = none
int32_t TransitionAnim(TransitionFn function, entt::entity entity, bool into, VillagerStates other, VillagerStates self)
{
	const auto current = CurrentClip(entity);
	switch (function)
	{
	case TransitionFn::None:
		return -1;
	case TransitionFn::SleepInTent:
		return into ? k_IntoSleep : k_OutOfSleep;
	case TransitionFn::Mourn:
		return into ? k_IntoMourning : k_OutOfMourning;
	case TransitionFn::Pray:
		return into ? k_IntoPray : k_OutOfPray;
	case TransitionFn::SitDown:
		if (into)
		{
			return current == k_SittingDown1 ? k_SittingDown1Into : k_SittingDown2Into;
		}
		return current == k_SittingDown1 ? k_SittingDown1Out : k_SittingDown2Out;
	case TransitionFn::Building:
		if (current == k_Hammering)
		{
			return into ? k_IntoHammering : k_OutOfHammering;
		}
		if (current == k_SawWood)
		{
			return into ? k_IntoSawWood : k_OutOfSawWood;
		}
		if (current == k_Sledgehammer)
		{
			return into ? k_IntoSledgehammer : k_OutOfSledgehammer;
		}
		return -1;
	case TransitionFn::ArrivesAtResource:
		return !into && other != self ? k_PickUpSticks : -1;
	case TransitionFn::MoveToPos:
	{
		// only for a crawling villager that is not going to die
		const bool death = other == VillagerStates::SetDying || other == VillagerStates::Dying ||
		                   other == VillagerStates::Dead || other == VillagerStates::Drowning;
		if (current != k_CrawlInjured || death)
		{
			return -1;
		}
		return into ? k_CrawlInjuredInto : k_OutOfSleep;
	}
	}
	return -1;
}

SkeletalAnimation& AnimationOf(entt::entity entity)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (auto* animation = registry.TryGet<SkeletalAnimation>(entity); animation != nullptr)
	{
		return *animation;
	}
	return registry.Assign<SkeletalAnimation>(entity);
}

/// Switches at once (no blending) and does nothing if that clip already plays
void SetAnim(entt::entity entity, int32_t clip, bool reset)
{
	if (clip < 0)
	{
		return;
	}
	auto& animation = AnimationOf(entity);
	if (animation.locked || (animation.hasClip && animation.clipIndex == clip))
	{
		return;
	}
	animation.clip = ClipId(static_cast<uint32_t>(clip));
	animation.clipIndex = clip;
	animation.hasClip = true;
	if (reset)
	{
		animation.time = 0.0f;
	}
}

/// The state's clip from the start; negative ids (not drawn) keep the old clip
void SetStateAnim(entt::entity entity)
{
	const auto clip = VillagerAnimId(entity);
	if (clip >= 0 && clip != CurrentClip(entity))
	{
		SetAnim(entity, clip, true);
	}
}

} // namespace

int32_t VillagerAnimId(entt::entity entity)
{
	// the carried object first
	if (auto* animation = Locator::entitiesRegistry::value().TryGet<SkeletalAnimation>(entity);
	    animation != nullptr && !animation->carriedLocked)
	{
		animation->carriedObject = CarriedObject(entity);
	}
	const auto state = TopState(entity);
	if (state == VillagerStates::InvalidState || static_cast<size_t>(state) >= 255)
	{
		return k_Stand;
	}
	const auto* villager = Locator::entitiesRegistry::value().TryGet<const Villager>(entity);
	const auto fallback = static_cast<int32_t>(StateInfo(state).animation);
	if (villager == nullptr)
	{
		return fallback;
	}
	return StateFunctionAnim(k_StateAnimFns.at(static_cast<size_t>(state)).anim, entity, *villager, fallback);
}

namespace
{
/// The out-of clip for a given current state
int32_t OutOfClip(entt::entity entity, VillagerStates current, VillagerStates next)
{
	if (StateInfo(next).field0xf0 != 0 || static_cast<size_t>(current) >= 255)
	{
		return -1;
	}
	const auto out = TransitionAnim(k_StateAnimFns.at(static_cast<size_t>(current)).transition, entity, false, next, current);
	if (out != -1)
	{
		// the flags 0x800 / 0x1000
		AnimationOf(entity).transitionFlags |= 0x1800;
	}
	return out;
}
} // namespace

int32_t VillagerCallOutOfAnimation(entt::entity entity, VillagerStates next)
{
	return OutOfClip(entity, TopState(entity), next);
}

void VillagerApplyStateClips(entt::entity entity, VillagerStates entered, int32_t out)
{
	const auto top = TopState(entity);
	// the state's speed with no test (its own skips, script-controlled or dancing, are inside SetVillagerStateSpeed)
	SetVillagerStateSpeed(entity);
	if (out != -1)
	{
		SetAnim(entity, out, true);
		return;
	}
	SetStateAnim(entity);
	if (static_cast<size_t>(top) >= 255)
	{
		return;
	}
	// the into clip: the TOP's function with (1, entered): the new top state, or the destination when both are set
	const auto into = TransitionAnim(k_StateAnimFns.at(static_cast<size_t>(top)).transition, entity, true, entered, top);
	if (into != -1)
	{
		// flags = (flags | 0x800) & ~0x1000
		auto& flags = AnimationOf(entity).transitionFlags;
		flags = static_cast<uint16_t>((flags | 0x800) & ~0x1000);
		SetAnim(entity, into, true);
	}
}

void OnVillagerStateChanged(entt::entity entity, VillagerStates previous, VillagerStates next)
{
	const auto out = OutOfClip(entity, previous, next);
	VillagerApplyStateClips(entity, next, out);
}

bool VillagerWaitsForTransition(entt::entity entity, uint16_t turnsSinceStateChange)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* animation = registry.TryGet<SkeletalAnimation>(entity);
	if (animation == nullptr || (animation->transitionFlags & 0x800) == 0)
	{
		return false;
	}
	// ready for a new clip: turns in the state * 100 ms >= the clip's duration (no resources, in the
	// unit tests: no duration)
	int32_t duration = 0;
	if (Locator::resources::has_value())
	{
		auto& animations = Locator::resources::value().GetAnimations();
		duration = animation->hasClip && animations.Contains(animation->clip)
		               ? animations.Handle(animation->clip)->GetDurationMs()
		               : 0;
	}
	if (static_cast<int32_t>(turnsSinceStateChange) * 100 < duration)
	{
		return true;
	}
	// the into / out-of clip is over, still skipping the state logic this turn
	auto* action = registry.TryGet<LivingAction>(entity);
	const auto state = TopState(entity);
	bool replayed = false;
	if ((animation->transitionFlags & 0x1000) != 0 && static_cast<size_t>(state) < 255 &&
	    k_StateAnimFns.at(static_cast<size_t>(state)).transition != TransitionFn::None)
	{
		// an out-of clip ended and the state has an into clip: the original calls SetAnim(VillagerAnimId(), into), so the
		// state's own clip restarts in "transition" mode for one more cycle and the into clip is never shown
		const auto into = TransitionAnim(k_StateAnimFns.at(static_cast<size_t>(state)).transition, entity, true, state, state);
		if (into != -1)
		{
			const auto clip = VillagerAnimId(entity);
			SetAnim(entity, clip, true);
			AnimationOf(entity).time = 0.0f;
			AnimationOf(entity).transitionFlags &= static_cast<uint16_t>(~0x1000);
			replayed = true;
		}
	}
	if (!replayed)
	{
		// no out-of clip, no into function or no into clip: SetStateAnim and the wait ends; only 0x800 is
		// cleared, 0x1000 stays
		SetStateAnim(entity);
		AnimationOf(entity).transitionFlags &= static_cast<uint16_t>(~0x800);
	}
	if (action != nullptr)
	{
		action->turnsSinceStateChange = 0;
	}
	return true;
}

void VillagerSetStateClip(entt::entity villager, bool reset)
{
	// VillagerAnimId; a negative id or the clip it has -> nothing; else the clip and, with n and not dancing, its
	// time from 0
	if (!Locator::entitiesRegistry::value().AllOf<SkeletalAnimation>(villager))
	{
		return;
	}
	SetAnim(villager, VillagerAnimId(villager), reset);
}

void VillagerSetClip(entt::entity villager, int32_t clip, bool reset)
{
	// SetAnim(clip, n) with a clip of the caller's, not VillagerAnimId's (the states that name one: the villager
	// amazed by a magic shield with 395)
	if (!Locator::entitiesRegistry::value().AllOf<SkeletalAnimation>(villager))
	{
		return;
	}
	SetAnim(villager, clip, reset);
}

int32_t VillagerLandedClip(uint8_t landType, bool carriesNothing)
{
	if ((landType & 3) == 0)
	{
		return carriesNothing ? k_LandedFromFeet : k_LandedFromFeetCarryObject;
	}
	return (landType & 3) == 1 ? k_Landed : k_LandedFromBack;
}

bool VillagerAnimationDone(entt::entity entity, uint16_t turnsSinceStateChange)
{
	const auto* animation = Locator::entitiesRegistry::value().TryGet<const SkeletalAnimation>(entity);
	if (animation == nullptr || !animation->hasClip || !Locator::resources::has_value())
	{
		return true;
	}
	auto& animations = Locator::resources::value().GetAnimations();
	if (!animations.Contains(animation->clip))
	{
		return true;
	}
	return static_cast<int32_t>(turnsSinceStateChange) * 100 >= animations.Handle(animation->clip)->GetDurationMs();
}

void SetVillagerState(entt::entity entity, VillagerStates state)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* action = registry.TryGet<LivingAction>(entity);
	if (action == nullptr || !registry.AllOf<Villager>(entity))
	{
		return;
	}
	registry.Remove<MoveStateLinearTag, MoveStateOrbitTag, MoveStateExitCircleTag, MoveStateStepThroughTag,
	                MoveStateFinalStepTag, MoveStateArrivedTag>(entity);
	// the end of physics / the hand: drawn at its position, no slide from where it was
	NotifyTeleported(entity);
	Locator::livingActionSystem::value().VillagerSetState(*action, LivingAction::Index::Top, state, true);
}

void UpdateVillagerAnimations()
{
	auto& registry = Locator::entitiesRegistry::value();
	// villagers without a clip yet: their state's clip
	std::vector<entt::entity> fresh;
	registry.Each<const Villager>([&registry, &fresh](entt::entity entity, const Villager&) {
		if (!registry.AnyOf<SkeletalAnimation, Unavailable>(entity))
		{
			fresh.push_back(entity);
		}
	});
	for (const auto entity : fresh)
	{
		registry.Assign<SkeletalAnimation>(entity);
		SetStateAnim(entity);
	}
	// not drawn in the states whose clip is -4 (the current state). A zombie (Unavailable) is left as it is in this
	// loop and the next: its deletion takes it off the living list
	std::vector<entt::entity> hide;
	std::vector<entt::entity> show;
	registry.Each<const Villager, const SkeletalAnimation>(
	    [&](entt::entity entity, const Villager&, const SkeletalAnimation& animation) {
		    const auto state = TopState(entity);
		    const bool hidden =
		        static_cast<size_t>(state) < 255 && static_cast<int32_t>(StateInfo(state).animation) == k_DontDraw;
		    if (hidden && animation.hiddenMesh == 0 && registry.AllOf<Mesh>(entity))
		    {
			    hide.push_back(entity);
		    }
		    else if (!hidden && animation.hiddenMesh != 0)
		    {
			    show.push_back(entity);
		    }
	    },
	    entt::exclude<Unavailable>);
	for (const auto entity : hide)
	{
		registry.Get<SkeletalAnimation>(entity).hiddenMesh = registry.Get<Mesh>(entity).id;
		registry.Remove<Mesh>(entity);
	}
	for (const auto entity : show)
	{
		auto& animation = registry.Get<SkeletalAnimation>(entity);
		registry.Assign<Mesh>(entity, animation.hiddenMesh, static_cast<int8_t>(0), static_cast<int8_t>(0));
		animation.hiddenMesh = 0;
	}
	// moving states (info field0x14) advance the clip with the ground covered
	registry.Each<const Villager, SkeletalAnimation>(
	    [&registry](entt::entity entity, const Villager&, SkeletalAnimation& animation) {
		    const auto state = TopState(entity);
		    const bool movingState = static_cast<size_t>(state) < 255 && StateInfo(state).field0x14 != 0;
		    const bool moving =
		        registry.AnyOf<MoveStateLinearTag, MoveStateOrbitTag, MoveStateExitCircleTag, MoveStateStepThroughTag>(entity);
		    const auto* wallHug = registry.TryGet<const WallHug>(entity);
		    // the ground really covered: one step per game turn of 100 ms
		    const float stepSpeed = wallHug != nullptr ? glm::length(wallHug->step) * 10.0f : 0.0f;
		    animation.distanceSpeed = movingState && wallHug != nullptr ? (moving ? std::max(stepSpeed, 1e-6f) : 1e-6f) : 0.0f;
	    },
	    entt::exclude<Unavailable>);
}

} // namespace openblack::ecs
