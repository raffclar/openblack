/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureSpellMind.h"

#include <algorithm>
#include <vector>

#include "CreatureLearning.h"

using namespace openblack;
using openblack::creature_desires::Desire;

bool creature_spell_mind::HeldDownByCheat(Desire other, bool all)
{
	switch (other)
	{
	case Desire::Hunger:
	case Desire::Poo:
	case Desire::Tiredness:
	case Desire::Water:
		return all;
	case Desire::IdleWithPlayer:
	case Desire::RestoreHealth:
	case Desire::BeFriends:
	case Desire::ManifestState:
	case Desire::Rest:
	case Desire::PlayWithPlayer:
	case Desire::HangAroundAtHome:
	case Desire::LookAround:
		return false;
	default:
		return true;
	}
}

creature_spell_mind::Cheat creature_spell_mind::SetCheatDominant(creature_desires::Desires& desires, Desire desire,
                                                                 const CheatSetup& setup, float turnsPerSecond)
{
	desires[desire].activated = true;
	const auto turns = static_cast<uint32_t>(std::max(turnsPerSecond * setup.seconds, 0.0f));
	for (size_t i = 0; i < creature_desires::k_DesireCount; ++i)
	{
		const auto other = static_cast<Desire>(i);
		if (other != desire && HeldDownByCheat(other, setup.all))
		{
			auto& state = desires.desires.at(i);
			state.suppressedTurns = std::max(state.suppressedTurns, turns);
		}
	}
	// It is wanted above all, the others as little as any can be, and everything that drives it is full
	const auto dominate = [&desires, &setup](Desire which) {
		MakeFullyDominantOverOthers(desires, which, setup.floor);
		auto& state = desires[which];
		for (const auto& source : std::vector(state.sources))
		{
			if (std::ranges::find(k_SourcesNotMaximised, source.type) == k_SourcesNotMaximised.end())
			{
				creature_desires::SetSource(desires, source.type, 1.0f);
			}
		}
		state.activated = true;
	};
	// Compassion makes it want to make friends above all too
	if (desire == Desire::Compassion)
	{
		dominate(Desire::BeFriends);
	}
	dominate(desire);
	return {.desire = desire, .turns = 0, .seconds = static_cast<uint32_t>(std::max(setup.seconds, 0.0f))};
}

bool creature_spell_mind::StepCheat(creature_desires::Desires& desires, Cheat& cheat, float turnsPerSecond)
{
	desires[cheat.desire].suppressedTurns = 0;
	++cheat.turns;
	// The game counts whole seconds by its whole turns a second
	const auto perSecond = std::max(static_cast<uint32_t>(turnsPerSecond), 1u);
	if (cheat.turns / perSecond > cheat.seconds)
	{
		ClearCheatDominance(desires);
		return false;
	}
	return true;
}

void creature_spell_mind::ClearOnlyDesire(creature_desires::Desires& desires, std::optional<Cheat>& cheat, bool letGo)
{
	if (letGo && cheat.has_value())
	{
		ClearCheatDominance(desires);
	}
	if (cheat.has_value())
	{
		desires[cheat->desire].value = 0.0f;
	}
	cheat.reset();
}

void creature_spell_mind::ClearCheatDominance(creature_desires::Desires& desires)
{
	for (auto& state : desires.desires)
	{
		state.suppressedTurns = 0;
	}
}

void creature_spell_mind::MakeLeastDominant(creature_desires::Desires& desires, Desire desire, float factor)
{
	creature_learning::MakeLeastDominant(desires, desire, factor);
	auto& state = desires[desire];
	state.value = std::min(state.value, state.max);
}

void creature_spell_mind::MakeFullyDominantOverOthers(creature_desires::Desires& desires, Desire desire, float floor)
{
	auto& state = desires[desire];
	state.suppressedTurns = 0;
	if (!state.activated)
	{
		return;
	}
	state.value = state.max;
	for (size_t i = 0; i < creature_desires::k_DesireCount; ++i)
	{
		if (i != static_cast<size_t>(desire))
		{
			desires.desires.at(i).value = floor;
		}
	}
}
