/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ReactionTimingRules.h"

#include <cmath>

#include <glm/geometric.hpp>

#include "InfoConstants.h"

using namespace openblack;
using namespace openblack::magic;

namespace
{
/// A villager's or an animal's nearness counts within the row's distance and this much further
constexpr double k_ReachExtra = 10.0;
/// Its nearness lengthens the time by up to half as much again
constexpr double k_NearnessShare = 0.5;
/// A predator faster than this, in metres a second, counts as running
constexpr float k_RunningPredator = 5.0f;
/// Heading and the way to the point agree when the cosine between them is at least this
constexpr float k_ComingAtCosine = 0.8f;

/// Turns as the game counts them: the whole part, towards zero
int32_t Whole(double turns)
{
	return static_cast<int32_t>(static_cast<int64_t>(turns));
}

/// How much longer than its turns one keeps at a thing, by its nearness within the reach
double Nearness(const villager_reaction::Distance& distance, float at)
{
	const double reach = static_cast<double>(distance.maxDistance) + k_ReachExtra;
	return (reach - static_cast<double>(at)) / reach * static_cast<double>(distance.importance) * k_NearnessShare + 1.0;
}
} // namespace

reaction_timing::Rule reaction_timing::ReactRule(Reaction type)
{
	switch (type)
	{
	case Reaction::ReactToCreature:
		return Rule::Creature;
	case Reaction::ReactToMagicShield:
		return Rule::Shield;
	case Reaction::FleeFromPredator:
		return Rule::Predator;
	case Reaction::ReactToBurningObjectInHand:
		return Rule::BurningObject;
	// Belief only: no one holds these
	case Reaction::ReactToMagicWaterPuttingOutFire:
	case Reaction::ReactToMissionary:
	case Reaction::ReactToFightWon:
	case Reaction::None:
		return Rule::None;
	default:
		return Rule::Standard;
	}
}

reaction_timing::Rule reaction_timing::AgainRule(Reaction type)
{
	switch (type)
	{
	case Reaction::ReactToCreature:
		return Rule::Creature;
	case Reaction::ReactToWood:
		return Rule::Wood;
	case Reaction::ReactToMagicShield:
		return Rule::Shield;
	case Reaction::FleeFromPredator:
		return Rule::Predator;
	case Reaction::ReactToBurningObjectInHand:
		return Rule::BurningObject;
	case Reaction::None:
		return Rule::None;
	default:
		return Rule::Standard;
	}
}

reaction_timing::Row reaction_timing::RowOf(const ReactionInfo& info)
{
	return {.turnsToReact = info.numGameTurnsForNormalThingsToReact,
	        .turnsBeforeReactingAgain = info.numGameTurnsForNormalThingsBeforeReactingAgain,
	        .creatureTurnsToReact = info.numGameTurnsForCreatureToReact,
	        .creatureTurnsBeforeReactingAgain = info.numGameTurnsForCreatureBeforeReactingAgain,
	        .distance = {.maxDistance = info.maxReactionDistance, .importance = info.howImportantIsDistance}};
}

int32_t reaction_timing::StandardTurnsToReact(const Row& row, Reacting who, float distance)
{
	if (who == Reacting::Creature)
	{
		const double reach = row.distance.maxDistance;
		const double importance = row.distance.importance;
		const double factor = (reach - static_cast<double>(distance)) * importance / reach + (1.0 - importance);
		return Whole(static_cast<double>(row.creatureTurnsToReact) * factor);
	}
	return Whole(static_cast<double>(static_cast<int32_t>(row.turnsToReact)) * Nearness(row.distance, distance));
}

int32_t reaction_timing::StandardTurnsBeforeReactingAgain(const Row& row, Reacting who, float distance)
{
	if (who == Reacting::Creature)
	{
		const double reach = row.distance.maxDistance;
		const double importance = row.distance.importance;
		const double factor = importance * static_cast<double>(distance) / reach + (1.0 - importance);
		return Whole(static_cast<double>(row.creatureTurnsBeforeReactingAgain) * factor);
	}
	return Whole(static_cast<double>(static_cast<int32_t>(row.turnsBeforeReactingAgain)) * Nearness(row.distance, distance));
}

int32_t reaction_timing::PredatorTurnsToReact(const PredatorWatch& watch, Reacting who, int32_t standard)
{
	if (who == Reacting::Villager && watch.hidingOrLooking)
	{
		return k_Forever;
	}
	if (!watch.animal)
	{
		return 0;
	}
	if (watch.lookingAtThing && watch.speed > k_RunningPredator && watch.comingAt)
	{
		return 0;
	}
	return standard;
}

int32_t reaction_timing::PredatorTurnsBeforeReactingAgain(const PredatorWatch& watch, Reacting who, int32_t standard)
{
	if (who == Reacting::Villager && watch.lookingIfSafe)
	{
		return 0;
	}
	if (watch.animal && watch.speed <= k_RunningPredator && watch.distance >= watch.leastRunDistance)
	{
		return standard;
	}
	return 0;
}

bool reaction_timing::ComingAt(const glm::vec3& thing, const glm::vec2& heading, const glm::vec3& point)
{
	auto way = point - thing;
	if (way != glm::vec3(0.0f))
	{
		way = glm::normalize(way);
	}
	auto along = glm::vec3(heading.x, 0.0f, heading.y);
	if (along != glm::vec3(0.0f))
	{
		along = glm::normalize(along);
	}
	return glm::dot(way, along) >= k_ComingAtCosine;
}

bool reaction_timing::ReactsToItself(Reaction type)
{
	return type == Reaction::Fainting || type == Reaction::Confused;
}

bool reaction_timing::LooksAtThing(Reaction type)
{
	return type == Reaction::LookAtObject || type == Reaction::LookAtSpell || type == Reaction::LookAtNiceSpell ||
	       type == Reaction::ReactToImpressiveSpell || type == Reaction::ReactToScaffold;
}
