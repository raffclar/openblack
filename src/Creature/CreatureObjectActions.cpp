/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureObjectActions.h"

using namespace openblack;
using namespace openblack::creature_object_actions;

std::string_view creature_object_actions::Name(Kind kind)
{
	switch (kind)
	{
	case Kind::PickUp:
		return "Picking up";
	case Kind::PutDown:
		return "Putting down";
	case Kind::Discard:
		return "Tossing away";
	case Kind::Lob:
		return "Lobbing";
	case Kind::Eat:
		return "Eating what it holds";
	case Kind::Keep:
		return "Looking over what it holds";
	case Kind::Throw:
		return "Throwing";
	case Kind::Destroy:
		return "Knocking down";
	case Kind::Point:
		return "Pointing";
	case Kind::Catch:
		return "Catching";
	case Kind::TakeFromHand:
		return "Taking from the hand";
	}
	return "Nothing";
}

Hands creature_object_actions::HandsFor(Kind kind)
{
	switch (kind)
	{
	case Kind::PutDown:
	case Kind::Discard:
	case Kind::Lob:
	case Kind::Eat:
	case Kind::Keep:
	case Kind::Throw:
		return Hands::Holding;
	case Kind::PickUp:
	case Kind::Destroy:
	case Kind::TakeFromHand:
		return Hands::Empty;
	case Kind::Point:
	case Kind::Catch:
		return Hands::Either;
	}
	return Hands::Either;
}

std::string_view creature_object_actions::Name(TownAttitude attitude)
{
	switch (attitude)
	{
	case TownAttitude::Fear:
		return "fear";
	case TownAttitude::Respect:
		return "respect";
	case TownAttitude::None:
		break;
	}
	return "nothing much";
}

TownAttitude creature_object_actions::AttitudeTo(Kind kind, bool targetIsVillager, bool holdingVillager)
{
	if (holdingVillager)
	{
		return TownAttitude::Fear;
	}
	switch (kind)
	{
	case Kind::Eat:
		return targetIsVillager ? TownAttitude::Fear : TownAttitude::None;
	case Kind::Throw:
	case Kind::Destroy:
		return TownAttitude::Fear;
	case Kind::PickUp:
	case Kind::PutDown:
	case Kind::Discard:
	case Kind::Lob:
	case Kind::Keep:
	case Kind::Point:
	case Kind::Catch:
	case Kind::TakeFromHand:
		break;
	}
	return TownAttitude::None;
}

float creature_object_actions::AttitudeSeconds(TownAttitude attitude)
{
	switch (attitude)
	{
	case TownAttitude::Fear:
		return 30.0f;
	case TownAttitude::Respect:
		return 10.0f;
	case TownAttitude::None:
		break;
	}
	return 0.0f;
}

bool creature_object_actions::ThrowOver(bool animationOver, bool waitsForLanding, bool thrownFlying)
{
	return animationOver && !(waitsForLanding && thrownFlying);
}

bool creature_object_actions::BringsBackUp(bool indestructible, bool heldDuringCutscene)
{
	return indestructible || heldDuringCutscene;
}

bool creature_object_actions::CanLift(const LiftFacts& facts)
{
	return facts.thingRadius + facts.creatureRadius <= facts.reach &&
	       facts.thingWeight <= facts.creatureWeight * k_MostLiftedWeightShare;
}

bool creature_object_actions::MayBeCaught(const PickUpFacts& facts)
{
	return !facts.cannotBePickedUp && !facts.villagerAtHome;
}

bool creature_object_actions::MayPickUpWhereverItIs(const PickUpFacts& facts)
{
	if (facts.leashTiedToIt || (facts.lift.has_value() && !CanLift(*facts.lift)))
	{
		return false;
	}
	return MayBeCaught(facts);
}

bool creature_object_actions::MayPickUp(const PickUpFacts& facts)
{
	if (facts.computerCreatureNotScripted && facts.inScript)
	{
		return false;
	}
	if (facts.townArtefact || facts.worshippingForCreaturesPlayer || !facts.inMapOrHeldByCreature)
	{
		return false;
	}
	return MayPickUpWhereverItIs(facts);
}

creature_object_actions::KindRule creature_object_actions::RuleOf(Thing thing)
{
	switch (thing)
	{
	case Thing::Other:
	case Thing::Creature:
	case Thing::Lantern:
	case Thing::Shield:
	case Thing::Vortex:
	case Thing::TeleportStone:
		return KindRule::Never;
	case Thing::Static:
		return KindRule::Toy;
	case Thing::Ball:
	case Thing::Creed:
		return KindRule::Always;
	case Thing::Bird:
		return KindRule::Bird;
	case Thing::Pile:
		return KindRule::Pile;
	case Thing::Villager:
	case Thing::Animal:
	case Thing::MobileObject:
	case Thing::Pot:
	case Thing::Rock:
	case Thing::Bonfire:
	case Thing::DeadTree:
	case Thing::Tree:
		return KindRule::Whole;
	}
	return KindRule::Never;
}

bool creature_object_actions::Holdable(Thing thing)
{
	return thing != Thing::Other && thing != Thing::Creature && thing != Thing::Lantern && thing != Thing::Shield;
}

bool creature_object_actions::MayPickUpKind(const KindFacts& kind, const PickUpFacts& facts)
{
	switch (kind.rule)
	{
	case KindRule::Never:
		return false;
	case KindRule::Always:
		return true;
	case KindRule::Toy:
		return kind.toy;
	case KindRule::Bird:
		return kind.low;
	case KindRule::Mushroom:
		return !facts.cannotBePickedUp;
	case KindRule::Pile:
		return !facts.cannotBePickedUp && kind.mayFly;
	case KindRule::Whole:
		return kind.holdable && MayPickUp(facts);
	}
	return false;
}

bool creature_object_actions::MayTakeKindFromHand(const KindFacts& kind, const PickUpFacts& facts)
{
	return kind.holdable && MayPickUpWhereverItIs(facts);
}

bool creature_object_actions::MayCatchKind(const KindFacts& kind, const PickUpFacts& facts)
{
	return kind.holdable && MayBeCaught(facts);
}
