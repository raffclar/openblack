/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <functional>

#include "ECS/Components/Spell.h"
#include "GameClock.h"

namespace openblack
{
struct GMagicEffectInfo;
} // namespace openblack

// The prayer power ("chants") economy of a spell: its safety level, strength, upkeep and refill. Pure functions of
// the spell's fields and a context, so the unit test can drive them without a world.

namespace openblack::magic::chants
{

/// What the chant functions read besides the Spell's own fields (Spell.cpp builds it from the ECS)
struct Context
{
	const GMagicEffectInfo* effect {nullptr};
	bool maintained {false};                       ///< IsMaintainedSpell
	bool recharged {false};                        ///< GMagicInfo.isSpellRecharged
	bool hasCreator {false};                       ///< the spell has a creator
	float tribalPower {1.0f};                      ///< the spell's tribal power
	float seedPower {1.0f};                        ///< the seed's power, 1 without a seed
	float costToMaintain {0.0f};                   ///< this spell's upkeep now
	unsigned int turnMs {game_clock::k_MsPerTurn}; ///< the turn length (Spell.cpp: game_clock::MsPerTurn())
	/// the creator maintains the spell with an amount: the chants the creator gives
	std::function<float(float amount)> maintain;
	/// a spell point (chants, perTurn): the mana path sprites (worship-site creators only)
	std::function<void(float chants, bool perTurn)> createSpellPoint;
};

/// The chants and the initial chants both become chants
void SetChants(ecs::components::Spell& spell, float chants);

/// Maintained spells -> initialChants; otherwise five seconds of upkeep
/// (cost x (1000 / turn ms) x 5), at most initialChants and at least one event (costPerEvent)
[[nodiscard]] float GetChantSafetyLevel(const ecs::components::Spell& spell, const Context& context);

/// Chants / safety clamped to 0..1 (1 / 0 for a safety <= 0), x tribal power x the
/// seed's power x strengthMultiplier; 0 without a creator
[[nodiscard]] float GetSpellStrength(const ecs::components::Spell& spell, const Context& context);

/// Takes the cost (divided by max(tribal power, 1) if divideCostsByTribalPower), then refills
/// the deficit under the safety level from the creator (all of it when forced, else at most the cost) if the spell is
/// recharged. Returns the new strength; 0 without a creator, 1 for a free spell.
float PayFor(ecs::components::Spell& spell, const Context& context, float cost, bool force);

/// One turn of upkeep; 1 when the upkeep is 0
float PayForOneTurn(ecs::components::Spell& spell, const Context& context);

/// Pays costPerEvent
float PayForOneEvent(ecs::components::Spell& spell, const Context& context);

/// The creator tops the chants up to the safety level (recharged spells); returns what it
/// gave
float Recharge(ecs::components::Spell& spell, const Context& context);

} // namespace openblack::magic::chants
