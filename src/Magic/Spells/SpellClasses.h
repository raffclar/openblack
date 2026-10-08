/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

// One registration per Spell class file (Spells/*.cpp), called in this order by magic::RegisterSpellClasses
// (Core/Spell.cpp). A class nobody registered runs as the plain Spell.

namespace openblack::magic
{
void RegisterGeneralSpell();  ///< SpellGeneral.cpp: the plain Spell (fireball, lightning bolt, beam explosion)
void RegisterHealSpell();     ///< SpellHeal.cpp
void RegisterResourceSpell(); ///< SpellResource.cpp: food and wood
void RegisterForestSpell();   ///< SpellForest.cpp: the forest (NATURE)
void RegisterTeleportSpell(); ///< SpellTeleport.cpp: the teleport stones
void RegisterShieldSpell();   ///< SpellShield.cpp: the magic and physical shields
void RegisterFlockSpells();   ///< SpellFlock.cpp: the flying and the ground flock
/// SpellFlock.cpp: the flock miracles' animals' own dying (animal_ai::SetSpeciesDying); part of RegisterFlockSpells
void RegisterFlockSpeciesDying();
void RegisterWaterSpell(); ///< SpellWater.cpp: the water miracle
void RegisterStormSpell(); ///< SpellStormAndTornado.cpp: storm, lightning storm and tornado
/// SpellCreature.cpp: the creature miracles, MAGIC_TYPE 26..41
void RegisterCreatureSpell();
} // namespace openblack::magic
