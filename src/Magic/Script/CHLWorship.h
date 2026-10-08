/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

// The CHL natives of the miracles' supply (worship sites, prayer power, town spells, dispensers, one-shots).
// CHLApi.cpp forwards to them; each pops its arguments in the handler's order.

namespace openblack::magic::script
{
void GameSetMana();              ///< 355 GAME_SET_MANA (site or a thing with one, chants)
void GetMana();                  ///< 422 GET_MANA (site)
void SetMagicInObject();         ///< 386 SET_MAGIC_IN_OBJECT (town, magic, on)
void SetCanBuildWorshipsite();   ///< 376 SET_CAN_BUILD_WORSHIPSITE (town or citadel, on)
void IsSpellCharging();          ///< 330 IS_SPELL_CHARGING (player)
void IsThatSpellCharging();      ///< 331 IS_THAT_SPELL_CHARGING (player, magic)
void ClearPlayerSpellCharging(); ///< 423 CLEAR_PLAYER_SPELL_CHARGING (player)
void GetSpellIconInTemple();     ///< 453 GET_SPELL_ICON_IN_TEMPLE (citadel, magic)
void GetTownWorshipDeaths();     ///< 410 GET_TOWN_WORSHIP_DEATHS (town)
void SetMagicProperties();       ///< 356 SET_MAGIC_PROPERTIES (dispenser, magic, seconds)

/// SET_ACTIVE / SET_TIMER_TIME on a spell dispenser (their other objects stay CHLApi's): true when
/// the object was one
bool SetDispenserActive(entt::entity object, bool active);
bool SetDispenserTimerTime(entt::entity object, float seconds);

/// CREATE / CREATE_WITH_ANGLE_AND_SCALE of the SCRIPT_OBJECT_TYPEs ONE_SHOT_SPELL (30: a one-off seed at the position),
/// ONE_SHOT_SPELL_IN_HAND (31: a one-off seed in the local player's hand) and SPELL_DISPENSER (36: a dispenser of
/// that abode info, no town, angle, scale); entt::null
entt::entity CreateOneShotSpell(uint32_t seed, const glm::vec3& position);
entt::entity CreateOneShotSpellInHand(uint32_t seed);
entt::entity CreateSpellDispenser(uint32_t abodeInfo, const glm::vec3& position, float yAngle, float scale);
} // namespace openblack::magic::script
