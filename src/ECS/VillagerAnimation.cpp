/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "VillagerAnimation.h"

using namespace openblack;
using namespace openblack::ecs;
using villager_animation::ClipChoice;
using villager_animation::Transition;

namespace
{
/// Near enough to the creature to look at it, in metres
constexpr float k_LookAtCreatureDistance = 8.0f;
/// High enough in the air to be looked up at, in metres
constexpr float k_LookUpHeight = 3.0f;
/// The match states the crowd reacts to: a goal, and play going on
constexpr uint32_t k_MatchGoal = 3;
constexpr uint32_t k_MatchPlaying = 2;
} // namespace

ClipChoice villager_animation::ClipChoiceOf(VillagerStates state)
{
	using S = VillagerStates;
	switch (state)
	{
	case S::MoveToPos:
	case S::MoveToObject:
	case S::MoveOnStructure:
	case S::FleeingFromObjectReaction:
	case S::MoveAlongPath:
	case S::MoveOnPath:
	case S::MoveToWorshipSiteWithSupplies:
	case S::ForesterMoveToForest:
	case S::FootballWalkToPosition:
	case S::StartMoveToPickUpBallForDeadBall:
	case S::ShepherdMoveFlockToWater:
	case S::ShepherdMoveFlockToFood:
	case S::ShepherdMoveFlockBack:
	case S::ApproachObjectReaction:
	case S::ApproachVillagerToTalkTo:
	case S::MoveTowardsObjectToLookAt:
	case S::MoveToDancePos:
	case S::MoveTowardsCreatureReaction:
	case S::FootballMoveToBall:
	case S::FleeingFromPredatorReaction:
		return ClipChoice::Walk;
	case S::Flying:
		return ClipChoice::Thrown;
	case S::Landed:
		return ClipChoice::Landed;
	case S::Dying:
		return ClipChoice::Dying;
	case S::Dead:
		return ClipChoice::Dead;
	case S::HavingSex:
		return ClipChoice::Kissing;
	case S::ForesterChopsTreeForBuilding:
	case S::ArrivesAtRockForWood:
	case S::GotWoodFromRock:
	case S::TakeWoodFromTree:
	case S::TakeWoodFromPot:
	case S::TakeWoodFromTreeForBuilding:
	case S::TakeWoodFromPotForBuilding:
		return ClipChoice::Forestering;
	case S::AfterTapOnAbode:
		return ClipChoice::Yawn;
	case S::PauseForASecond:
		return ClipChoice::PauseForASecond;
	case S::LookAtMagicTree:
		return ClipChoice::LookAtLargeObject;
	case S::LookAtFlyingObjectReaction:
	case S::WatchFlyingObjectReaction:
		return ClipChoice::LookAtFlyingObject;
	case S::PointAtFlyingObjectReaction:
		return ClipChoice::PointAtFlyingObject;
	case S::InspectCreatureReaction:
	case S::PerformInspectCreatureReaction:
	case S::ApproachCreatureReaction:
	case S::TurnToFaceCreatureReaction:
		return ClipChoice::InspectCreature;
	case S::InitialiseRespectCreatureReaction:
	case S::PerformRespectCreatureReaction:
	case S::FinishRespectCreatureReaction:
		return ClipChoice::RespectCreature;
	case S::ControlledByCreature:
		return ClipChoice::ControlledByCreature;
	case S::AmazedByMagicShieldReaction:
		return ClipChoice::AmazedByShield;
	case S::CongregateInTownAfterEmergency:
		return ClipChoice::TownEmergency;
	case S::ScriptInCrowd:
		return ClipChoice::RandomCrowd;
	case S::SitAndChillout:
		return ClipChoice::SitDown;
	case S::Building:
		return ClipChoice::Building;
	case S::PerformFightReaction:
		return ClipChoice::WatchFight;
	case S::FootballWaitForKickOff:
		return ClipChoice::FootballWaitForKickOff;
	case S::FootballMatchPaused:
		return ClipChoice::FootballMatchPaused;
	case S::FootballGoalie:
		return ClipChoice::FootballGoalKeeper;
	case S::FootballAttacker:
	case S::FootballDefender:
		return ClipChoice::FootballOutfield;
	case S::FootballWatchMatch:
		return ClipChoice::FootballWatchMatch;
	case S::InDance:
	case S::WorshippingAtWorshipSite:
	case S::RestartWorshippingAtWorshipSite:
	case S::RestartWorshippingCreature:
	case S::WorshippingCreature:
	case S::DanceForEditingPurposes:
	case S::DanceButNotWorship:
	case S::DanceWhileReacting:
	case S::ArtifactDance:
	case S::WaitForArtifactDance:
		return ClipChoice::Dance;
	case S::ScriptPlayAnim:
		return ClipChoice::Script;
	default:
		return ClipChoice::Table;
	}
}

