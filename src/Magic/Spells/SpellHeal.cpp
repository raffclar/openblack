/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <spdlog/spdlog.h>

#include "Magic/CastRules.h"
#include "Magic/Core/Spell.h"
#include "Magic/Core/SpellEvent.h"
#include "SpellClasses.h"

// SpellHeal: MAGIC_TYPE 10-11, no state of its own.
// The healing itself is the PSys (UR_HealSpellChakra sends a type 5 event per target).

namespace
{
using namespace openblack;
using namespace openblack::magic;

/// The base cast, then FindHealTargets gives the PSys its targets (the count is not checked: a script cast with nobody
/// to heal still plays)
int InitWithPos(entt::entity spell, const glm::vec3& position, SpellCastData* castData, const psys::ProcessInfo& info)
{
	const int result = base::InitWithPos(spell, position, castData, info);
	if (result == 1)
	{
		const int targets = cast_rules::FindHealTargets(position, spell);
		if (TraceEnabled())
		{
			SPDLOG_LOGGER_INFO(spdlog::get("game"), "Spell trace: spell {} SpellHeal::InitWithPos: FindTargets {} targets",
			                   static_cast<uint32_t>(spell), targets);
		}
	}
	return result;
}
} // namespace

void openblack::magic::RegisterHealSpell()
{
	const SpellOps ops {.initWithPos = InitWithPos,
	                    .initWithObject = base::InitWithObject,
	                    .process = base::Process,
	                    .spellEvent = spell_event::SpellEvent,
	                    .costToMaintain = base::CalculateCostToMaintain,
	                    .closeDown = base::CloseDown,
	                    .toBeDeleted = nullptr,
	                    .hasEnoughChantsForRecast = base::HasEnoughChantsAndLifeForRecast,
	                    .particleType = base::GetParticleType};
	RegisterOps(SpellClass::Heal, ops);
}
