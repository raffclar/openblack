/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ComputerMindDefenceRules.h"

#include "Common/GUtilsDistance.h"
#include "Common/X87.h"
#include "ECS/ComputerMindRules.h"

using namespace openblack;
using namespace openblack::computer_mind_rules;

std::optional<float> computer_mind_rules::HitReaction(uint32_t turn, uint32_t lastHit)
{
	if (lastHit == 0)
	{
		return std::nullopt;
	}
	// The turns since the hit, as a number that is never negative
	const uint32_t since = turn - lastHit;
	if (since <= k_RecentHitTurns)
	{
		return std::nullopt;
	}
	float forgotten = x87::Mul(static_cast<double>(since), k_HitMemoryShare);
	if (!(forgotten < 1.0f))
	{
		forgotten = 1.0f;
	}
	return 1.0f - forgotten;
}

float computer_mind_rules::DefendWish(uint32_t turn, uint32_t lastTurn, float aggression)
{
	float strength = aggression + aggression;
	if (!(strength < 1.0f))
	{
		strength = 1.0f;
	}
	// The turn of the attack is kept as a float, and the game turn is taken from it whole
	const float attackTurn = x87::RoundToFloat(static_cast<double>(lastTurn));
	const float since = x87::Sub(static_cast<double>(turn), attackTurn);
	float forgotten = x87::Mul(since, k_AttackOnTownMemoryShare);
	if (!(forgotten < 1.0f))
	{
		forgotten = 1.0f;
	}
	const float remembered = 1.0f - forgotten;
	const float cube = remembered * remembered * remembered;
	return strength * cube;
}

std::optional<DefendChoice> computer_mind_rules::ChooseDefence(std::span<const TownAttackFacts> attacks, uint32_t turn)
{
	std::optional<DefendChoice> choice;
	float best = 0.0f;
	for (size_t i = 0; i < attacks.size(); ++i)
	{
		const auto& attack = attacks[i];
		if (!(attack.aggression > 0.0f))
		{
			continue;
		}
		const float wish = DefendWish(turn, attack.lastTurn, attack.aggression);
		if (wish > best || !choice.has_value())
		{
			choice = DefendChoice {.index = i, .value = wish};
			best = wish;
		}
	}
	return choice;
}

float computer_mind_rules::BeliefAttack(const TownBeliefFacts& town, PlayerNames god,
                                        std::span<const bool, static_cast<size_t>(PlayerNames::_COUNT)> allies)
{
	const auto own = static_cast<size_t>(god);
	float most = k_LeastBeliefAttack;
	size_t attacker = 0;
	for (size_t i = 0; i < k_BeliefAttackPlayers; ++i)
	{
		if (i == own || allies[i])
		{
			continue;
		}
		if (town.recent.at(i) > most)
		{
			most = town.recent.at(i);
			attacker = i;
		}
	}
	if (!(most > k_LeastBeliefAttack) || own >= town.belief.size() || town.belief.at(own) == 0.0f)
	{
		return 0.0f;
	}
	const float share = town.belief.at(attacker) / town.belief.at(own);
	const float doubled = share + share;
	const float square = doubled * doubled;
	return square < 1.0f ? square : 1.0f;
}

float computer_mind_rules::BeliefMargin(const TownBeliefFacts& town, PlayerNames god)
{
	const auto own = static_cast<size_t>(god);
	float most = 0.0f;
	for (size_t i = 0; i < town.belief.size(); ++i)
	{
		if (i != own && town.belief.at(i) > most)
		{
			most = town.belief.at(i);
		}
	}
	const float godBelief = own < town.belief.size() ? town.belief.at(own) : 0.0f;
	return godBelief - most;
}

std::optional<BeliefAttackChoice> computer_mind_rules::ChooseBeliefDefence(std::span<const float> attacks,
                                                                           std::span<const float> margins)
{
	std::optional<BeliefAttackChoice> choice;
	float best = 0.0f;
	for (size_t i = 0; i < attacks.size() && i < margins.size(); ++i)
	{
		if (!(attacks[i] > 0.0f) || margins[i] == 0.0f)
		{
			continue;
		}
		if (attacks[i] > best)
		{
			best = attacks[i];
			choice = BeliefAttackChoice {.index = i, .value = attacks[i]};
		}
	}
	return choice;
}

float computer_mind_rules::CloseRivalShare(float godBelief, std::span<const float> rivals)
{
	float most = 0.0f;
	for (const float belief : rivals)
	{
		if (belief > most)
		{
			most = belief;
		}
	}
	return godBelief == 0.0f ? 0.0f : most / godBelief;
}

float computer_mind_rules::BeliefDefenceImportance(bool ownTown, float closeRivalShare)
{
	if (!ownTown)
	{
		return 1.0f;
	}
	const float importance = closeRivalShare * k_CloseRivalScale;
	return importance < 1.0f ? importance : 1.0f;
}

float computer_mind_rules::BeliefDefenceSuit(MagicType type, bool ownShieldThere)
{
	if (ownShieldThere)
	{
		return 0.0f;
	}
	switch (type)
	{
	case MagicType::Shield:
		return 1.0f;
	case MagicType::PhysicalShield:
		return k_PhysicalShieldSuit;
	default:
		return 0.0f;
	}
}

float computer_mind_rules::CreatureThreat(float templeDistance, float influencePower, std::span<const TownNearCreature> towns)
{
	float threat = gutils::GetDistanceModifier(templeDistance, influencePower + 1.0f);
	for (const auto& town : towns)
	{
		if (town.radius * k_InsideTownShare > town.distance)
		{
			threat = threat + 1.0f;
		}
		threat = gutils::GetDistanceModifier(town.distance, town.influence) + threat;
	}
	return threat;
}

float computer_mind_rules::CreatureHelpReach(float size)
{
	const float reach = size * k_CreatureReachPerSize;
	return reach + reach + k_CreatureHelpReach;
}