Transition villager_animation::TransitionOf(VillagerStates state)
{
	using S = VillagerStates;
	switch (state)
	{
	case S::SleepInTent:
	case S::SleepInTentFromWorship:
		return Transition::SleepOnTheGround;
	case S::MornDeath:
	case S::MournDeadPerson:
		return Transition::Mourn;
	case S::AtAltarRest:
	case S::PerformImpressedReaction:
	case S::DiscipleNothingToDo:
		return Transition::Pray;
	case S::ArrivesAtFoodReaction:
	case S::ArrivesAtWoodReaction:
		return Transition::ArrivesAtResource;
	case S::SitAndChillout:
		return Transition::SitDown;
	case S::Building:
		return Transition::Building;
	default:
		return ClipChoiceOf(state) == ClipChoice::Walk ? Transition::Walk : Transition::None;
	}
}

bool villager_animation::CarriesInArms(CarriedObject carried)
{
	switch (carried)
	{
	case CarriedObject::None:
	case CarriedObject::Saw:
	case CarriedObject::Ball:
	case CarriedObject::Hammer:
	case CarriedObject::NoChange:
		return false;
	default:
		return true;
	}
}

AnimId villager_animation::WalkClip(const WalkInputs& in)
{
	if (!in.scriptControlled)
	{
		if (in.life <= in.lifeWhenCrawlsWounded)
		{
			return AnimId::PCrawlInjured;
		}
		if (in.life <= in.lifeWhenWalksWounded)
		{
			return AnimId::PWalkInjured;
		}
	}
	const bool carrying = CarriesInArms(in.carried);
	if (in.speed <= in.walkMax)
	{
		if (carrying)
		{
			return AnimId::PCarryAxe;
		}
		return in.female ? AnimId::PWalkWoman : AnimId::PWalkMan;
	}
	if (carrying)
	{
		return AnimId::PCarryObjectRun;
	}
	if (in.speed <= in.runMax)
	{
		return in.female ? AnimId::PRunWoman : AnimId::PRunMan;
	}
	return in.female ? AnimId::PSprintRunWoman : AnimId::PSprintRunMan;
}

AnimId villager_animation::DyingClip(bool onWater, bool onBack)
{
	if (onWater)
	{
		return AnimId::PIntoDeadDrowned;
	}
	return onBack ? AnimId::PDead2 : AnimId::PDying;
}

AnimId villager_animation::DeadClip(bool onWater, bool onBack)
{
	if (onWater)
	{
		return AnimId::PDeadDrowned;
	}
	return onBack ? AnimId::PDead2 : AnimId::PDead1;
}

AnimId villager_animation::KissingClip(bool female)
{
	return female ? AnimId::PCoupleKissWoman : AnimId::PCoupleKissMan;
}

AnimId villager_animation::YawnClip(const IntRandom& random)
{
	return random(2) == 1 ? AnimId::PYawn2 : AnimId::PYawn;
}

AnimId villager_animation::PauseForASecondClip(bool poisoned, const IntRandom& random)
{
	if (poisoned)
	{
		return AnimId::PPoisoned;
	}
	return random(2) == 1 ? AnimId::POverworked2 : AnimId::POverworked1;
}

AnimId villager_animation::LookAtLargeObjectClip(uint16_t turnsUntilStateChange)
{
	const auto low = static_cast<uint8_t>(turnsUntilStateChange & 0xFFu);
	const auto high = static_cast<uint8_t>(turnsUntilStateChange >> 8u);
	return (high >> 1u) < low ? AnimId::PLookAtHand : AnimId::PLookingForSomething;
}

int32_t villager_animation::LookAtFlyingObjectClip(int32_t previousTableClip, float heightAboveLand)
{
	if (previousTableClip == k_HiddenClip)
	{
		return k_HiddenClip;
	}
	return static_cast<int32_t>(heightAboveLand > k_LookUpHeight ? AnimId::PLookAtHand : AnimId::PStand);
}

