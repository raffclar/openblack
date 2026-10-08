/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "TownStores.h"

#include <algorithm>
#include <optional>

#include "Common/GUtilsAngle.h"
#include "ECS/AnimalAI.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/Town.h"
#include "ECS/MapCells.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/ObjectResources.h"
#include "ECS/PotResource.h"
#include "ECS/Registry.h"
#include "ECS/ToBeDeleted.h"
#include "ECS/Town/AbodeQueries.h"
#include "ECS/Town/TownBelief.h"
#include "ECS/Town/TownQueries.h"
#include "GameClock.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/Objects/MagicPiles.h"

namespace openblack::ecs::town_stores
{
TemporaryStore GetTemporaryResourceStorePotOrPos(entt::entity town, const map_coords::MapCoords& from, ResourceType type)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* t = town != entt::null && registry.Valid(town) ? registry.TryGet<components::Town>(town) : nullptr;
	if (t == nullptr || (type != ResourceType::Food && type != ResourceType::Wood))
	{
		return {entt::null, from};
	}
	auto& pot = t->temporaryPots.at(static_cast<size_t>(type));
	// the pot of that type, if any and available
	if (pot == entt::null || !ecs::IsAvailable(pot))
	{
		// the congregation point + GetPosFromAngle(0, WOOD ? 5 : 0). (approximate) town_queries::GetCongregationPos
		// gives x / z only: the cached altitude is dropped, the pile stands on the land anyway
		const auto congregation = town_queries::GetCongregationPos(town);
		map_coords::MapCoords pos {congregation.x, congregation.y, 0.0f};
		pos += gutils::GetPosFromAngle(0.0f, type == ResourceType::Wood ? k_WoodPotOffset : 0.0f);
		// FindClearArea(pos, pos, 45, 1.5, 2, the MultiMapFixed filter, none): result and start are the same point. The
		// bool it returns is not tested
		glm::ivec2 clear {pos.x, pos.z};
		clear = town_queries::FindClearArea(clear, k_ClearAreaA, k_ClearAreaB, k_ClearAreaRadius,
		                                    map_cells::IsMultiCellStaticClass, entt::null)
		            .value_or(clear);
		pos.x = clear.x;
		pos.z = clear.y;
		// a pot with the pot info FOOD ? 10 : 9: for those two infos a magic food / magic wood pile (no player, amount 0;
		// the town is not passed on), then the creation calls; kept as the town's pot of that type
		pot = magic::objects::CreateMagicResourcePile(map_coords::ToWorld(pos), std::nullopt, type, 0, true);
		if (pot == entt::null)
		{
			// (openblack) no pile could be made (PotArchetype has no info for it): the point itself
			return {entt::null, pos};
		}
	}
	// the point = the pot's nearest edge to `from`
	return {pot, object::GetNearestEdgeToPos(pot, from)};
}

namespace
{
components::Town* TownComponent(entt::entity town)
{
	auto& registry = Locator::entitiesRegistry::value();
	return town != entt::null && registry.Valid(town) ? registry.TryGet<components::Town>(town) : nullptr;
}

/// The bounds: player < 8 (unsigned), 0 <= type < 2
bool InBounds(PlayerNames player, ResourceType type)
{
	return static_cast<uint32_t>(player) < 8 && (type == ResourceType::Food || type == ResourceType::Wood);
}
} // namespace

float GetGameTurnResourceLastRemovedModifier(entt::entity town, PlayerNames player, ResourceType type)
{
	auto* t = TownComponent(town);
	if (t == nullptr || !InBounds(player, type))
	{
		return 0.0f;
	}
	const uint32_t last = t->resourceLastRemovedTurn.at(static_cast<size_t>(player)).at(static_cast<size_t>(type));
	if (last == 0)
	{
		return 1.0f;
	}
	// the unsigned turn difference / maxGameturnsForBeliefAfterRemovingFromStoragePit; min 1; r * r * r. (approximate) in
	// float, the original keeps more digits until the store
	const auto max = Locator::infoConstants::value().town.maxGameturnsForBeliefAfterRemovingFromStoragePit;
	const float r = std::min(static_cast<float>(game_clock::Turn() - last) / static_cast<float>(max), 1.0f);
	return r * r * r;
}

