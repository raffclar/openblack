/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The miracles that need the hand report of this computer at each turn: a miracle that follows the hand of the player
// gets the hand at each turn, so the report does not wait after a send.

#define LOCATOR_IMPLEMENTATIONS

#include <algorithm>

#include "ECS/Components/Spell.h"
#include "ECS/Registry.h"
#include "Hand/HandReportRules.h"
#include "Locator.h"
#include "Magic/SpellBehaviours.h"
#include "MagicSystem.h"

using namespace openblack;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;

bool MagicSystem::SpellNeedsHandReportEachTurn() const
{
	const auto own = OwnPlayer();
	const auto& registry = Locator::entitiesRegistry::value();
	// The flock rule reads the info and the tribal power of the spell services, which are not const
	auto& self = const_cast<MagicSystem&>(*this);
	return std::ranges::any_of(_spells, [&](entt::entity entity) {
		const auto* spell = registry.TryGet<const Spell>(entity);
		if (spell == nullptr)
		{
			return false;
		}
		return hand_report::NeedsReportEachTurn({
		    .castFromHand = spell->castFromHand && spell->seed != entt::null,
		    .closedDown = spell->closedDown,
		    .humanCasting = spell->humanCasting,
		    .ownPlayer = spell->caster.player == own,
		    .flockMakesAnimals = magic::spells::MakesAnimals(self, *spell),
		});
	});
}