AnimId villager_animation::InspectCreatureClip(bool womanOrChild, std::optional<float> distanceToTarget,
                                               const IntRandom& random)
{
	if (womanOrChild && random(3) == 0)
	{
		return AnimId::PScaredStiff;
	}
	if (distanceToTarget.has_value() && *distanceToTarget < k_LookAtCreatureDistance && random(3) == 0)
	{
		return AnimId::PLookAtHand;
	}
	return random(8) > 2 ? AnimId::PTalkingAndPointing : AnimId::PStand;
}

AnimId villager_animation::RespectCreatureClip(const IntRandom& random)
{
	const auto roll = random(5);
	if (roll == 0)
	{
		return AnimId::PCrowdImpressed_1;
	}
	return roll <= 2 ? AnimId::PStand : AnimId::PPray;
}

AnimId villager_animation::ControlledByCreatureClip(const IntRandom& random)
{
	return random(3) == 2 ? AnimId::PWaitingImpateintly : AnimId::PStand;
}

AnimId villager_animation::AmazedByShieldClip(const IntRandom& random)
{
	const auto roll = random(5);
	if (roll == 0)
	{
		return AnimId::PIntoPointing;
	}
	return roll <= 2 ? AnimId::PLookAtHand : AnimId::PStand;
}

AnimId villager_animation::TownEmergencyClip(bool female, const IntRandom& random)
{
	switch (random(10))
	{
	case 0:
		return AnimId::PAttractYourAttention;
	case 1:
		return AnimId::PBeckon;
	case 2:
		return AnimId::PConductMeeting;
	case 3:
		return female ? AnimId::PPanicWoman : AnimId::PPanicMan;
	case 4:
		return AnimId::PScaredStiff;
	case 5:
		return AnimId::PScaredStiff_2;
	case 6:
		return AnimId::PStandDespair_1;
	case 7:
		return AnimId::PStandDespair_2;
	case 8:
		return AnimId::PStandDespair_3;
	default:
		return AnimId::PTalkingAndPointing;
	}
}

AnimId villager_animation::RandomCrowdClip(const IntRandom& random)
{
	switch (random(25))
	{
	case 0:
		return AnimId::PAmbient2;
	case 1:
		return AnimId::PAmbient1;
	case 2:
		return AnimId::PWaitingImpateintly;
	case 3:
		return AnimId::PCrowdImpressed_1;
	case 4:
	case 5:
		return AnimId::PCrowdWon;
	default:
		return AnimId::PStand;
	}
}

AnimId villager_animation::SitDownClip(bool transitionPlaying, AnimId current, const IntRandom& random)
{
	if (transitionPlaying)
	{
		if (current == AnimId::PSittingDown1Into)
		{
			return AnimId::PSittingDown1Sitting;
		}
		if (current == AnimId::PSittingDown2Into)
		{
			return AnimId::PSittingDown2Sitting;
		}
	}
	return random(2) != 0 ? AnimId::PSittingDown2Sitting : AnimId::PSittingDown1Sitting;
}

villager_animation::BuildingClipChoice villager_animation::BuildingClip(bool transitionPlaying, AnimId current,
                                                                        const IntRandom& random)
{
	if (transitionPlaying)
	{
		switch (current)
		{
		case AnimId::PHammering:
			return {.clip = AnimId::PHammering, .tool = CarriedObject::Hammer};
		case AnimId::PSawWood:
			return {.clip = AnimId::PSawWood, .tool = CarriedObject::Saw};
		case AnimId::PSledgehammer:
			return {.clip = AnimId::PSledgehammer, .tool = CarriedObject::MalletHeavy};
		default:
			break;
		}
	}
	switch (random(3))
	{
	case 0:
		return {.clip = AnimId::PHammering, .tool = CarriedObject::Hammer};
	case 1:
		return {.clip = AnimId::PSawWood, .tool = CarriedObject::Saw};
	default:
		return {.clip = AnimId::PSledgehammer, .tool = CarriedObject::MalletHeavy};
	}
}

namespace
{
AnimId QuarterSplit(uint32_t roll, AnimId a, AnimId b, AnimId c, AnimId d)
{
	if (roll < 25)
	{
		return a;
	}
	if (roll < 50)
	{
		return b;
	}
	return roll < 75 ? c : d;
}
} // namespace

