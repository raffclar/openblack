/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureActivity.h"

#include "Creature/CreatureDecisionTree.h"
#include "Creature/CreaturePlanner.h"

using namespace openblack;
using namespace openblack::creature_activity;
using creature_desires::Desire;

std::optional<Whole> creature_activity::WholeOf(uint32_t beliefKind)
{
	switch (beliefKind)
	{
	case creature_tree::belief_types::k_Town:
		return Whole::Town;
	case creature_tree::belief_types::k_Forest:
		return Whole::Forest;
	case creature_tree::belief_types::k_Citadel:
		return Whole::Citadel;
	case creature_tree::belief_types::k_Creature:
		return Whole::Creature;
	case creature_tree::belief_types::k_Flock:
		return Whole::Flock;
	default:
		return std::nullopt;
	}
}

namespace
{
/// Whether anger can be about a whole thing, by the kind of the thing
bool AngerCanBeAbout(const WholeFacts& whole)
{
	switch (whole.kind)
	{
	case Whole::Town:
		return !whole.samePlayer || whole.lookedAt;
	case Whole::Creature:
		return whole.attackable;
	case Whole::Citadel:
		return !whole.samePlayer;
	case Whole::Forest:
	case Whole::Flock:
		return true;
	}
	return false;
}
} // namespace

bool creature_activity::CanBeAbout(Desire desire, const WholeFacts& whole, bool mayAttackOwnPlayer)
{
	switch (desire)
	{
	case Desire::Impress:
		return whole.kind == Whole::Town && !(whole.believesInPlayer && whole.samePlayer);
	case Desire::Compassion:
		return whole.kind == Whole::Town || whole.kind == Whole::Creature;
	case Desire::Anger:
		return (mayAttackOwnPlayer || !whole.samePlayer) && AngerCanBeAbout(whole);
	case Desire::BeFriends:
	case Desire::ObeyCreature:
	case Desire::TellCreature:
	case Desire::EducateFriend:
	case Desire::MissFriend:
		return whole.kind == Whole::Creature;
	default:
		return false;
	}
}

bool creature_activity::PartSuits(Desire desire, const PartFacts& part)
{
	if (!part.canActOn)
	{
		return false;
	}
	const bool lives = part.kind == Part::Villager || part.kind == Part::Animal;
	switch (desire)
	{
	case Desire::Impress:
		return part.kind == Part::Villager && !part.dancing;
	case Desire::Compassion:
		return part.kind != Part::Villager || part.pickable;
	case Desire::Anger:
		if (part.kind == Part::Tree)
		{
			return true;
		}
		return part.attackable && !(lives && part.dead);
	default:
		return true;
	}
}

std::optional<size_t> creature_activity::MostUseful(std::span<const float> usefulness)
{
	std::optional<size_t> best;
	float most = 0.0f;
	for (size_t i = 0; i < usefulness.size(); ++i)
	{
		if (usefulness[i] > most)
		{
			best = i;
			most = usefulness[i];
		}
	}
	return best;
}

float creature_activity::ActedOnUsefulness(Desire desire, float partUsefulness)
{
	return desire == Desire::Anger ? partUsefulness : creature_planner::k_DefaultUsefulness;
}
