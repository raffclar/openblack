/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Magic/Core/Spell.h"
#include "Magic/Core/SpellEvent.h"
#include "SpellClasses.h"

// The plain spell: MAGIC_TYPE 0-9. What a fireball, a lightning bolt or a beam explosion does lives in its PSys rules,
// which send the events.

void openblack::magic::RegisterGeneralSpell()
{
	const SpellOps ops {.initWithPos = base::InitWithPos,
	                    .initWithObject = base::InitWithObject,
	                    .process = base::Process,
	                    .spellEvent = spell_event::SpellEvent,
	                    .costToMaintain = base::CalculateCostToMaintain,
	                    .closeDown = base::CloseDown,
	                    .toBeDeleted = nullptr,
	                    .hasEnoughChantsForRecast = base::HasEnoughChantsAndLifeForRecast,
	                    .particleType = base::GetParticleType};
	RegisterOps(SpellClass::General, ops);
}