AnimId villager_animation::FootballWaitForKickOffClip(const IntRandom& random)
{
	return QuarterSplit(random(100), AnimId::PWaitingForKickOff, AnimId::PWaitingForKickOff_2, AnimId::PWaitingForKickOff_3,
	                    AnimId::PWaitingForKickOff_4);
}

AnimId villager_animation::FootballMatchPausedClip(const IntRandom& random)
{
	return QuarterSplit(random(100), AnimId::PLookingForSomething, AnimId::PStand, AnimId::PWaitingForBall,
	                    AnimId::PWaitingForBall_2);
}

AnimId villager_animation::FootballWatchMatchClip(std::optional<uint32_t> matchState, const IntRandom& random)
{
	if (!matchState.has_value())
	{
		return AnimId::PGossipMan;
	}
	if (*matchState == k_MatchGoal)
	{
		switch (random(3))
		{
		case 0:
			return AnimId::PStandDespair_1;
		case 1:
			return AnimId::PCrowdWon;
		default:
			return AnimId::PCrowdWon_2;
		}
	}
	if (*matchState != k_MatchPlaying)
	{
		switch (random(3))
		{
		case 0:
			return AnimId::PTalkingAndPointing;
		case 1:
			return AnimId::PTalkToNeighbour;
		default:
			return AnimId::PGossipMan;
		}
	}
	return AnimId::PCrowdUnimpressed_1;
}

std::optional<AnimId> villager_animation::TransitionClip(Transition transition, bool into, bool destinationIsCurrent,
                                                         bool deathState, AnimId current)
{
	switch (transition)
	{
	case Transition::SleepOnTheGround:
		return into ? AnimId::PIntoSleep : AnimId::POutOfSleep;
	case Transition::Mourn:
		return into ? AnimId::PIntoMourning : AnimId::POutOfMourning;
	case Transition::Pray:
		return into ? AnimId::PIntoPray : AnimId::POutOfPray;
	case Transition::ArrivesAtResource:
		if (into || destinationIsCurrent)
		{
			return std::nullopt;
		}
		return AnimId::PPickUpSticks;
	case Transition::Walk:
		// Only a crawling villager gets up into its next state, or goes down into a crawl
		if (deathState || current != AnimId::PCrawlInjured)
		{
			return std::nullopt;
		}
		return into ? AnimId::PCrawlInjuredInto : AnimId::POutOfSleep;
	case Transition::SitDown:
		if (current == AnimId::PSittingDown1Sitting)
		{
			return into ? AnimId::PSittingDown1Into : AnimId::PSittingDown1OutOf;
		}
		return into ? AnimId::PSittingDown2Into : AnimId::PSittingDown2OutOf;
	case Transition::Building:
		switch (current)
		{
		case AnimId::PHammering:
			return into ? AnimId::PIntoHammering : AnimId::POutOfHammering;
		case AnimId::PSawWood:
			return into ? AnimId::PIntoSawWood : AnimId::POutOfSawWood;
		case AnimId::PSledgehammer:
			return into ? AnimId::PIntoSledgehammer : AnimId::POutOfSledgehammer;
		default:
			return std::nullopt;
		}
	case Transition::None:
	default:
		return std::nullopt;
	}
}

CarriedObject villager_animation::WoodCarriedObject(uint8_t woodKind)
{
	switch (woodKind)
	{
	case 1:
		return CarriedObject::Tree_1;
	case 2:
		return CarriedObject::Tree_2;
	case 3:
		return CarriedObject::Tree_3;
	default:
		return CarriedObject::Wood;
	}
}

std::optional<CarriedObject> villager_animation::CarriedObjectOf(const CarryInputs& in)
{
	if (in.scriptHeld)
	{
		return std::nullopt;
	}
	auto carried = CarriedObject::None;
	if (in.life > in.lifeWhenCrawlsWounded)
	{
		if (in.wood > in.minWoodToShowGraphic)
		{
			carried = WoodCarriedObject(in.woodKind);
		}
		else if (in.food > in.minFoodToShowGraphic && !in.building)
		{
			carried = CarriedObject::Bag;
		}
	}
	if (in.finalStateCarries != 0)
	{
		carried = static_cast<CarriedObject>(in.finalStateCarries);
	}
	if (in.topStateCarries != 0)
	{
		carried = static_cast<CarriedObject>(in.topStateCarries);
	}
	return carried;
}
