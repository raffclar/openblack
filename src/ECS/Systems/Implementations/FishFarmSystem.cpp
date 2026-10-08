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

#include <glm/geometric.hpp>

#include "Common/GameRandom.h"
#include "ECS/Components/FishFarm.h"
#include "ECS/Registry.h"
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
	};
}

uint32_t FishFarmSystem::TakeFish(entt::entity farm, uint32_t wanted)
{
	auto* data = Locator::entitiesRegistry::value().TryGet<FishFarm>(farm);
	return data != nullptr ? fish_farm::Take(data->fish, wanted) : 0;
}

void FishFarmSystem::Reset()
{
	_scare.reset();
}
