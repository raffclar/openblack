/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "VillagerFarmer.h"

#include <string>
#include <utility>

#include <entt/entity/entity.hpp>
#include <fmt/format.h>

#include "Common/GUtilsDistance.h"
#include "ECS/Components/SkeletalAnimation.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Villager.h"
#include "ECS/Fields.h"
#include "ECS/Registry.h"
#include "ECS/Systems/VillagerFieldsInterface.h"
#include "ECS/ToBeDeleted.h"
#include "ECS/Town/TownQueries.h"
#include "ECS/Villager/VillagerCore.h"
#include "ECS/Villager/VillagerHome.h"
#include "ECS/Villager/VillagerResources.h"
#include "ECS/Villager/VillagerScript.h"
#include "ECS/Villager/VillagerTrace.h"
#include "Locator.h"

// The farmers (VillagerFarmer.h)

namespace openblack::ecs::villager
{
using namespace components;
namespace tq = town_queries;

namespace
{
/// FindBestField's distance limit
constexpr float k_FieldDistanceLimit = 300.0f;
/// FarmerDigsUpCrop's capacity test: always 0 (the original never changes it)
constexpr float k_DigCapacityLimit = 0.0f;
/// AreWeThere's reach at the work point
constexpr float k_PointReach = 0.0f;
/// CARRIED_OBJECT 1 NONE: FarmerArrivesAtFarm's carried object
constexpr int32_t k_CarriedNone = 1;
/// GetFieldActivity's results
constexpr int k_ActivitySow = 1;
constexpr int k_ActivityHarvest = 2;

/// The field side, through the Locator
systems::VillagerFieldsInterface& FieldSide()
{
	return Locator::villagerFields::value();
}

Registry& Entities()
{
	return Locator::entitiesRegistry::value();
}

Villager* VillagerOf(entt::entity villager)
{
	return Entities().TryGet<Villager>(villager);
}

void TraceIf(entt::entity villager, const std::string& line)
{
	if (TraceOn(villager))
	{
		Trace(villager, line);
	}
}

/// The villager's town: a valid town entity, or null
entt::entity TownOf(entt::entity villager)
{
	const auto* v = VillagerOf(villager);
	// (guard) the original only reads the link: this stands for the missing town unlinking
	if (v == nullptr || v->town == entt::null || !ecs::IsAvailable(v->town) || !Entities().AllOf<Town>(v->town))
	{
		return entt::null;
	}
	return v->town;
}

glm::ivec2 Xz(const map_coords::MapCoords& pos)
{
	return {pos.x, pos.z};
}

glm::vec2 Metres(const map_coords::MapCoords& pos)
{
	return tq::ToMetres(Xz(pos));
}

/// The villager's field (a valid entity), else null
entt::entity FieldOf(entt::entity villager)
{
	const auto* v = VillagerOf(villager);
	if (v == nullptr || v->targetThing == entt::null || !ecs::IsAvailable(v->targetThing))
	{
		return entt::null;
	}
	return v->targetThing;
}

/// The villager's work point = point
void SetWorkPos(entt::entity villager, const map_coords::MapCoords& point)
{
	if (auto* v = VillagerOf(villager); v != nullptr)
	{
		v->workPos = point;
	}
}

map_coords::MapCoords WorkPos(entt::entity villager)
{
	const auto* v = VillagerOf(villager);
	return v != nullptr ? v->workPos : map_coords::MapCoords {};
}
} // namespace

// ---- the pure layer ----------------------------------------------------------------------------------------------

float FieldScore(float distance, float desireToBeFarmed)
{
	// The distance modifier (limit 300) times the desire, kept as a float
	return gutils::GetDistanceModifier(distance, k_FieldDistanceLimit) * desireToBeFarmed;
}

bool OverFull(int16_t capacity)
{
	return static_cast<float>(capacity) < k_DigCapacityLimit;
}

// ---- the finder and the job --------------------------------------------------------------------------------------

entt::entity FindBestField(entt::entity town, entt::entity villager, float& score)
{
	// score = 0 always; best = 0, no field
	score = 0.0f;
	float best = 0.0f;
	entt::entity bestField = entt::null;
	if (town == entt::null)
	{
		return entt::null;
	}
	auto& fieldSide = FieldSide();
	const auto me = tq::PosOf(villager);
	// The town's fields, newest first (ecs::fields::TownFields)
	for (const auto field : fieldSide.TownFields(town))
	{
		// The distance from the villager, scored with the field's desire to be farmed
		const float d = tq::GetDistanceInMetres(tq::PosOf(field), me);
		const float s = FieldScore(d, fieldSide.GetDesireToBeFarmed(field));
		// Only strictly above the best
		if (s > best)
		{
			best = s;
			bestField = field;
		}
	}
	// The best score
	score = best;
	return bestField;
}

uint32_t VillagerBecomesFarmer(entt::entity villager, entt::entity field)
{
	// No field -> the town's best field (no town or none -> 0)
	if (field == entt::null)
	{
		const auto town = TownOf(villager);
		if (town == entt::null)
		{
			return 0;
		}
		float score = 0.0f;
		field = FindBestField(town, villager, score);
		if (field == entt::null)
		{
			return 0;
		}
	}
	// Go to the field
	return SetFarmerGotoField(villager, field);
}

uint32_t SetFarmerGotoField(entt::entity villager, entt::entity field)
{
	auto& fieldSide = FieldSide();
	// The arrive position, copied before the activity is asked
	const auto arrive = fieldSide.GetArrivePos(field);
	// The field's activity: sowing and harvesting go the same way
	const int activity = fieldSide.GetFieldActivity(field);
	if (activity != k_ActivitySow && activity != k_ActivityHarvest)
	{
		villager::TraceFormatted(villager, "farm: goto {} act {} -> 0", static_cast<uint32_t>(field), activity);
		return 0;
	}
	// TOP 163: a farming exit clears a previous field first
	SetTopState(villager, VillagerStates::DecideWhatToDo);
	// The new field
	SetTargetThing(villager, field);
	// Walk to the arrive position, then 67 FARMER_ARRIVES_AT_FARM
	SetupMoveToOnFootpath(villager, field, Xz(arrive), VillagerStates::FarmerArrivesAtFarm);
	// The work point = RandomFarmPoint of the villager's field (two GameFloatRand(10), x then z).
	// (openblack, guard) the fallback to `field`: the original reads the villager's field unchecked; harmless, nothing
	// since it was set changes it (the footpath setup does not), so it is always the field
	const auto target = FieldOf(villager);
	const auto point = fieldSide.RandomFarmPoint(target != entt::null ? target : field);
	SetWorkPos(villager, point);
	villager::TraceFormatted(villager, "farm: goto {} act {} point ({}, {})", static_cast<uint32_t>(field), activity, point.x,
	                         point.z);
	return 1;
}

// ---- the states --------------------------------------------------------------------------------------------------

uint32_t FarmerArrivesAtFarm(LivingAction& action)
{
	const auto villager = Entities().ToEntity(action);
	auto& fieldSide = FieldSide();
	const auto field = FieldOf(villager);
	if (field == entt::null)
	{
		// (openblack, guard) the villager always has a field here (EnterFarming refuses without one)
		SetTopState(villager, VillagerStates::DecideWhatToDo);
		return 1;
	}
	// The field's activity
	const int activity = fieldSide.GetFieldActivity(field);
	if (activity == k_ActivitySow)
	{
		// Carries nothing (NONE): overwritten by the state change that follows either way
		if (auto* animation = Entities().TryGet<SkeletalAnimation>(villager); animation != nullptr)
		{
			animation->carriedObject = k_CarriedNone;
		}
		// At the work point
		if (AreWeThere(villager, Metres(WorkPos(villager)), k_PointReach))
		{
			// The next work point (2 draws); the clip, then 68 FARMER_PLANTS_CROP
			const auto point = fieldSide.RandomFarmPoint(field);
			SetWorkPos(villager, point);
			villager::TraceFormatted(villager, "farm 67: act 1 there -> 68, next ({}, {})", point.x, point.z);
			PlayAnimThenSetState(villager, VillagerStates::FarmerPlantsCrop);
			return 1;
		}
		// Walk to the work point, then 67 again
		TraceIf(villager, "farm 67: act 1 walk");
		SetupMoveToPos(villager, Metres(WorkPos(villager)), VillagerStates::FarmerArrivesAtFarm);
		return 1;
	}
	if (activity == k_ActivityHarvest)
	{
		// RipeFarmPoint: unripe -> 163 (no draw); ripe: 2 draws, the point discarded
		map_coords::MapCoords discarded;
		if (!fieldSide.RipeFarmPoint(field, discarded))
		{
			TraceIf(villager, "farm 67: act 2 unripe -> 163");
			SetTopState(villager, VillagerStates::DecideWhatToDo);
			return 1;
		}
		// At the work point
		if (AreWeThere(villager, Metres(WorkPos(villager)), k_PointReach))
		{
			// The next work point; the clip, then 69 FARMER_DIGS_UP_CROP
			const auto point = fieldSide.RandomFarmPoint(field);
			SetWorkPos(villager, point);
			villager::TraceFormatted(villager, "farm 67: act 2 there -> 69, next ({}, {})", point.x, point.z);
			PlayAnimThenSetState(villager, VillagerStates::FarmerDigsUpCrop);
			return 1;
		}
		TraceIf(villager, "farm 67: act 2 walk");
		SetupMoveToPos(villager, Metres(WorkPos(villager)), VillagerStates::FarmerArrivesAtFarm);
		return 1;
	}
	// Activity 0 -> TOP 163, 1
	villager::TraceFormatted(villager, "farm 67: act {} -> 163", activity);
	SetTopState(villager, VillagerStates::DecideWhatToDo);
	return 1;
}

uint32_t FarmerPlantsCrop(LivingAction& action)
{
	const auto villager = Entities().ToEntity(action);
	auto& fieldSide = FieldSide();
	const auto field = FieldOf(villager);
	// PlantCrop (the villager's position is not read) false -> 163
	if (field == entt::null || !fieldSide.PlantCrop(field))
	{
		TraceIf(villager, "farm 68: full -> 163");
		SetTopState(villager, VillagerStates::DecideWhatToDo);
		return 1;
	}
	// Still sowing ? 67 : 163
	const bool sowing = fieldSide.IsStillSowing(field);
	villager::TraceFormatted(villager, "farm 68: planted -> {}", sowing ? 67 : 163);
	SetTopState(villager, sowing ? VillagerStates::FarmerArrivesAtFarm : VillagerStates::DecideWhatToDo);
	return 1;
}

uint32_t FarmerDigsUpCrop(LivingAction& action)
{
	const auto villager = Entities().ToEntity(action);
	auto& fieldSide = FieldSide();
	const auto field = FieldOf(villager);
	if (field == entt::null)
	{
		// (openblack, guard) as FarmerArrivesAtFarm
		SetTopState(villager, VillagerStates::DecideWhatToDo);
		return 1;
	}
	// cap = GetFoodCapacity (signed 16 bits); the field gives up to cap food, its integer result
	const int16_t cap = GetFoodCapacity(villager);
	const auto got = fieldSide.RemoveFood(field, static_cast<float>(cap));
	// Anything but 0 (unsigned)
	if (got != 0)
	{
		// The farmer picks it up (as 16 bits)
		PickupFood(villager, static_cast<int16_t>(got));
		// The capacity left below 0 -> GotoStoragePitForDropOff (its result)
		const int16_t after = GetFoodCapacity(villager);
		if (OverFull(after))
		{
			villager::TraceFormatted(villager, "farm 69: cap {} got {} -> 31", cap, got);
			return GotoStoragePitForDropOff(villager);
		}
	}
	// TOP 67, 1. With capacity 0 the farmer digs for 0 again (literal)
	villager::TraceFormatted(villager, "farm 69: cap {} got {} -> 67", cap, got);
	SetTopState(villager, VillagerStates::FarmerArrivesAtFarm);
	return 1;
}

uint32_t EnterFarming(LivingAction& action, VillagerStates final, VillagerStates next)
{
	const auto villager = Entities().ToEntity(action);
	const auto* v = VillagerOf(villager);
	// No field -> 0 (refused: the villager's wrapper enters 163)
	if (v == nullptr || v->targetThing == entt::null)
	{
		villager::TraceFormatted(villager, "farm enter {}: no field -> refused", static_cast<uint32_t>(next));
		return 0;
	}
	// Coming from another entry function -> the field's farmer list gets the villager
	if (!IsStateEntryFunctionSameAs(final, next))
	{
		const auto field = v->targetThing;
		if (ecs::IsAvailable(field))
		{
			FieldSide().AddFarmer(field, villager);
		}
		villager::TraceFormatted(villager, "farm: add {}", static_cast<uint32_t>(field));
	}
	return 1;
}

uint32_t ExitFarming(LivingAction& action, VillagerStates next)
{
	const auto villager = Entities().ToEntity(action);
	// The same exit function -> 1
	if (IsStateExitFunctionSameAs(villager, next))
	{
		return 1;
	}
	const auto* v = VillagerOf(villager);
	// No field -> 1
	if (v == nullptr || v->targetThing == entt::null)
	{
		return 1;
	}
	// The field still in the global field list -> out of its farmers (which clears the villager's field); not in it (a
	// deleted field) -> the field is kept
	const auto field = v->targetThing;
	auto& fieldSide = FieldSide();
	if (fieldSide.IsField(field))
	{
		villager::TraceFormatted(villager, "farm: remove {} -> {}", static_cast<uint32_t>(field), static_cast<uint32_t>(next));
		fieldSide.RemoveFarmer(field, villager);
	}
	return 1;
}

int FieldActivityOf(entt::entity field)
{
	return FieldSide().GetFieldActivity(field);
}
} // namespace openblack::ecs::villager
