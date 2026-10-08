/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Chants.h"

#include <cstdint>

#include "InfoConstants.h"

using namespace openblack;
using namespace openblack::magic;
using openblack::ecs::components::Spell;

void chants::SetChants(Spell& spell, float chants)
{
	spell.chants = chants;
	spell.initialChants = chants;
}

float chants::GetChantSafetyLevel(const Spell& spell, const Context& context)
{
	if (context.maintained)
	{
		return spell.initialChants;
	}
	// the integer turns a second (1000 / turn ms) as a float, then x 5 x the cost
	const auto turnsPerSecond = static_cast<float>(static_cast<uint64_t>(1000u / context.turnMs));
	float level = turnsPerSecond * 5.0f * context.costToMaintain;
	if (!(level < spell.initialChants))
	{
		level = spell.initialChants;
	}
	const float costPerEvent = context.effect != nullptr ? context.effect->costPerEvent : 0.0f;
	if (!(level > costPerEvent))
	{
		level = costPerEvent;
	}
	return level;
}

float chants::GetSpellStrength(const Spell& spell, const Context& context)
{
	if (!context.hasCreator)
	{
		return 0.0f;
	}
	float ratio = 0.0f;
	const float safety = GetChantSafetyLevel(spell, context);
	if (safety > 0.0f)
	{
		ratio = spell.chants / safety;
		if (1.0f < ratio)
		{
			ratio = 1.0f;
		}
		else if (ratio <= 0.0f)
		{
			ratio = 0.0f;
		}
	}
	else
	{
		ratio = spell.chants > 0.0f ? 1.0f : 0.0f;
	}
	return ratio * context.tribalPower * context.seedPower * spell.strengthMultiplier;
}

float chants::PayFor(Spell& spell, const Context& context, float cost, bool force)
{
	if (!context.hasCreator)
	{
		return 0.0f;
	}
	if (spell.free)
	{
		return 1.0f;
	}
	// divideCostsByTribalPower == 1
	if (context.effect != nullptr && context.effect->divideCostsByTribalPower == 1)
	{
		const float power = context.tribalPower > 1.0f ? context.tribalPower : 1.0f;
		cost /= power;
	}
	spell.chants -= cost;
	const float deficit = GetChantSafetyLevel(spell, context) - spell.chants;
	if (deficit > 0.0f && context.recharged && context.maintain)
	{
		const float asked = force ? deficit : (deficit < cost ? deficit : cost);
		spell.chants += context.maintain(asked);
	}
	return GetSpellStrength(spell, context);
}

float chants::PayForOneTurn(Spell& spell, const Context& context)
{
	const float cost = context.costToMaintain;
	if (cost == 0.0f)
	{
		return 1.0f;
	}
	if (context.createSpellPoint)
	{
		context.createSpellPoint(cost, true);
	}
	return PayFor(spell, context, cost, false);
}

float chants::PayForOneEvent(Spell& spell, const Context& context)
{
	if (!context.hasCreator)
	{
		return 0.0f; // no creator: 0, and no mana path point
	}
	const float cost = context.effect != nullptr ? context.effect->costPerEvent : 0.0f;
	const float strength = PayFor(spell, context, cost, false);
	if (context.createSpellPoint)
	{
		context.createSpellPoint(cost, false);
	}
	return strength;
}

float chants::Recharge(Spell& spell, const Context& context)
{
	const float deficit = GetChantSafetyLevel(spell, context) - spell.chants;
	if (!(deficit > 0.0f) || !context.recharged || !context.maintain)
	{
		return 0.0f;
	}
	const float given = context.maintain(deficit);
	spell.chants += given;
	return given;
}
