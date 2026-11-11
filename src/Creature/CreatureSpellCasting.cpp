/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureSpellCasting.h"

#include <algorithm>

#include "Common/X87.h"

using namespace openblack::creature_spell_casting;

float openblack::creature_spell_casting::ChantsToEnergy(float chants, const Body& body, const Rates& rates)
{
	return chants / ((body.size * rates.sizeFactor + body.strength + 1.0f) * rates.chantsPerEnergy);
}

float openblack::creature_spell_casting::MostChants(const Body& body, const Rates& rates)
{
	return (body.size + body.strength + 1.0f) * (body.energy - rates.energyFloor) * rates.chantsPerEnergy;
}

float openblack::creature_spell_casting::MaintainSpell(Body& body, const Rates& rates, float amount, float energyShare)
{
	if (!(amount > 0.0f))
	{
		return amount;
	}
	const float most = MostChants(body, rates);
	if (!(most > 0.0f))
	{
		return 0.0f;
	}
	const float paid = amount < most ? amount : most;
	const float tired = ClampLikeGame(ChantsToEnergy(paid, body, rates), 0.0f, k_MostTiredByPaying);
	const float energy = body.energy - (tired * energyShare);
	// A size below 1, or not a number, keeps the energy at 1 or less
	const float mostEnergy = x87::LessOrUnordered(body.size, 1.0f) ? 1.0f : body.size;
	body.energy = ClampLikeGame(energy, 0.0f, mostEnergy);
	body.exhaustion += tired;
	return paid;
}

bool openblack::creature_spell_casting::CanCast(const Body& body, const Rates& rates, float costToCreate)
{
	return k_TooExhaustedToCast - ChantsToEnergy(costToCreate, body, rates) > body.exhaustion;
}

float openblack::creature_spell_casting::ClampLikeGame(float value, float least, float most)
{
	if (x87::LessOrUnordered(value, least))
	{
		return least;
	}
	return value > most ? most : value;
}

float openblack::creature_spell_casting::StaminaCost(float costToCreate)
{
	return ClampLikeGame(costToCreate / k_ChantsPerStamina, 0.0f, 1.0f);
}

std::optional<float> openblack::creature_spell_casting::PayStamina(float stamina, float cost)
{
	// It can't pay when it has less than the cost, or when a value is not a number
	if (x87::LessOrUnordered(stamina, cost))
	{
		return std::nullopt;
	}
	return ClampLikeGame(stamina - cost, 0.0f, 1.0f);
}

bool openblack::creature_spell_casting::MayTry(float seen, float needed)
{
	// A share that is not a number (no sightings needed and none seen) does not pass
	const float share = seen / needed;
	return share > 1.0f || share >= k_ShareToTry;
}

bool openblack::creature_spell_casting::TrySucceeds(float seen, float needed)
{
	// Fewer than two sightings needed, or not a number, counts as one
	const float less = needed - 1.0f;
	const float share = seen / (x87::LessOrUnordered(less, 1.0f) ? 1.0f : less);
	return share > 1.0f || !(share < k_ShareToSucceed);
}

namespace
{
/// A fire miracle is as big as the caster's height over 10, at most 1. A height that is not a number gives 1.
float FireMagnitude(float casterHeight)
{
	const float magnitude = casterHeight / openblack::creature_spell_casting::k_FireMagnitudeHeight;
	return openblack::x87::LessOrUnordered(1.0f, magnitude) ? 1.0f : magnitude;
}
} // namespace

float openblack::creature_spell_casting::CastMagnitude(float targetSize, std::optional<float> targetCreatureScale,
                                                       bool fireSeed, float casterHeight)
{
	if (fireSeed)
	{
		return FireMagnitude(casterHeight);
	}
	if (targetCreatureScale.has_value())
	{
		return *targetCreatureScale * k_HeightOfSizeOne * k_MagnitudeOverSize;
	}
	return targetSize * k_MagnitudeOverSize;
}

float openblack::creature_spell_casting::CastMagnitudeAtPlace(bool fireSeed, float casterHeight)
{
	return fireSeed ? FireMagnitude(casterHeight) : k_PlaceMagnitude;
}
