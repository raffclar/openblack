/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Graveyard.h"

#include "ECS/Abodes.h"
#include "ECS/Components/Town.h"
#include "ECS/Registry.h"
#include "ECS/Town/AbodeQueries.h"
#include "ECS/Town/AbodeVillagers.h"
#include "ECS/Town/TownStats.h"
#include "Locator.h"

namespace openblack::ecs::graveyard
{
namespace
{
components::Town* TownComponent(entt::entity town)
{
	auto& registry = Locator::entitiesRegistry::value();
	return town != entt::null && registry.Valid(town) ? registry.TryGet<components::Town>(town) : nullptr;
}
} // namespace

entt::entity GetGraveyard(entt::entity town)
{
	const auto* t = TownComponent(town);
	return t != nullptr ? t->graveyard : entt::null;
}

void SetGraveyard(entt::entity town, entt::entity graveyard)
{
	auto* t = TownComponent(town);
	if (t == nullptr)
	{
		return;
	}
	// No graveyard yet -> set; else only a null argument is written
	if (t->graveyard == entt::null || graveyard == entt::null)
	{
		t->graveyard = graveyard;
	}
}

void AddDead(entt::entity graveyard)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* g =
	    graveyard != entt::null && registry.Valid(graveyard) ? registry.TryGet<components::Graveyard>(graveyard) : nullptr;
	// A town and functional
	if (g == nullptr || abode_villagers::TownOf(graveyard) == entt::null || !abode_queries::IsFunctional(graveyard))
	{
		return;
	}
	// The count as a float, only below 50
	if (!(static_cast<float>(g->dead) < k_MaxDead))
	{
		return;
	}
	// One more dead; stage = truncated (n + 1) x 0.18; stage 0 with someone buried -> 1
	++g->dead;
	auto stage = static_cast<int32_t>(static_cast<float>(g->dead) * k_GravesPerDead);
	if (stage == 0 && g->dead != 0)
	{
		stage = 1;
	}
	// The 3D object keeps the stage in 3 bits
	g->gravesStage = static_cast<uint8_t>(stage & 7);
}

void MakeFunctional(entt::entity graveyard)
{
	// The abode part is the caller's (abodes::MakeFunctional runs the class part after it)
	const auto town = abode_villagers::TownOf(graveyard);
	// With a town that has no graveyard -> SetGraveyard(this)
	if (town != entt::null && GetGraveyard(town) == entt::null)
	{
		SetGraveyard(town, graveyard);
	}
	// Literal: one dead counted
	AddDead(graveyard);
}

void DeleteDependants(entt::entity graveyard)
{
	const auto town = abode_villagers::TownOf(graveyard);
	// A town whose graveyard is this one
	if (town == entt::null)
	{
		return;
	}
	entt::entity found = entt::null;
	if (GetGraveyard(town) == graveyard)
	{
		// The first other functional abode whose ABODE_TYPE has bit 2 or bit 9
		for (const auto abode : town_stats::AbodesOf(town))
		{
			const auto type = abodes::TypeOf(abode);
			const auto bits = type.has_value() ? static_cast<uint32_t>(*type) : 0u;
			if ((bits & 0x204u) != 0 && abode_queries::IsFunctional(abode) && abode != graveyard)
			{
				found = abode;
				break;
			}
		}
	}
	// SetGraveyard(null) then SetGraveyard(found): null when the town's graveyard was another one or nothing was found
	SetGraveyard(town, entt::null);
	SetGraveyard(town, found);
	// The abode's own DeleteDependants is the caller's
}
} // namespace openblack::ecs::graveyard
