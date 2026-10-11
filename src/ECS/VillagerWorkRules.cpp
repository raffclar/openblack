/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "VillagerWorkRules.h"

#include <cmath>

#include <algorithm>

#include "Common/GUtilsAngle.h"
#include "Common/GUtilsDistance.h"

using namespace openblack;
using namespace openblack::ecs;

namespace
{
/// Out from a place towards a villager by a distance, on the ground
glm::vec2 TowardsVillager(glm::vec2 from, glm::vec2 villager, float distance)
{
	const float angle = gutils::Get3DAngleFromXZ(from, villager);
	auto coords = map_coords::FromMetres(from);
	gutils::AddDistanceFromAngle(coords, angle, distance);
	return map_coords::ToMetres(coords);
}

/// Keeps a carried amount's sum from dividing by nothing
constexpr float k_Tiny = 1e-5f;
} // namespace

glm::vec2 villager_work::TreeWorkingPosition(glm::vec2 tree, glm::vec2 villager, float villagerRadius)
{
	return TowardsVillager(tree, villager, villagerRadius + k_TreeWorkingGap);
}

glm::vec2 villager_work::EdgeWorkingPosition(glm::vec2 thing, glm::vec2 villager, float thingRadius, float villagerRadius)
{
	return TowardsVillager(thing, villager, villagerRadius + thingRadius);
}

glm::vec2 villager_work::BigForestArrivePosition(glm::vec2 bigForest, glm::vec2 villager, float radius)
{
	return TowardsVillager(bigForest, villager, radius * k_BigForestArriveShare);
}

bool villager_work::Touching(float distanceToPoint, float villagerRadius, float speedInMetres)
{
	return distanceToPoint - villagerRadius < speedInMetres;
}

bool villager_work::AreWeThere(map_coords::MapCoords walker, map_coords::MapCoords point, float wholeSpeed, float slack)
{
	const auto dx = static_cast<float>(walker.x - point.x);
	const auto dz = static_cast<float>(walker.z - point.z);
	const float reach = wholeSpeed + slack;
	return dx * dx + dz * dz < reach * reach;
}

uint32_t villager_work::TurnsToWaitForMovingWood(uint32_t millisecondsPerTurn)
{
	const uint32_t turnsPerSecond = millisecondsPerTurn != 0 ? 1000u / millisecondsPerTurn : 0u;
	return static_cast<uint32_t>(static_cast<float>(turnsPerSecond) * 100.0f);
}

bool villager_work::ReactsToWoodAgainAtOnce(VillagerStates finalState)
{
	switch (finalState)
	{
	case VillagerStates::ForesterMoveToForest:
	case VillagerStates::ForesterGotoForest:
	case VillagerStates::ForesterArrivesAtForest:
	case VillagerStates::ForesterChopsTree:
	case VillagerStates::ForesterFinishedForestering:
		return true;
	default:
		return false;
	}
}

bool villager_work::IsInterestedInWood(const WoodInterestInputs& in)
{
	if (!in.available || in.room == 0 || !in.hasTown || !in.healthy)
	{
		return false;
	}
	// A fallen trunk is taken up while what it holds is less than its room left (as the game compares them)
	if (in.fallenTree && static_cast<int32_t>(in.held) < in.room && in.free)
	{
		return true;
	}
	if (in.siteHasEnough)
	{
		return false;
	}
	if ((in.siteWood || in.disciplePicksUpWood) && in.free)
	{
		return true;
	}
	const float share = static_cast<float>(in.room) / static_cast<float>(in.maxCarried);
	const float interest = in.interactDesire + in.townWoodDesire * share * in.stateInterest * in.distanceModifier;
	return in.townTrigger < interest;
}

villager_work::FallenWoodTake villager_work::TakeFromFallenTree(uint32_t wood, uint32_t wanted, uint32_t woodValue,
                                                                float multiplier)
{
	if (wood <= wanted)
	{
		return {.taken = wood, .scale = std::nullopt};
	}
	const float scale = static_cast<float>(wood - wanted) / (static_cast<float>(woodValue) * multiplier);
	return {.taken = wanted, .scale = scale};
}

