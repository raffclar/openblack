/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "FishFarmSystem.h"

#include <algorithm>
#include <vector>

#include <glm/geometric.hpp>
#include <glm/gtx/vec_swizzle.hpp>

#include "3D/MapCoords.h"
#include "Common/GameRandom.h"
#include "ECS/Components/FishFarm.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "ECS/Systems/WeatherSystemInterface.h"
#include "InfoConstants.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::systems;
using namespace openblack::ecs::components;

void FishFarmSystem::ProcessTurn(uint32_t turn)
{
	const auto type = GetType();
	const float full = fish_farm::Full(type);
	Locator::entitiesRegistry::value().Each<FishFarm>([turn, &type, full](FishFarm& farm) {
		farm.fish = fish_farm::Grow(farm.fish, turn, type);
		if (farm.shoal.has_value() && full > 0.0f)
		{
			farm.shoal->fullness = farm.fish / full;
		}
	});
}

void FishFarmSystem::Update(float seconds, glm::vec3 camera)
{
	auto& random = Locator::gameRandom::value();
	const fish_shoal::Random draw = [&random](float a, float b) { return random.CrtRandom(a, b); };
	Locator::entitiesRegistry::value().Each<FishFarm>([&](FishFarm& farm) {
		farm.shownAlpha.reset();
		if (!farm.shoal.has_value())
		{
			return;
		}
		const auto offset = camera - farm.shoal->centre;
		farm.shownAlpha = fish_shoal::AlphaAt(glm::dot(offset, offset));
		// Too far off to be drawn, it doesn't swim either
		if (farm.shownAlpha.has_value())
		{
			fish_shoal::Step(*farm.shoal, seconds, _scare, draw);
		}
	});
	// A scare reaches the shoals of one frame only
	_scare.reset();
}

void FishFarmSystem::Scare(glm::vec3 point)
{
	_scare = point;
}

std::optional<entt::entity> FishFarmSystem::FarmWithFishAt(glm::vec2 point) const
{
	std::optional<entt::entity> found;
	Locator::entitiesRegistry::value().Each<const FishFarm>([&](entt::entity entity, const FishFarm& farm) {
		if (!found.has_value() && farm.shoal.has_value() && fish_shoal::HasShownFishNear(*farm.shoal, point))
		{
			found = entity;
		}
	});
	return found;
}

fish_farm::Type FishFarmSystem::GetType() const
{
	const auto& info = Locator::infoConstants::value().fishFarm;
	return {
	    .foodValue = info.foodValue,
	    .foodType = static_cast<uint32_t>(info.foodType),
	    .turnsPerFish = info.numGameTurnsAfterWhichFoodIsIncreased,
	    .maxFishermen = info.maxNoFishermanPerFishFarm,
	};
}

uint32_t FishFarmSystem::TakeFish(entt::entity farm, uint32_t wanted)
{
	auto* data = Locator::entitiesRegistry::value().TryGet<FishFarm>(farm);
	return data != nullptr ? fish_farm::Take(data->fish, wanted) : 0;
}

float FishFarmSystem::FishLeft(entt::entity farm) const
{
	const auto* data = Locator::entitiesRegistry::value().TryGet<const FishFarm>(farm);
	return data != nullptr ? data->fish : 0.0f;
}

std::optional<entt::entity> FishFarmSystem::ClosestFarm(glm::vec3 point, float maxDistance) const
{
	std::optional<entt::entity> closest;
	float best = maxDistance;
	Locator::entitiesRegistry::value().Each<const FishFarm, const Transform>(
	    [&](entt::entity entity, const FishFarm&, const Transform& transform) {
		    const float distance = glm::distance(glm::xz(transform.position), glm::xz(point));
		    if (distance < best)
		    {
			    best = distance;
			    closest = entity;
		    }
	    });
	return closest;
}

std::optional<entt::entity> FishFarmSystem::BestFarmFor(entt::entity town, glm::vec3 fisherman) const
{
	std::vector<entt::entity> farms;
	std::vector<fish_farm::Candidate> candidates;
	Locator::entitiesRegistry::value().Each<const FishFarm, const Transform>(
	    [&](entt::entity entity, const FishFarm& farm, const Transform& transform) {
		    if (farm.town != town)
		    {
			    return;
		    }
		    farms.push_back(entity);
		    candidates.push_back({.distance = glm::distance(glm::xz(transform.position), glm::xz(fisherman)),
		                          .fishermen = farm.fishermen.size()});
	    });
	const auto best = fish_farm::BestFarm(candidates, GetType().maxFishermen);
	return best.has_value() ? std::optional(farms.at(*best)) : std::nullopt;
}

void FishFarmSystem::AddFisherman(entt::entity farm, entt::entity villager)
{
	auto* data = Locator::entitiesRegistry::value().TryGet<FishFarm>(farm);
	if (data != nullptr && std::ranges::find(data->fishermen, villager) == data->fishermen.end())
	{
		data->fishermen.push_back(villager);
	}
}

void FishFarmSystem::RemoveFisherman(entt::entity farm, entt::entity villager)
{
	if (auto* data = Locator::entitiesRegistry::value().TryGet<FishFarm>(farm))
	{
		std::erase(data->fishermen, villager);
	}
}

glm::vec3 FishFarmSystem::FishingSpot(entt::entity farm)
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* transform = registry.TryGet<const Transform>(farm);
	if (transform == nullptr)
	{
		return glm::vec3(0.0f);
	}
	const auto* data = registry.TryGet<const FishFarm>(farm);
	if (data == nullptr)
	{
		return transform->position;
	}
	// Within a square the farm's width across, drawn along x and then z
	auto& random = Locator::gameRandom::value();
	const float x = random.GameFloatRand(fish_farm::k_FishingSpread) - fish_farm::k_FishingSpread * 0.5f;
	const float z = random.GameFloatRand(fish_farm::k_FishingSpread) - fish_farm::k_FishingSpread * 0.5f;
	const auto spot = fish_farm::FishingSpot(data->place, {x, z});
	return {map_coords::ToMetres(spot.x), transform->position.y, map_coords::ToMetres(spot.y)};
}

std::optional<int32_t> FishFarmSystem::Fish(entt::entity farm, uint32_t capacity, uint32_t held, float tribalPower)
{
	const auto* data = Locator::entitiesRegistry::value().TryGet<const FishFarm>(farm);
	if (data == nullptr)
	{
		return std::nullopt;
	}
	// The more fishermen a farm has, the less often each gets a bite
	if (Locator::gameRandom::value().GameRand(static_cast<uint32_t>(data->fishermen.size())) != 0)
	{
		return std::nullopt;
	}
	const auto season = Locator::weatherSystem::value().GetSeason(Locator::time::value().GetTurn());
	return fish_farm::Catch(capacity, held, season, tribalPower);
}

void FishFarmSystem::Reset()
{
	_scare.reset();
}