void SetGameTurnResourceLastRemoved(entt::entity town, PlayerNames player, ResourceType type)
{
	if (auto* t = TownComponent(town); t != nullptr && InBounds(player, type))
	{
		t->resourceLastRemovedTurn.at(static_cast<size_t>(player)).at(static_cast<size_t>(type)) = game_clock::Turn();
	}
}

void AddToBelief(entt::entity town, PlayerNames player, float f, entt::entity thing, bool draw, int guidanceAlignment)
{
	auto* t = TownComponent(town);
	// (openblack) the player bound is a guard of the array, the original indexes with the player number unchecked
	if (t == nullptr || static_cast<uint32_t>(player) >= 8)
	{
		return;
	}
	const auto n = static_cast<size_t>(player); // the player number
	t->belief.pending.at(n) += f;
	t->belief.recent.at(n) += f;
	if (f != 0.0f)
	{
		t->belief.lastAddedTurn.at(n) = game_clock::Turn();
	}
	// with a thing, DrawBelief(f, thing, P) when draw, then the belief sound
	if (thing != entt::null)
	{
		if (draw)
		{
			town_belief::DrawBelief(f, thing, player);
		}
		// the guidance's belief sound: audio::guidance::PlayBeliefRemark(t->belief.belief, ...). (pending) the distance from
		// the interface's position is not available here
	}
	static_cast<void>(guidanceAlignment);
}

void SetStoragePit(entt::entity town, entt::entity pit)
{
	auto* t = TownComponent(town);
	if (t == nullptr)
	{
		return;
	}
	t->storagePit = pit;
	// each temporary pot (FOOD, WOOD): available and holding its resource -> its reaction (the villagers carry it to
	// the pit); else ToBeDeleted; the slot = 0
	for (size_t type = 0; type < t->temporaryPots.size(); ++type)
	{
		auto& pot = t->temporaryPots.at(type);
		if (pot != entt::null && ecs::IsAvailable(pot))
		{
			if (object_resources::GetResource(pot, static_cast<ResourceType>(type)) != 0)
			{
				animal_ai::SetupPotReaction(pot);
			}
			else
			{
				// a food pile's deletion closes its speed-up visual; then the common deletion
				// (ecs::ToBeDeleted)
				pot_resource::SetSpeedUp(pot, false);
				ecs::animal_ai::RemovePotReaction(pot); // a pot's deletion removes its reaction
				ecs::ToBeDeleted(pot);
			}
		}
		pot = entt::null;
	}
}

void ProcessTemporaryPots(entt::entity town)
{
	auto& registry = Locator::entitiesRegistry::value();
	// the two slots
	for (size_t type = 0; type < 2; ++type)
	{
		auto* t = TownComponent(town);
		if (t == nullptr)
		{
			return;
		}
		const auto pot = t->temporaryPots.at(type);
		if (pot == entt::null)
		{
			continue;
		}
		// not available -> the slot = 0
		if (!ecs::IsAvailable(pot))
		{
			t->temporaryPots.at(type) = entt::null;
			continue;
		}
		// the amount != 0 -> it stays
		const auto* p = registry.TryGet<const components::Pot>(pot);
		if (p == nullptr || p->amount != 0)
		{
			continue;
		}
		// the town's storage pit (none -> it stays) and whether it is functional
		if (const auto pit = town_queries::GetStoragePit(town); pit == entt::null || !abode_queries::IsFunctional(pit))
		{
			continue;
		}
		// ToBeDeleted (a food pile closes its speed-up visual, then the pot its reaction), then the slot = 0
		pot_resource::SetSpeedUp(pot, false);
		ecs::animal_ai::RemovePotReaction(pot); // a pot's deletion removes its reaction
		ecs::ToBeDeleted(pot);
		if ((t = TownComponent(town)) != nullptr)
		{
			t->temporaryPots.at(type) = entt::null;
		}
	}
}
} // namespace openblack::ecs::town_stores
