/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureObjectActions.h"

#include <algorithm>

#include <glm/geometric.hpp>

#include "Common/GUtilsDistance.h"
#include "Creature/CreatureCastMoves.h"

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
	case Kind::Kick:
		return "Kicking";
	}
	return "Nothing";
}

bool creature_object_actions::ShowsHoldPose(std::optional<Kind> playing)
{
	if (!playing.has_value())
	{
		return true;
	}
	switch (*playing)
	{
	case Kind::PickUp:
	case Kind::PutDown:
	case Kind::Discard:
	case Kind::Eat:
	case Kind::Keep:
	case Kind::Throw:
	case Kind::Catch:
	case Kind::TakeFromHand:
		return false;
	case Kind::Destroy:
	case Kind::Point:
	case Kind::Kick:
		return true;
	}
	return true;
}

Hands creature_object_actions::HandsFor(Kind kind)
{
	switch (kind)
	{
	case Kind::PutDown:
	case Kind::Discard:
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
	case Kind::Kick:
		return Hands::Either;
	}
	return Hands::Either;
}

bool creature_object_actions::LeavesThingAlone(Kind kind, Failure failure)
{
	const bool walksToThing = kind == Kind::PickUp || kind == Kind::Destroy || kind == Kind::Kick;
	return walksToThing && failure != Failure::Other;
}

creature_object_actions::ApproachStep creature_object_actions::NextApproachStep(bool seen, float flatDistance,
                                                                                float straightReach, bool inRange)
{
	if (!seen)
	{
		return ApproachStep::WalkNear;
	}
	return flatDistance < straightReach || inRange ? ApproachStep::Begin : ApproachStep::WalkInReach;
}

float creature_object_actions::NearWalkDistance(float creatureRadius, float thingRadius)
{
	const float radii = creatureRadius + thingRadius;
	return radii + radii;
}

bool creature_object_actions::TooHighToPickUp(float rise, float distance)
{
	return distance < rise && rise > k_MostPickUpRise;
}

bool creature_object_actions::StallsOnTurn(ApproachStall& stall, glm::vec2 here, bool scripted)
{
	const float moved = gutils::GetDistanceInMetres(here, stall.note);
	const auto check = creature_cast_moves::CheckStall(stall.turns, moved, scripted);
	++stall.turns;
	if (check == creature_cast_moves::Stall::Note)
	{
		stall.note = here;
	}
	return check == creature_cast_moves::Stall::GiveUp;
}

bool creature_object_actions::RefusesAloud(Failure failure)
{
	return failure == Failure::NoRoute;
}

glm::vec3 creature_object_actions::KickDirection(glm::vec2 forward, glm::vec2 lean, float scale)
{
	// The game takes the creature's up row less its back row, from its turn, its lean and its scale. The forward way is
	// (sine, -cosine) of the turn.
	const float sine = forward.x;
	const float cosine = -forward.y;
	const float leanedUp = (-sine * lean.x) + (cosine * lean.y);
	const glm::vec3 direction {scale * sine, scale - (scale * leanedUp), -(scale * cosine)};
	const float squares = ((direction.x * direction.x) + (direction.z * direction.z)) + (direction.y * direction.y);
	auto unit = direction;
	if (direction != glm::vec3(0.0f))
	{
		unit *= 1.0f / std::sqrt(squares);
	}
	return unit;
}

glm::vec3 creature_object_actions::KickVelocity(glm::vec2 forward, glm::vec2 lean, float scale, float size)
{
	return (size * k_KickSpeedPerSize) * KickDirection(forward, lean, scale);
}

bool creature_object_actions::ThrowOver(bool animationOver, bool waitsForLanding, bool thrownFlying)
{
	return animationOver && !(waitsForLanding && thrownFlying);
}

bool creature_object_actions::BringsBackUp(bool indestructible, bool heldDuringCutscene)
{
	return indestructible || heldDuringCutscene;
}

bool creature_object_actions::IsToy(const KindFacts& kind)
{
	return kind.toy || kind.thing == Thing::Ball;
}

bool creature_object_actions::IsStatic(Thing thing)
{
	switch (thing)
	{
	case Thing::Rock:
	case Thing::Bonfire:
	case Thing::DeadTree:
	case Thing::Static:
	case Thing::Vortex:
	case Thing::TeleportStone:
		return true;
	default:
		return false;
	}
}

