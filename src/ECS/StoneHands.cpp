/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "StoneHands.h"

#include "ECS/Components/Abode.h"
#include "ECS/Components/Fire.h"
#include "ECS/Registry.h"
#include "ECS/Systems/ObjectDrawListSystemInterface.h"
#include "Magic/SpellIcons.h"

namespace openblack::ecs::stone_hands
{

bool CentreShowsHands(const Registry& registry, const systems::ObjectDrawListSystemInterface* drawList, entt::entity centre)
{
	if (!registry.Valid(centre))
	{
		return false;
	}
	const auto* abode = registry.TryGet<const components::Abode>(centre);
	const auto& towns = registry.Context().towns;
	const auto town = abode != nullptr ? towns.find(abode->townId) : towns.end();
	const bool inTown = town != towns.end() && registry.Valid(town->second);
	const bool drawn = drawList == nullptr || drawList->Drawn(registry, centre);
	const auto* life = registry.TryGet<const components::ObjectLife>(centre);
	return magic::spell_icons::CentreShowsHands(inTown, drawn, life != nullptr ? life->life : 1.0f);
}

} // namespace openblack::ecs::stone_hands
