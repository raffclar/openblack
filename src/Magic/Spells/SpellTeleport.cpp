/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <vector>

#include <spdlog/spdlog.h>

#include "ECS/Components/MagicTeleport.h"
#include "ECS/Registry.h"
#include "Locator.h"
#include "Magic/Core/Spell.h"
#include "Magic/Core/SpellEvent.h"
#include "Magic/Core/SpellWithObjects.h"
#include "Magic/Objects/MagicTeleport.h"
#include "SpellClasses.h"

// SpellTeleport (a spell with objects): MAGIC_TYPE 12. Its one object is the MagicTeleport stone; the spell has no
// particle type (the vortex belongs to the stone), so the base InitWithPos sends it SpellEvent 11. Its other virtuals
// are those of a spell with objects and of the base Spell: Process, CloseDown, ToBeDeleted, the seed's turn and
// SpellEvent.

namespace
{
using namespace openblack;
using namespace openblack::magic;

/// The stone (teleport::Create(pos, spell)) joins the object list, then the base InitWithPos
int InitWithPos(entt::entity spell, const glm::vec3& position, SpellCastData* castData, const psys::ProcessInfo& info)
{
	if (const auto stone = teleport::Create(position, spell); stone != entt::null)
	{
		spell_objects::Add(spell, stone);
	}
	return base::InitWithPos(spell, position, castData, info);
}

/// CloseDown as a spell with objects: the base CloseDown, then (its objects die on close down) every object not already
/// going gets SetDying (for the stone, ToBeDeleted at once). Not named CloseDown: inside magic::RegisterTeleportSpell
/// that name is magic::CloseDown (the dispatching one) -> recursion.
void TeleportCloseDown(entt::entity spell)
{
	base::CloseDown(spell);
	for (const auto object : std::vector<entt::entity>(spell_objects::Objects(spell)))
	{
		teleport::ToBeDeleted(object);
	}
	spell_objects::ProcessObjectsAndRemoveDeleted(spell);
}

/// The class part of ToBeDeleted: CloseDown, the object list emptied (the objects themselves are not deleted here), then
/// the base ToBeDeleted (DeleteSpell runs it next)
void ToBeDeleted(entt::entity spell)
{
	TeleportCloseDown(spell);
	for (const auto object : std::vector<entt::entity>(spell_objects::Objects(spell)))
	{
		spell_objects::Remove(spell, object);
	}
}
} // namespace

void openblack::magic::RegisterTeleportSpell()
{
	const SpellOps ops {.initWithPos = InitWithPos,
	                    .initWithObject = base::InitWithObject,
	                    .process = spell_objects::Process,
	                    .spellEvent = spell_event::SpellEvent,
	                    .costToMaintain = base::CalculateCostToMaintain,
	                    .closeDown = TeleportCloseDown,
	                    .toBeDeleted = ToBeDeleted,
	                    .hasEnoughChantsForRecast = base::HasEnoughChantsAndLifeForRecast,
	                    .particleType = base::GetParticleType};
	RegisterOps(SpellClass::Teleport, ops);
}
