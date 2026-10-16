/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <memory>

#include <glm/mat3x3.hpp>
#include <gtest/gtest.h>

#include "ECS/Components/Ball.h"
#include "ECS/Components/FishFarm.h"
#include "ECS/Components/StreetLantern.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/WorldObjects.h"
#include "InfoConstants.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::components;

namespace
{
/// The world's things with tables of their own, and no models
class EffectReceivers: public ::testing::Test
{
protected:
	void SetUp() override
	{
		Locator::entitiesRegistry::emplace<ecs::Registry>();
		_info = std::make_unique<InfoConstants>();
		Locator::infoConstants::emplace(*_info);
	}
	void TearDown() override
	{
		Locator::infoConstants::reset();
		Locator::entitiesRegistry::reset();
	}

	static entt::entity Made()
	{
		auto& registry = Locator::entitiesRegistry::value();
		const auto entity = registry.Create();
		registry.Assign<Transform>(entity, glm::vec3(100.0f, 0.0f, 100.0f), glm::mat3(1.0f), glm::vec3(1.0f));
		return entity;
	}

	std::unique_ptr<InfoConstants> _info;
};
} // namespace

TEST_F(EffectReceivers, AStreetLanternTakesEffectsByItsRowOfTheStatics)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto town = Made();
	registry.Assign<StreetLantern>(town, StreetLantern {.country = false, .info = MobileStaticInfo::StreetLantern});
	const auto country = Made();
	registry.Assign<StreetLantern>(country, StreetLantern {.country = true, .info = MobileStaticInfo::CountryLantern});
	EXPECT_EQ(ecs::world_objects::EffectInfoOf(town),
	          &Locator::infoConstants::value().mobileStatic.at(static_cast<size_t>(MobileStaticInfo::StreetLantern)));
	EXPECT_EQ(ecs::world_objects::EffectInfoOf(country),
	          &Locator::infoConstants::value().mobileStatic.at(static_cast<size_t>(MobileStaticInfo::CountryLantern)));
	// A lantern is not crushed and is the neutral player's
	EXPECT_FALSE(ecs::world_objects::CanBeCrushed(town));
	EXPECT_EQ(ecs::world_objects::PlayerOf(town), PlayerNames::NEUTRAL);
}

TEST_F(EffectReceivers, AFishFarmTakesEffectsByItsOwnRowAndReachesFiveMetres)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto farm = Made();
	registry.Assign<FishFarm>(farm);
	EXPECT_EQ(ecs::world_objects::EffectInfoOf(farm), &Locator::infoConstants::value().fishFarm);
	const auto size = ecs::world_objects::SizeOf(farm);
	EXPECT_FLOAT_EQ(size.radius, 5.0f);
	EXPECT_FLOAT_EQ(size.height, 0.0f);
	EXPECT_TRUE(ecs::world_objects::CanBeCrushed(farm));
}

TEST_F(EffectReceivers, AFishFarmIsItsTownsPlayersOrNoOnes)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto farm = Made();
	registry.Assign<FishFarm>(farm);
	EXPECT_FALSE(ecs::world_objects::PlayerOf(farm).has_value());
	const auto town = registry.Create();
	registry.Assign<Town>(town, 3u, PlayerNames::PLAYER_ONE);
	registry.Get<FishFarm>(farm).town = town;
	EXPECT_EQ(ecs::world_objects::PlayerOf(farm), PlayerNames::PLAYER_ONE);
}

TEST_F(EffectReceivers, ABallTakesEffectsByTheBallsRowAndIsNotCrushed)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto ball = Made();
	registry.Assign<Ball>(ball);
	EXPECT_EQ(ecs::world_objects::EffectInfoOf(ball), &Locator::infoConstants::value().ball);
	EXPECT_FALSE(ecs::world_objects::CanBeCrushed(ball));
	EXPECT_EQ(ecs::world_objects::PlayerOf(ball), PlayerNames::NEUTRAL);
}
