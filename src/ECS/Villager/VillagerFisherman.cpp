/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "VillagerFisherman.h"

#include <cmath>

#include <algorithm>
#include <array>
#include <string>
#include <utility>

#include <entt/entity/entity.hpp>
#include <fmt/format.h>

#include "Common/GUtilsDistance.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Villager.h"
#include "ECS/FishFarms.h"
#include "ECS/Registry.h"
#include "ECS/Systems/VillagerFishFarmsInterface.h"
#include "ECS/ToBeDeleted.h"
#include "ECS/Town/TownQueries.h"
#include "ECS/Villager/VillagerCore.h"
#include "ECS/Villager/VillagerHome.h"
#include "ECS/Villager/VillagerResources.h"
#include "ECS/Villager/VillagerScript.h"
#include "ECS/Villager/VillagerTrace.h"
#include "ECS/VillagerAnimations.h"
#include "InfoConstants.h"
#include "Locator.h"

// The fishermen (VillagerFisherman.h)

namespace openblack::ecs::villager
{
using namespace components;
namespace tq = town_queries;

namespace
{
/// FindBestFishFarm's distance limit
constexpr float k_FishFarmDistanceLimit = 500.0f;
/// The catch's share of MaxFoodCarried
constexpr float k_CatchShare = 0.25f;
/// The catch's multiplier for each season
constexpr std::array<float, 4> k_SeasonMultiplier = {1.0f, 0.9f, 0.7f, 0.6f};
/// The fish farm side and the catch's inputs, through the Locator
systems::VillagerFishFarmsInterface& FarmSide()
{
	return Locator::villagerFishFarms::value();
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
	// (guard) the original only reads the town field: stands for the missing town unlinking
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

/// The farm, the target thing (a valid entity), else null
entt::entity FarmOf(entt::entity villager)
{
	const auto* v = VillagerOf(villager);
	if (v == nullptr || v->targetThing == entt::null || !ecs::IsAvailable(v->targetThing))
	{
		return entt::null;
	}
	return v->targetThing;
}
} // namespace

// ---- the pure layer ----------------------------------------------------------------------------------------------

float FishFarmScore(int32_t farmScore, float distance)
{
	// The score read as a u64 and stored as a float, times GetDistanceModifier(d, 500)
	const float score = static_cast<float>(static_cast<uint64_t>(static_cast<uint32_t>(farmScore)));
	return gutils::GetDistanceModifier(distance, k_FishFarmDistanceLimit) * score;
}

float FishCatchBase(uint32_t maxFoodCarried)
{
	// MaxFoodCarried read as a u64, x 0.25, stored as a float
	return static_cast<float>(static_cast<uint64_t>(maxFoodCarried)) * k_CatchShare;
}

float FishSeasonMultiplier(uint32_t season)
{
	// GetSeason is 0..3 (Calendar.h). (openblack, guard) the clamp to 3: the original
	// indexes the 4 floats unchecked; harmless, GetSeason never gives more (only a test could)
	return k_SeasonMultiplier.at(std::min<size_t>(season, k_SeasonMultiplier.size() - 1));
}

int32_t FishCatch(float base, uint32_t season, int16_t capacity, float tribalPower)
{
	// (float)cap; v = base x mult; v > cap -> cap (v is kept when <=)
	const float cap = static_cast<float>(capacity);
	const float v = base * FishSeasonMultiplier(season);
	const float kept = v > cap ? cap : v;
	// The tribal power x v; truncated to a 32-bit int
	const float n = tribalPower * kept;
	return static_cast<int32_t>(static_cast<int64_t>(std::trunc(n)));
}

// ---- the finder and the job --------------------------------------------------------------------------------------

entt::entity FindBestFishFarm(entt::entity town, entt::entity villager, float& score)
{
	// best = 0, bestFarm = none
	float best = 0.0f;
	entt::entity bestFarm = entt::null;
	if (town != entt::null)
	{
		auto& farmSide = FarmSide();
		const auto me = tq::PosOf(villager);
		// The town's fish farms from the head (newest first)
		for (const auto farm : farmSide.TownFishFarms(town))
		{
			// The distance in metres to the farm, scored with the farm's score
			const float d = tq::GetDistanceInMetres(tq::PosOf(farm), me);
			const float s = FishFarmScore(farmSide.Score(farm), d);
			// Only strictly above
			if (s > best)
			{
				best = s;
				bestFarm = farm;
			}
		}
	}
	// *score = best
	score = best;
	return bestFarm;
}

uint32_t VillagerBecomesFisherman(entt::entity villager, entt::entity farm)
{
	SetTopState(villager, VillagerStates::DecideWhatToDo);
	// The target thing = farm
	SetTargetThing(villager, farm);
	// SetupMoveToOnFootpath(target thing, the farm's arrive point, 55)
	const auto arrive = FarmSide().GetArrivePos(farm);
	villager::TraceFormatted(villager, "fish: goto {} arrive ({}, {})", static_cast<uint32_t>(farm), arrive.x, arrive.z);
	const auto target = FarmOf(villager);
	SetupMoveToOnFootpath(villager, target != entt::null ? target : farm, Xz(arrive),
	                      VillagerStates::FishermanArrivesAtFishing);
	return 1;
}

uint32_t FishermanLookForWater(entt::entity villager)
{
	// The town, then FindBestFishFarm on it
	const auto town = TownOf(villager);
	if (town == entt::null)
	{
		return 0;
	}
	float score = 0.0f;
	const auto farm = FindBestFishFarm(town, villager, score);
	if (farm == entt::null)
	{
		return 0;
	}
	// VillagerBecomesFisherman (its result unused); 1
	VillagerBecomesFisherman(villager, farm);
	return 1;
}

bool IsAtValidFishingPos(entt::entity villager)
{
	const auto farm = FarmOf(villager);
	if (farm == entt::null)
	{
		return false;
	}
	// The same cells of x and z
	const auto me = tq::PosOf(villager);
	const auto at = tq::PosOf(farm);
	return map_coords::SignedCellOf(me.x) == map_coords::SignedCellOf(at.x) &&
	       map_coords::SignedCellOf(me.y) == map_coords::SignedCellOf(at.y);
}

// ---- the states --------------------------------------------------------------------------------------------------

uint32_t FishermanArrivesAtFishing(LivingAction& action)
{
	const auto villager = Entities().ToEntity(action);
	auto& farmSide = FarmSide();
	const auto farm = FarmOf(villager);
	if (farm == entt::null)
	{
		// (openblack, guard) the target thing is the farm while the villager is in 55 / 56
		SetTopState(villager, VillagerStates::DecideWhatToDo);
		return 1;
	}
	// !IsAtValidFishingPos -> SetupMoveToOnFootpath(farm, the farm's arrive point, 55), 1
	if (!IsAtValidFishingPos(villager))
	{
		TraceIf(villager, "fish 55: cell -> walk");
		SetupMoveToOnFootpath(villager, farm, Xz(farmSide.GetArrivePos(farm)), VillagerStates::FishermanArrivesAtFishing);
		return 1;
	}
	// My position == the farm's arrive point (x, z and the altitude: an exact compare). Kept exact: the walk ends with
	// the position snapped to the goal (PathfindingSystem), so a villager that walked to the arrive point is on it.
	// (approximate) x and z only, and the arrive point through the metres the walk's goal went through (openblack keeps
	// the villager in float metres; the altitude of a walking villager is not kept)
	const auto arrive = farmSide.GetArrivePos(farm);
	if (tq::PosOf(villager) == tq::ToMapCoords(tq::ToMetres(Xz(arrive))))
	{
		// SetupMoveToPos(the farm's FishingSpot (2 GameFloatRand(5) - 2.5, x then z), 55)
		const auto spot = farmSide.FishingSpot(farm);
		villager::TraceFormatted(villager, "fish 55: arrive spot ({}, {})", spot.x, spot.z);
		SetupMoveToPos(villager, tq::ToMetres(Xz(spot)), VillagerStates::FishermanArrivesAtFishing);
		return 1;
	}
	// PlayAnimThenSetState(56), 1
	TraceIf(villager, "fish 55: anim -> 56");
	PlayAnimThenSetState(villager, VillagerStates::Fishing);
	return 1;
}

uint32_t Fishing(LivingAction& action)
{
	const auto villager = Entities().ToEntity(action);
	// Ready for a new animation (clip 262 played once), else 1
	if (!VillagerAnimationDone(villager, action.turnsSinceStateChange))
	{
		return 1;
	}
	// The next cycle
	action.turnsSinceStateChange = 0;
	const auto farm = FarmOf(villager);
	if (farm == entt::null)
	{
		// (openblack, guard) the target thing is the farm while the villager is in 55 / 56: the original reads the
		// farm's fishermen with no test and would crash; here no draw and no catch, as in 55
		TraceIf(villager, "fish 56: no farm -> 163");
		SetTopState(villager, VillagerStates::DecideWhatToDo);
		return 1;
	}
	auto& farmSide = FarmSide();
	// GameRand(the farm's fishermen): no draw for 0 (GameRandom.h)
	const auto fishermen = farmSide.FishermanCount(farm);
	const auto roll = GameRand(fishermen);
	// Not 0 -> 1
	if (roll != 0)
	{
		villager::TraceFormatted(villager, "fish 56: roll {}/{} -> 1", roll, fishermen);
		return 1;
	}
	// The catch (GetSeason and GetFoodCapacity read as the original does: twice on the clamp path)
	const auto& info = InfoOf(villager);
	const float base = FishCatchBase(info.maxFoodCarried);
	const int16_t cap = GetFoodCapacity(villager);
	const auto season = farmSide.Season();
	const int32_t n = FishCatch(base, season, cap, farmSide.TribalPower(villager));
	// f = n read as a u64 and stored as a float; f != 0 -> PickupFood(f truncated to int16)
	const float f = static_cast<float>(static_cast<uint64_t>(static_cast<uint32_t>(n)));
	if (f != 0.0f)
	{
		const auto amount = static_cast<int16_t>(static_cast<int64_t>(std::trunc(f)));
		PickupFood(villager, amount);
	}
	const auto* v = VillagerOf(villager);
	villager::TraceFormatted(villager, "fish 56: roll {}/{} catch {} season {} -> pick {} (held {})", roll, fishermen, n,
	                         season, f, v != nullptr ? v->resourceHeld.at(0) : 0);
	// (float)GetFoodCapacity < f -> GotoStoragePitForDropOff
	if (static_cast<float>(GetFoodCapacity(villager)) < f)
	{
		TraceIf(villager, "fish 56: -> 31");
		return GotoStoragePitForDropOff(villager);
	}
	// GetFoodCapacity != 0 -> 1; else GotoStoragePitForDropOff
	if (GetFoodCapacity(villager) != 0)
	{
		return 1;
	}
	TraceIf(villager, "fish 56: full -> 31");
	return GotoStoragePitForDropOff(villager);
}

uint32_t EnterFishing(LivingAction& action, VillagerStates final, VillagerStates next)
{
	const auto villager = Entities().ToEntity(action);
	// IsStateEntryFunctionSameAs(final, next) -> 1 (55 -> 56 adds nothing)
	if (IsStateEntryFunctionSameAs(final, next))
	{
		return 1;
	}
	// No town -> 1
	if (TownOf(villager) == entt::null)
	{
		return 1;
	}
	// Not in the farm's fishermen list -> AddFisherman (which sets the target thing to the farm)
	const auto farm = FarmOf(villager);
	auto& farmSide = FarmSide();
	if (farm != entt::null && !farmSide.HasFisherman(farm, villager))
	{
		farmSide.AddFisherman(farm, villager);
		villager::TraceFormatted(villager, "fish: add {} (fishermen {})", static_cast<uint32_t>(farm),
		                         farmSide.FishermanCount(farm));
	}
	return 1;
}

uint32_t ExitFishing(LivingAction& action, VillagerStates next)
{
	const auto villager = Entities().ToEntity(action);
	// IsStateExitFunctionSameAs(next) -> 1
	if (IsStateExitFunctionSameAs(villager, next))
	{
		return 1;
	}
	// The target thing available -> RemoveFisherman(me)
	const auto* v = VillagerOf(villager);
	const auto farm = v != nullptr ? v->targetThing : entt::entity(entt::null);
	auto& farmSide = FarmSide();
	if (farm != entt::null && farmSide.IsAvailable(farm))
	{
		farmSide.RemoveFisherman(farm, villager);
		villager::TraceFormatted(villager, "fish: remove {} -> {}", static_cast<uint32_t>(farm), static_cast<uint32_t>(next));
	}
	// The target thing cleared
	SetTargetThing(villager, entt::null);
	return 1;
}
} // namespace openblack::ecs::villager
