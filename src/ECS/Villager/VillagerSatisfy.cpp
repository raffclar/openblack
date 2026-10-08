/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "VillagerSatisfy.h"

#include <array>
#include <string>

#include <entt/entity/entity.hpp>
#include <fmt/format.h>

#include "Common/GUtilsDistance.h"
#include "ECS/Components/Flock.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Villager.h"
#include "ECS/Registry.h"
#include "ECS/ToBeDeleted.h"
#include "ECS/Town/TownQueries.h"
#include "ECS/Villager/VillagerCore.h"
#include "ECS/Villager/VillagerFarmer.h"
#include "ECS/Villager/VillagerFisherman.h"
#include "ECS/Villager/VillagerResources.h"
#include "InfoConstants.h"
#include "Locator.h"

// The villager's CheckSatisfy functions (VillagerSatisfy.h)

namespace openblack::ecs::villager
{
using namespace components;

namespace
{
void TraceIf(entt::entity villager, const std::string& line)
{
	if (TraceOn(villager))
	{
		Trace(villager, line);
	}
}

/// One node {score, object, k} of CheckSatisfyFoodDesire's list
struct FoodJob
{
	float score {0.0f};
	entt::entity object {entt::null};
	uint32_t k {0};
};
} // namespace

entt::entity FindFlockWithoutShepherd([[maybe_unused]] entt::entity town, [[maybe_unused]] entt::entity villager, float& score)
{
	// The town's flocks without a shepherd, the strictly best GetDistanceModifier(distance, 300) above 0.
	// (pending) the shepherd: VillagerBecomesShepherd is not ported, so every flock counts as taken and none is found,
	// as in the original once each flock has its shepherd; the food desire then tries the fish farms and the fields
	score = 0.0f;
	return entt::null;
}

uint32_t VillagerBecomesShepherd([[maybe_unused]] entt::entity villager, [[maybe_unused]] entt::entity flock)
{
	// TODO: the shepherds. Neutral 0 until then (the original
	// would start shepherding and return 1)
	return 0;
}

uint32_t CheckSatisfyFoodDesire(entt::entity villager)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* v = registry.TryGet<const Villager>(villager);
	// The villager's town. (openblack, guard) the desires only reach villagers of a town
	const auto* t =
	    v != nullptr && v->town != entt::null && ecs::IsAvailable(v->town) ? registry.TryGet<const Town>(v->town) : nullptr;
	if (t == nullptr)
	{
		return 0;
	}
	// For k 0..2 a node {score, object, k}: k 0 the fish farms, k 1 the fields, k 2 the flocks without a shepherd,
	// each the strictly best above 0 or {0, null}; inserted before the first node it beats strictly: high to low, ties
	// keep k's order.
	std::array<FoodJob, 3> found {};
	found[0].k = 0;
	found[0].object = FindBestFishFarm(v->town, villager, found[0].score);
	found[1].k = 1;
	found[1].object = FindBestField(v->town, villager, found[1].score);
	found[2].k = 2;
	found[2].object = FindFlockWithoutShepherd(v->town, villager, found[2].score);
	std::array<const FoodJob*, 3> list {};
	size_t count = 0;
	for (const auto& job : found)
	{
		size_t at = 0;
		while (at < count && !(job.score > list.at(at)->score))
		{
			++at;
		}
		for (size_t i = count; i > at; --i)
		{
			list.at(i) = list.at(i - 1);
		}
		list.at(at) = &job;
		++count;
	}
	const auto& head = *list.at(0);
	// frac = (float)(1 - (GetFoodCapacity + 1e-5) / (MaxFoodCarried + 1e-5));
	// GetDistanceModifier(GetDistanceInMetres(GetResourceDropoffPos(FOOD), me), 500) x frac
	// (DropOffScore; asking for the drop-off point may make the town's temporary food pot: a side effect, literal)
	const auto& info = InfoOf(villager);
	const auto pos = GetResourceDropoffPos(villager, ResourceType::Food);
	const auto distance = town_queries::GetDistanceInMetres(pos, town_queries::PosOf(villager));
	const float drop = DropOffScore(v->resourceHeld.at(0), info.maxFoodCarried, distance);
	const auto line = [&](const char* to) {
		return fmt::format("food-desire: farm {} {:.9f} field {} {:.9f} flock {} {:.9f} drop {:.9f} -> {}",
		                   static_cast<uint32_t>(found[0].object), found[0].score, static_cast<uint32_t>(found[1].object),
		                   found[1].score, static_cast<uint32_t>(found[2].object), found[2].score, drop, to);
	};
	// drop > head.score -> GotoStoragePitForDropOff (its result), after the list is freed
	if (drop > head.score)
	{
		TraceIf(villager, line("31"));
		return GotoStoragePitForDropOff(villager);
	}
	// No head object -> 0
	if (head.object == entt::null)
	{
		TraceIf(villager, line("0"));
		return 0;
	}
	// switch head.k; r == 1 -> 1, else 0
	uint32_t r = 0;
	switch (head.k)
	{
	case 0:
		TraceIf(villager, line("fisher"));
		r = VillagerBecomesFisherman(villager, head.object);
		break;
	case 1:
		TraceIf(villager, line("farmer"));
		r = VillagerBecomesFarmer(villager, head.object);
		break;
	default:
		// (approximate, no shepherds) 0: the field / farm behind it is not tried (the original tries only the head too)
		TraceIf(villager, line("shepherd TODO"));
		r = VillagerBecomesShepherd(villager, head.object);
		break;
	}
	return r == 1 ? 1 : 0;
}

// CheckSatisfyWoodDesire: VillagerForester.cpp

uint32_t CheckSatisfyPlaytimeDesire([[maybe_unused]] entt::entity villager)
{
	return 0;
}

// CheckSatisfyAbodesDesire / CheckSatisfyCivicBuildings: VillagerBuild.cpp

uint32_t CheckSatisfySupplyWorship([[maybe_unused]] entt::entity villager)
{
	// TODO: -> GotoStoragePitForWorshipSupplies. Neutral (the desire 7 is always 0 plus
	// the boosts)
	return 0;
}

// CheckSatisfyToBuild / CheckSatisfyToRepair: VillagerBuild.cpp

uint32_t CheckSatisfySupplyWorkshop([[maybe_unused]] entt::entity villager)
{
	// TODO(workshops): the town's best workshop. Neutral
	return 0;
}

uint32_t CheckSatisfyRelaxation([[maybe_unused]] entt::entity villager)
{
	// The best activity desire of the football, the player's creature and the town's artifacts.
	// TODO(football/creature/artifacts): openblack has none of them, so the best is 0 and the answer is 0 (literal)
	return 0;
}
} // namespace openblack::ecs::villager
