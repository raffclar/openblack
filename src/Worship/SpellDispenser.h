/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

#include "ECS/Components/SpellDispenser.h"
#include "Enums.h"

// The miracle dispensers: an Abode that makes a one-shot orb of its magic above itself, and another one `period` turns
// after the last was taken. Land 3 and 5 have CREATE_SPELL_DISPENSER in their land script; the challenge script's
// GiveSpellDispenserReward (CREATE_WITH_ANGLE_AND_SCALE(SPELL_DISPENSER), SET_MAGIC_PROPERTIES, SET_ACTIVE,
// SET_TIMER_TIME) only runs from the test land's SpellDispensors, so Land 1 and 2 have none.

namespace openblack::worship::dispenser
{
/// The abode (AbodeArchetype), the period (the abode info's timeEachMobileObjectTakesToProduce, 300; 0 -> inactive)
/// and magic 0, and its effect (particle type 0x90) on the land under it. townId -1: the town nearest (the first
/// player's first town in the original).
entt::entity Create(const glm::vec3& position, AbodeInfo type, int townId, float yAngle, float scale);

/// Active; activating makes an orb at once (CreateOneOffSpellSeed)
void SetActive(entt::entity dispenser, bool active);
/// SET_MAGIC_PROPERTIES (the magic, seconds): seconds > 0 -> period = seconds x turns per second, else the abode
/// info's; a period of 0 deactivates it
void SetMagicProperties(entt::entity dispenser, MagicType magic, float seconds);
/// SET_TIMER_TIME on a dispenser: period = seconds x turns per second when above 0
void SetTimerTime(entt::entity dispenser, float seconds);
/// The magic and the period in turns (the map script's CREATE_SPELL_DISPENSER, case 90: the float is turns)
void SetMagicAndPeriod(entt::entity dispenser, MagicType magic, uint32_t periodTurns);

/// Its own part after the abode's update, from the town's abode pass (every processAbodeEvery turns), so the period
/// counts these calls. While its orb exists and touches it (within 0.001) it waits; else, functional (active), with a
/// magic, built and repaired: every `period` calls an orb
void Process(entt::entity dispenser);
/// What a dispenser's turn does
enum class TurnStep
{
	Wait,    ///< its orb is still on it
	OrbGone, ///< its orb was taken: the tick starts again from 0
	Idle,    ///< inactive or without a magic
	Count,   ///< one more tick
	MakeOrb, ///< the tick reached the period: an orb
};
/// The turn's step on the dispenser's own state (no registry): the tick and the orb it made
[[nodiscard]] TurnStep StepTurn(ecs::components::SpellDispenser& component, bool orbStillThere);

/// The magic's seed (the first seed of its magic type) and level, at the dispenser's position + 1.2 x its height, a
/// one-off seed (pos, seed, pu, 1), spot visual 9
entt::entity CreateOneOffSpellSeed(entt::entity dispenser);

/// A seed applied to a dispenser: a seed not cast yet given to it becomes a one-shot there (its chants are lost) and
/// the seed goes; spot visual 0x1B. 1 when it was.
bool ApplySeed(entt::entity dispenser, entt::entity seed);
[[nodiscard]] bool IsDispenser(entt::entity entity);
} // namespace openblack::worship::dispenser
