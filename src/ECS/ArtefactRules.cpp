/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ArtefactRules.h"

#include <algorithm>

using namespace openblack;
using namespace openblack::ecs;

namespace
{
/// Objects narrower than these get the small and the medium object dances
constexpr float k_SmallDanceWidth = 7.0f;
constexpr float k_MediumDanceWidth = 10.0f;
/// A wonder's planning reaches this far for each unit of the town's desire for one, and at least this far
constexpr float k_WonderReachPerDesire = 15.0f;
/// Each thing in reach adds this share of its impressiveness to the wonder's power
constexpr float k_WonderImpressivenessShare = 0.1f;
/// A wonder's power is at least this, and its scale is worked from no more than this
constexpr float k_LeastWonderPower = 0.25f;
constexpr float k_MostWonderPower = 5.0f;
constexpr float k_WonderScaleDivisor = 10.0f;
/// Each turn's belief gift waits a random part of up to this share of its gap
constexpr uint32_t k_GiftRandomDivisor = 4;
} // namespace

DanceInfo artefact_rules::DanceForRadius(float radius)
{
	const float width = radius + radius;
	if (width < k_SmallDanceWidth)
	{
		return DanceInfo::DanceAroundObjectDiameterLess_7;
	}
	return width < k_MediumDanceWidth ? DanceInfo::DanceAroundObjectDiameterLess_10
	                                  : DanceInfo::DanceAroundObjectDiameterGreater_20;
}

bool artefact_rules::Glows(float worth)
{
	return worth > k_GlowingWorth;
}

bool artefact_rules::ShowsSymbol(float worth)
{
	return worth > k_SymbolWorth;
}

float artefact_rules::ImpressiveModifier(float worth)
{
	return Glows(worth) ? worth + 1.0f : 1.0f;
}

bool artefact_rules::WillImpress(float worth, bool sameTown)
{
	return Glows(worth) && !sameTown;
}

float artefact_rules::JoiningDesire(const JoiningInputs& in, float distanceModifier)
{
	if (in.dancing)
	{
		return 0.0f;
	}
	// The fuller the dance, the less wanted
	float room = 1.0f;
	if (in.maximumDancers != 0)
	{
		const float full = static_cast<float>(in.dancers.value_or(0)) / static_cast<float>(in.maximumDancers);
		room = 1.0f - std::min(full, 1.0f);
	}
	// A dance under way that has run past its length is wanted more, as the game works it out
	float lateness = 1.0f;
	if (in.dancers.has_value() && in.running && in.duration != 0)
	{
		const float share = static_cast<float>(in.sinceStart) / static_cast<float>(in.duration);
		lateness = share > 1.0f ? share : 1.0f;
	}
	return lateness * room * distanceModifier;
}

artefact_rules::Waiting artefact_rules::WaitingTurn(float distance, uint16_t waited, bool roomHere, uint16_t timeToWait)
{
	if (distance > k_WaitingReach)
	{
		return Waiting::WalkCloser;
	}
	if (waited == 0 && !roomHere)
	{
		return Waiting::MakeRoom;
	}
	return waited >= timeToWait ? Waiting::StartDance : Waiting::Wait;
}

uint32_t artefact_rules::DanceTimeLeft(uint32_t duration, uint32_t turn, uint32_t started)
{
	const auto left = static_cast<int32_t>(duration - (turn - started));
	return left > 0 ? static_cast<uint32_t>(left) : 0;
}

float artefact_rules::TownModifier(std::optional<float> beliefInOwner)
{
	if (!beliefInOwner.has_value())
	{
		return 1.0f;
	}
	const float belief = *beliefInOwner;
	if (belief <= k_TownBeliefFull)
	{
		return belief;
	}
	const float counted = k_TownBeliefFull + (belief - k_TownBeliefFull) * (1.0f / 6.0f) * 2.0f;
	return counted < k_TownBeliefCap ? counted : k_TownBeliefCap;
}

float artefact_rules::DanceGain(const WorthInputs& in)
{
	const float danced = in.duration != 0 ? static_cast<float>(in.danced) / static_cast<float>(in.duration) : 1.0f;
	const float dancers =
	    in.maximumDancers != 0 ? static_cast<float>(in.dancers) / static_cast<float>(in.maximumDancers) : 1.0f;
	return in.townModifier * in.artefactMultiplier * dancers * danced * in.artefactSpeed;
}

float artefact_rules::AddToWorth(float worth, float gain)
{
	float slowing = 1.0f / worth;
	if (slowing >= 1.0f)
	{
		slowing = 1.0f;
	}
	return slowing * k_WorthGainScale * gain + worth;
}

float artefact_rules::DancingBelief(const BeliefInputs& in)
{
	const float extra = ImpressiveModifier(in.worth) - 1.0f;
	const float worth = extra > 0.0f ? extra : 0.0f;
	float perHead = 1.0f;
	if (in.population != 0)
	{
		perHead = static_cast<float>(in.dancers) / static_cast<float>(in.population) * in.beliefMultiplier;
	}
	return perHead * in.impressiveness * worth;
}

bool artefact_rules::BeliefGiftDue(uint32_t sinceLastGift, uint32_t randomPart, uint32_t gap)
{
	return sinceLastGift + randomPart > gap;
}

float artefact_rules::SitePowerUp(float artefactSpeed, float powerUpMultiplier, uint32_t dancers, uint32_t playerPopulation)
{
	const float share = playerPopulation != 0 ? static_cast<float>(dancers) / static_cast<float>(playerPopulation) : 0.0f;
	return artefactSpeed * powerUpMultiplier * share;
}

float artefact_rules::WonderReach(float wonderDesire)
{
	const float reach = wonderDesire * k_WonderReachPerDesire;
	return reach > k_WonderReachPerDesire ? reach : k_WonderReachPerDesire;
}

float artefact_rules::WonderPower(float wonderDesire, std::span<const WonderNeighbour> inCell, uint32_t passes)
{
	const float reach = WonderReach(wonderDesire);
	float power = wonderDesire;
	for (uint32_t pass = 0; pass < passes; ++pass)
	{
		for (const auto& thing : inCell)
		{
			if (thing.distance < reach)
			{
				power += thing.artefactWorth;
				power += thing.impressiveness * k_WonderImpressivenessShare;
			}
		}
	}
	return k_LeastWonderPower > power ? k_LeastWonderPower : power;
}

float artefact_rules::WonderScale(float power)
{
	const float capped = std::min(power, k_MostWonderPower);
	return capped * (1.0f - capped / k_WonderScaleDivisor);
}

uint32_t artefact_rules::GiftRandomLimit(uint32_t gap)
{
	return gap / k_GiftRandomDivisor;
}
