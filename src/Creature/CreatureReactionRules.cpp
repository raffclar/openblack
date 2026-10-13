/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureReactionRules.h"

#include <cmath>

#include "Common/GUtilsAngle.h"

using namespace openblack;
using namespace openblack::creature_reaction;
using creature_desires::Desire;

namespace
{
/// A way shorter than this, squared, is no way at all
constexpr float k_NoWaySquared = 1e-6f;
constexpr float k_QuarterTurn = 1.5707964f;
constexpr float k_HalfTurn = 3.1415927f;

/// The heading of a way across the land as a creature keeps it, a quarter turn on from the game's angles: the arctangent
/// of the way's x over its negated z, worked out by octant in single floats as the game does
float HeadingOf(float x, float z)
{
	const float across = -z;
	const auto atan = [](float ratio) { return static_cast<float>(std::atan(static_cast<double>(ratio))); };
	if (!(across < x) && !(-x > across))
	{
		return atan(x / across);
	}
	if (!(x < across) && !(-across > x))
	{
		return k_QuarterTurn - atan(across / x);
	}
	if (!(-x < across) && across < x)
	{
		const float turn = atan(x / across);
		return x < 0.0f ? turn - k_HalfTurn : turn + k_HalfTurn;
	}
	return -k_QuarterTurn - atan(across / x);
}

/// Whether a thing seen at a turn is past its wait now
bool PastWait(std::optional<uint32_t> saw, uint32_t turn)
{
	return !saw.has_value() || static_cast<int32_t>(turn - *saw) > static_cast<int32_t>(k_SightingTurns);
}
} // namespace

bool creature_reaction::CanSee(uint16_t lookAngle, uint16_t angleToPoint, bool sameCell)
{
	const int32_t full = gutils::k_GameAngleCircle;
	auto off = std::abs(static_cast<int32_t>(lookAngle) - static_cast<int32_t>(angleToPoint));
	if (off > full / 2)
	{
		off = full - off;
	}
	return static_cast<uint32_t>(off) <= k_SightHalfAngle || sameCell;
}

uint16_t creature_reaction::LookAngle(glm::vec2 way)
{
	const float heading = way.x * way.x + way.y * way.y > k_NoWaySquared ? HeadingOf(way.x, way.y) : 0.0f;
	return static_cast<uint16_t>(gutils::ConvertScawenAngleToGameAngle(heading) &
	                             static_cast<uint32_t>(gutils::k_GameAngleMask));
}

std::optional<uint8_t> creature_reaction::Priority(Reaction type, const Facts& facts,
                                                   const std::function<uint32_t(uint32_t)>& random)
{
	// Another player's flying or crushed thing is mostly passed over, by a draw made only then
	const auto othersPassedOver = [&]() { return facts.otherPlayers && random(k_OtherPlayersDraw) >= k_OtherPlayersChance; };
	switch (type)
	{
	case Reaction::ReactToFood:
		return facts.humanPlayer && facts.seesThing ? facts.tablePriority : 0;
	case Reaction::ReactToMagicTree:
	case Reaction::ReactToHandPuttingStuffInStoragePit:
		return facts.humanPlayer ? facts.tablePriority : 0;
	case Reaction::ReactToHandUsingTotem:
		return facts.humanPlayer && facts.seesThing ? facts.tablePriority : 0;
	case Reaction::ReactToFlyingObject:
		if (!facts.humanPlayer || othersPassedOver())
		{
			return 0;
		}
		return PastWait(facts.sawFlying, facts.turn) && !facts.catching && facts.thrownFromHand && facts.inTheAir &&
		               facts.seesThing
		           ? facts.tablePriority
		           : 0;
	case Reaction::ReactToHandPickUp:
		return facts.humanPlayer && facts.ownPlayersHand && facts.handHolds && facts.seesThing
		           ? static_cast<uint8_t>(facts.tablePriority * k_HandPickUpUrgency)
		           : 0;
	case Reaction::ReactToObjectCrushed:
		if (!facts.humanPlayer || othersPassedOver())
		{
			return 0;
		}
		return PastWait(facts.sawCrushed, facts.turn) && !facts.angry && !facts.thingIsSelf && facts.seesThing
		           ? facts.tablePriority
		           : 0;
	case Reaction::ReactToFight:
		if (facts.thingGone || facts.thingIsSelf || facts.fighting || !facts.seesThing || !facts.thingIsCreature)
		{
			return 0;
		}
		return facts.thingFightsSelf ? 0 : facts.tablePriority;
	case Reaction::ReactToDeath:
		return !facts.thingGone && facts.thingIsCreature ? facts.tablePriority : 0;
	case Reaction::ReactToDroppedByHand:
		return 0;
	default:
		return std::nullopt;
	}
}

std::optional<Setup> creature_reaction::SetupFor(Reaction type, bool thingInTheAir)
{
	switch (type)
	{
	case Reaction::LookAtObject:
	case Reaction::LookAtSpell:
	case Reaction::ReactToScaffold:
	case Reaction::FollowObject:
	case Reaction::FollowSpell:
		return Setup {};
	case Reaction::ReactToFood:
	case Reaction::ReactToGiftFromCreature:
		return Setup {.comesAcross = true};
	case Reaction::ReactToFlyingObject:
		// It looks up at a thing still flying; one already down it doesn't take up at all
		if (!thingInTheAir)
		{
			return Setup {.keeps = false, .notesFlying = true};
		}
		return Setup {.notesFlying = true,
		              .comesAcross = true,
		              .plan = Plan {.desire = Desire::Curiosity, .action = "LookAtFlyingObject"}};
	case Reaction::ReactToHandPickUp:
		return Setup {.plan = Plan {.desire = Desire::Curiosity, .action = "LookAtHand", .onSelf = true}};
	case Reaction::ReactToHandUsingTotem:
		return Setup {.comesAcross = true,
		              .plan = Plan {.desire = Desire::Curiosity, .action = "ExamineByLooking"},
		              .skill = k_UseTotemSkill};
	case Reaction::ReactToObjectCrushed:
		return Setup {.notesCrushed = true,
		              .comesAcross = true,
		              .plan = Plan {.desire = Desire::Curiosity, .action = "ExamineByLooking"}};
	case Reaction::ReactToHandPuttingStuffInStoragePit:
		return Setup {.comesAcross = true, .skill = k_UseStoragePitSkill};
	case Reaction::ReactToDeath:
		return Setup {.desireSource = std::pair {creature_desires::sources::k_Sadness, k_SadnessAtDeath}};
	case Reaction::Fainting:
		return Setup {.plan = Plan {.desire = Desire::Tiredness, .action = "SleepAtHome", .onSelf = true}};
	case Reaction::Confused:
		return Setup {.comesAcross = true, .plan = Plan {.desire = Desire::Tiredness, .action = "ExamineByLooking"}};
	case Reaction::ReactToFight:
	case Reaction::ReactToWood:
	case Reaction::ReactToNewBuilding:
	case Reaction::ReactToDroppedByHand:
	case Reaction::AvoidFallingTree:
	case Reaction::CrowdAround:
	case Reaction::ReactToBreeder:
	case Reaction::ReactToTownCelebration:
	case Reaction::ReactToVillagerInHand:
	case Reaction::ReactToBurningObjectInHand:
	case Reaction::ReactToMagicShieldStruck:
	case Reaction::ReactToMagicShieldDestroyed:
	case Reaction::ReactToMagicWaterPuttingOutFire:
	case Reaction::ReactToMissionary:
	case Reaction::ReactToFightWon:
		// Heeded, if at all, without being kept
		return Setup {.keeps = false};
	default:
		return std::nullopt;
	}
}
