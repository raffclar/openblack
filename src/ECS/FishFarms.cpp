/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "FishFarms.h"

#include <cmath>

#include <algorithm>

#include "Common/GameRandom.h"
#include "ECS/Components/FishFarm.h"
#include "ECS/Components/Town.h"
#include "ECS/Fields.h"
#include "ECS/ObjectCreationIndex.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/Registry.h"
#include "ECS/Villager/VillagerCore.h"
#include "InfoConstants.h"
#include "Locator.h"

// Fish farms (FishFarms.h)

namespace openblack::ecs::fish_farms
{
using namespace components;

namespace
{
FishFarm* FarmOf(entt::entity entity)
{
	auto& registry = Locator::entitiesRegistry::value();
	return registry.Valid(entity) ? registry.TryGet<FishFarm>(entity) : nullptr;
}

/// entt's on_destroy<FishFarm> (Registry::Destroy from ecs::ToBeDeleted, Remove, Reset): the farm's deletion ->
/// DeleteDependants. The component is still there during the signal
void OnFishFarmDestroyed(entt::registry& /*registry*/, entt::entity entity)
{
	DeleteDependants(entity);
}

/// GFishFarmInfo: info.dat has only row 0, InfoConstants::fishFarm
const GFishFarmInfo& InfoOf()
{
	return Locator::infoConstants::value().fishFarm;
}
} // namespace

void AddFisherman(entt::entity farm, entt::entity villager)
{
	auto* f = FarmOf(farm);
	// a null villager -> nothing (the original would write through NULL)
	if (f == nullptr || villager == entt::null)
	{
		return;
	}
	// a new node at the head. The deletion listener connected first (entt's sink::connect
	// is idempotent: once per registry)
	Locator::entitiesRegistry::value().OnDestroy<FishFarm>().connect<&OnFishFarmDestroyed>();
	f->fishermen.insert(f->fishermen.begin(), villager);
	// the villager's target = this farm
	villager::SetTargetThing(villager, farm);
}

void RemoveFisherman(entt::entity farm, entt::entity villager)
{
	if (auto* f = FarmOf(farm); f != nullptr)
	{
		// every node of the villager unlinked and freed; its target untouched
		std::erase(f->fishermen, villager);
	}
}

bool HasFisherman(entt::entity farm, entt::entity villager)
{
	const auto* f = FarmOf(farm);
	return f != nullptr && std::find(f->fishermen.begin(), f->fishermen.end(), villager) != f->fishermen.end();
}

uint32_t FishermanCount(entt::entity farm)
{
	const auto* f = FarmOf(farm);
	return f != nullptr ? static_cast<uint32_t>(f->fishermen.size()) : 0u;
}

int32_t Score(entt::entity farm)
{
	const auto* f = FarmOf(farm);
	if (f == nullptr)
	{
		return 0;
	}
	return ScoreFor(f->fishermen.size(), InfoOf().maxNoFishermanPerFishFarm);
}

int32_t ScoreFor(size_t fishermen, uint32_t maxFishermen)
{
	// the fishermen (unsigned 64-bit) / maxNoFishermanPerFishFarm (read signed)
	const auto maximum = static_cast<float>(static_cast<int32_t>(maxFishermen));
	// (the 24-bit FPU: the division, subtraction and product are each rounded to float)
	const float ratio = static_cast<float>(fishermen) / maximum;
	// not below 1 (and ordered) -> 1.0; else the ratio
	const float share = ratio < 1.0f || std::isnan(ratio) ? ratio : 1.0f;
	// the farm's weight (1.0) x (1 - share), truncated (FtoL)
	constexpr float k_FishFarmWeight = 1.0f;
	const float rest = 1.0f - share;
	return map_coords::FtoL(k_FishFarmWeight * rest);
}

map_coords::MapCoords FishingSpot(entt::entity farm)
{
	auto pos = object::MapCoordsOf(farm);
	// r = Get2DRadius (5.0); h = r x 0.5
	const float r = object::Get2DRadius(farm);
	const float h = r * 0.5f;
	// x first, then z: GameFloatRand(r) - h
	const float a = game_random::GameFloatRand(r) - h;
	const float b = game_random::GameFloatRand(r) - h;
	return FishingSpotAt(pos, a, b);
}

map_coords::MapCoords FishingSpotAt(map_coords::MapCoords pos, float dx, float dz)
{
	// (pos x 10 x 2^-16 + d) x 65536 / 10, truncated toward zero, on each axis; the altitude copied
	pos.x = map_coords::ToFixedGUtils(map_coords::ToMetres(pos.x) + dx);
	pos.z = map_coords::ToFixedGUtils(map_coords::ToMetres(pos.z) + dz);
	return pos;
}

map_coords::MapCoords GetArrivePos(entt::entity farm)
{
	// the farm's position
	return object::MapCoordsOf(farm);
}

int32_t RemoveFood(entt::entity farm, int32_t amount)
{
	auto* f = FarmOf(farm);
	if (f == nullptr)
	{
		return 0;
	}
	// n above food -> what is left
	const auto n = static_cast<double>(amount);
	if (!(n > static_cast<double>(f->food)))
	{
		// food -= n (in double, stored as float); n taken
		f->food = static_cast<float>(static_cast<double>(f->food) - n);
		return amount;
	}
	// food truncated toward zero; food = 0
	const int32_t left = map_coords::FtoL(f->food);
	f->food = 0.0f;
	return left;
}

int32_t RemoveResource(entt::entity farm, ResourceType type, int32_t amount)
{
	// type 0 (FOOD) -> RemoveFood(n); else 0
	return type == ResourceType::Food ? RemoveFood(farm, amount) : 0;
}

entt::entity TownOf(entt::entity farm)
{
	const auto* f = FarmOf(farm);
	auto& registry = Locator::entitiesRegistry::value();
	return f != nullptr && f->town != entt::null && registry.Valid(f->town) ? f->town : entt::null;
}

std::optional<PlayerNames> PlayerOf(entt::entity farm)
{
	const auto town = TownOf(farm);
	if (town == entt::null)
	{
		return std::nullopt;
	}
	const auto* t = Locator::entitiesRegistry::value().TryGet<const Town>(town);
	return t != nullptr ? std::optional(t->owner) : std::nullopt;
}

std::vector<entt::entity> TownFishFarms(entt::entity town)
{
	std::vector<entt::entity> list;
	if (town == entt::null)
	{
		return list;
	}
	auto& registry = Locator::entitiesRegistry::value();
	registry.Each<const FishFarm>([&](entt::entity entity, const FishFarm& farm) {
		if (farm.town == town)
		{
			list.push_back(entity);
		}
	});
	// the head insertion at construction: newest first
	std::sort(list.begin(), list.end(),
	          [](entt::entity a, entt::entity b) { return object_index::Of(a) > object_index::Of(b); });
	return list;
}

void DisconnectDeletionListener()
{
	if (Locator::entitiesRegistry::has_value())
	{
		Locator::entitiesRegistry::value().OnDestroy<FishFarm>().disconnect<&OnFishFarmDestroyed>();
	}
}

bool IsFishFarm(entt::entity thing)
{
	return FarmOf(thing) != nullptr;
}

void DeleteDependants(entt::entity farm)
{
	// while there are fishermen: the head villager's SetTopState(163); ExitFishing unlinks it (RemoveFisherman), the
	// farm still being available (it is marked deleted later)
	for (auto* f = FarmOf(farm); f != nullptr && !f->fishermen.empty(); f = FarmOf(farm))
	{
		const auto before = f->fishermen.size();
		fields::ReleaseWorker(f->fishermen.front());
		f = FarmOf(farm);
		// (openblack guard) nothing unlinked it: stop instead of looping for ever
		if (f == nullptr || f->fishermen.size() >= before)
		{
			break;
		}
	}
	// out of the town's list and the global list: nothing to do (the components are the lists). RemoveMapObject:
	// ecs::ToBeDeleted's generic part
}
} // namespace openblack::ecs::fish_farms