std::optional<size_t> villager_work::NearestTownForest(std::span<const ForestCandidate> forests, float reach)
{
	std::optional<size_t> best;
	std::optional<size_t> bestScenic;
	float bestDistance = reach;
	float bestScenicDistance = reach;
	for (size_t i = 0; i < forests.size(); ++i)
	{
		const auto& forest = forests[i];
		if (forest.scenic)
		{
			if (forest.distance < bestScenicDistance)
			{
				bestScenicDistance = forest.distance;
				bestScenic = i;
			}
		}
		else if (forest.distance < bestDistance)
		{
			bestDistance = forest.distance;
			best = i;
		}
	}
	return best.has_value() ? best : bestScenic;
}

std::optional<size_t> villager_work::CentreTree(std::optional<float> nearestGrown, std::optional<float> nearestGrowing)
{
	if (nearestGrown.has_value() && (!nearestGrowing.has_value() || *nearestGrown < *nearestGrowing))
	{
		return 0;
	}
	if (nearestGrowing.has_value())
	{
		return 1;
	}
	return std::nullopt;
}

villager_work::GotWood villager_work::AfterGettingWood(int16_t woodHeld, bool workingDisciple, bool siteStillWantsIt)
{
	if (woodHeld == 0 || workingDisciple)
	{
		return GotWood::Decide;
	}
	return siteStillWantsIt ? GotWood::BuildingSite : GotWood::LookForSiteOrStore;
}

bool villager_work::IsAtValidFishingPos(map_coords::MapCoords fisherman, map_coords::MapCoords farm)
{
	return map_coords::CellOf(fisherman.x) == map_coords::CellOf(farm.x) &&
	       map_coords::CellOf(fisherman.z) == map_coords::CellOf(farm.z);
}

villager_work::FishingArrival villager_work::ArriveAtFishing(map_coords::MapCoords fisherman, map_coords::MapCoords farm)
{
	if (!IsAtValidFishingPos(fisherman, farm))
	{
		return FishingArrival::WalkAgain;
	}
	if (fisherman.x == farm.x && fisherman.z == farm.z)
	{
		return FishingArrival::MoveToSpot;
	}
	return FishingArrival::StartFishing;
}

villager_work::FoodJobChoice villager_work::ChooseFoodJob(std::span<const FoodJobCandidate> jobs, int32_t room,
                                                          int32_t maxCarried, float storeNearness)
{
	// The jobs are listed most first, a new one going before the first that scores strictly less
	const FoodJobCandidate* best = nullptr;
	for (const auto& job : jobs)
	{
		if (best == nullptr || job.score > best->score)
		{
			best = &job;
		}
	}
	const float fullness = 1.0f - (static_cast<float>(room) + k_Tiny) / (static_cast<float>(maxCarried) + k_Tiny);
	const float dropOff = storeNearness * fullness;
	const float bestScore = best != nullptr ? best->score : 0.0f;
	if (dropOff > bestScore)
	{
		return {.dropOff = true, .job = std::nullopt};
	}
	if (best == nullptr || !best->found)
	{
		return {};
	}
	return {.dropOff = false, .job = best->job};
}

float villager_work::DesireToBeFarmed(const FieldDesireInputs& in)
{
	const float crowd = std::min(static_cast<float>(in.farmers) / static_cast<float>(in.maxFarmers), 1.0f);
	const float free = 1.0f - crowd;
	const float sown = static_cast<float>(in.timesSown) / in.timesToSow;
	if (sown < 1.0f)
	{
		return (1.0f - sown) * free * free * free;
	}
	if (in.age >= in.ageGrowth)
	{
		return in.age >= in.ageRecolt ? free : 0.0f;
	}
	return 0.0f;
}