bool creature_object_actions::MayEat(const KindFacts& kind, const EatFacts& facts)
{
	return !facts.refusedSort && !facts.computersOwnVillager && !IsToy(kind) && facts.mayPickUp;
}

bool creature_object_actions::MayEatHeld(const KindFacts& kind, bool mayPickUp)
{
	return IsStatic(kind.thing) ? !IsToy(kind) : mayPickUp;
}

bool creature_object_actions::BringsMealBackUp(float food, bool magicMushroom, bool foodPile, bool poisoned)
{
	return !magicMushroom && !foodPile && (food < k_LeastFoodKeptDown || poisoned);
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
	case Thing::Fixture:
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
	case Thing::MagicMushroom:
		return KindRule::Mushroom;
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

std::optional<creature_object_actions::Toy> creature_object_actions::ToyOf(MeshId mesh)
{
	switch (mesh)
	{
	case MeshId::ObjectToyBall:
		return Toy::Ball;
	case MeshId::ObjectToyBowlingBall:
		return Toy::BowlingBall;
	case MeshId::ObjectToyCuddly:
		return Toy::Cuddly;
	case MeshId::ObjectToyDice:
		return Toy::Die;
	case MeshId::ObjectToySkittle:
		return Toy::Skittle;
	default:
		return std::nullopt;
	}
}

bool creature_object_actions::Holdable(Thing thing)
{
	return thing != Thing::Other && thing != Thing::Fixture && thing != Thing::Creature && thing != Thing::Lantern &&
	       thing != Thing::Shield && thing != Thing::MagicMushroom;
}

bool creature_object_actions::IsLiving(Thing thing)
{
	return thing == Thing::Villager || thing == Thing::Animal || thing == Thing::Bird || thing == Thing::Creature;
}

bool creature_object_actions::CanBeStoned(Thing thing, bool dead)
{
	switch (thing)
	{
	case Thing::Villager:
	case Thing::Animal:
	case Thing::Bird:
	case Thing::Creature:
		return !dead;
	case Thing::Rock:
	case Thing::Bonfire:
	case Thing::DeadTree:
	case Thing::Static:
	case Thing::MagicMushroom:
	case Thing::Shield:
	case Thing::Fixture:
		return true;
	case Thing::Other:
	case Thing::Ball:
	case Thing::Creed:
	case Thing::MobileObject:
	case Thing::Pot:
	case Thing::Pile:
	case Thing::Tree:
	case Thing::Lantern:
	case Thing::Vortex:
	case Thing::TeleportStone:
		return false;
	}
	return false;
}

bool creature_object_actions::DoesThrowingDamage(Thing thing, bool toy)
{
	switch (thing)
	{
	case Thing::Rock:
	case Thing::Bonfire:
	case Thing::DeadTree:
	case Thing::Static:
	case Thing::Vortex:
	case Thing::TeleportStone:
		return !toy;
	default:
		return false;
	}
}

bool creature_object_actions::CanBeThrownToImpress(Thing thing, float thingHeight, float creatureWeight)
{
	if (IsLiving(thing) || thing == Thing::Vortex || thing == Thing::TeleportStone)
	{
		return false;
	}
	return thingHeight < creatureWeight * k_ThrowToImpressWeightShare;
}

bool creature_object_actions::CanBeThrownInTheSea(Thing thing)
{
	switch (thing)
	{
	case Thing::Ball:
	case Thing::Creed:
	case Thing::MobileObject:
	case Thing::Pot:
	case Thing::Pile:
	case Thing::Rock:
	case Thing::Bonfire:
	case Thing::DeadTree:
	case Thing::Static:
	case Thing::Vortex:
	case Thing::TeleportStone:
	case Thing::Tree:
		return true;
	default:
		return false;
	}
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

creature_object_actions::EatenThingEnd creature_object_actions::EatenThingEndFor(bool eatGoesOn, bool creatureExists)
{
	if (!creatureExists)
	{
		return EatenThingEnd::Fall;
	}
	return eatGoesOn ? EatenThingEnd::Wait : EatenThingEnd::Swallow;
}

creature_object_actions::HoldStart creature_object_actions::HoldStartFor(bool living, bool creature, bool acceptsHandState)
{
	if (!living || creature)
	{
		return HoldStart::NoChange;
	}
	return acceptsHandState ? HoldStart::IntoHandState : HoldStart::Refused;
}
