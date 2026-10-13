/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureGiving.h"

#include <cmath>

#include <algorithm>

#include <glm/geometric.hpp>

using namespace openblack;
using namespace openblack::creature_giving;

bool Offer::RunDown(int32_t frameMs, bool reaching)
{
	if (ms <= 0 || reaching)
	{
		return false;
	}
	ms -= frameMs;
	if (ms < 0)
	{
		ms = 0;
		return true;
	}
	return false;
}

std::optional<float> creature_giving::OfferTurn(HoldType hold, bool rightHanded, const Offer& offer, bool reaching)
{
	if (offer.ms < k_OfferTurnsBackMs && !reaching)
	{
		return 0.0f;
	}
	constexpr float k_Pi = std::numbers::pi_v<float>;
	switch (hold)
	{
	case HoldType::Above:
		return rightHanded ? -k_Pi : k_Pi;
	case HoldType::Tree:
	case HoldType::Side:
	case HoldType::Villager:
		return (rightHanded ? -k_Pi : k_Pi) * 0.5f;
	default:
		return std::nullopt;
	}
}

Verdict creature_giving::MayGive(const GiveFacts& facts)
{
	if (!facts.creatureHasPlayer || !facts.giverHasPlayer || !facts.friendly || facts.spellSeed)
	{
		return Verdict::No;
	}
	return facts.creatureCanPickUp ? Verdict::Give : Verdict::CantPickUp;
}

std::optional<glm::vec3> creature_giving::DrawToCreature(glm::vec3 creatureHand, glm::vec3 target, float msFromTake, float hang)
{
	const auto ms = std::abs(msFromTake);
	if (!(ms < k_DrawMs))
	{
		return std::nullopt;
	}
	const auto share = ms * k_DrawPerMs;
	auto drawn = creatureHand + (target - creatureHand) * share;
	drawn.y -= (1.0f - share) * hang;
	return drawn;
}

bool creature_giving::CallsOff(glm::vec3 hand, glm::vec3 target, float creatureSize)
{
	return glm::distance(target, hand) > creatureSize * k_CallOffSizes;
}

Choice creature_giving::ChooseUse(const GiftFacts& facts)
{
	struct Candidate
	{
		Weighed weighed;
		Use use;
	};
	const std::array<Candidate, 3> candidates {{
	    {.weighed = facts.curiosity, .use = Use::Examine},
	    {.weighed = facts.hunger, .use = Use::Eat},
	    {.weighed = facts.playWithPlayer, .use = Use::Throw},
	}};
	float best = 0.0f;
	float strongest = 0.0f;
	std::optional<Use> chosen;
	for (const auto& candidate : candidates)
	{
		const auto score = candidate.weighed.desire * candidate.weighed.usefulness;
		strongest = std::max(strongest, candidate.weighed.desire);
		if (score > best && !(candidate.use == Use::Eat && facts.cantEatKind))
		{
			best = score;
			chosen = candidate.use;
		}
	}
	if (chosen.has_value() && !(strongest < k_LeastDesire))
	{
		return {.use = *chosen};
	}
	// No desire for it: the first thing of a kind is looked over, later ones are acted on as when led to them
	if (!facts.givenKindBefore)
	{
		return {.use = Use::Examine, .remembersKind = true};
	}
	return {.use = Use::AsLed};
}
