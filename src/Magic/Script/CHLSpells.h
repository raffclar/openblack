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

#include "ECS/Components/Spell.h"
#include "Enums.h"

// CHL natives of the spells, called from CHLApi.cpp. They pop and push the VM stack themselves.

namespace openblack::magic::script
{
/// Casts a spell for a script (pos, magic, from, creator, check, radius, time, curl, dir): creator none = the neutral
/// player; with `check` the class's CanCast at pos first; castData {radius, initialChants, time, -1}; the process info
/// {hand = from, camera forward = pos - from, dir, power 1, curl, enabled}. Positions are world points (the script's
/// vectors; the conversion to MapCoords keeps x, z). The spell, or entt::null.
entt::entity CastSpellAtPos(const glm::vec3& position, MagicType magic, const glm::vec3& from,
                            ecs::components::SpellCreator creator, bool check, float radius, float time, float curl,
                            const glm::vec3& direction);

/// 195 SPELL_AT_THING
void SpellAtThing();
/// 196 SPELL_AT_POS, through CastSpellAtPos
void SpellAtPos();
/// 227 SPELL_AT_POINT
void SpellAtPoint();
/// 244 SET_PLAYER_MAGIC
void SetPlayerMagic();
/// 245 HAS_PLAYER_MAGIC
void HasPlayerMagic();
/// 293 PLAYER_SPELL_CAST_TIME
void PlayerSpellCastTime();
/// 294 PLAYER_SPELL_LAST_CAST
void PlayerSpellLastCast();
/// 295 GET_LAST_SPELL_CAST_POS
void GetLastSpellCastPos();
/// 403 GET_MANA_FOR_SPELL
void GetManaForSpell();
} // namespace openblack::magic::script
