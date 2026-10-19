/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <array>
#include <memory>

#include <glm/mat3x3.hpp>
#include <gtest/gtest.h>

#include "ECS/Components/Ball.h"
#include "ECS/Components/FishFarm.h"
#include "ECS/Components/StreetLantern.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Vortex.h"
#include "ECS/Registry.h"
#include "ECS/StreetLanternRules.h"
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

TEST(StreetLanternRules, NoLanternWithinHalfAMetreOfAStatic)
{
	constexpr glm::vec3 k_At {100.0f, 0.0f, 100.0f};
	EXPECT_TRUE(ecs::street_lanterns::Refused(k_At, std::array {glm::vec3(100.4f, 0.0f, 100.0f)}));
	EXPECT_FALSE(ecs::street_lanterns::Refused(k_At, std::array {glm::vec3(100.6f, 0.0f, 100.0f)}));
	// Only the distance across the land counts, not the height
	EXPECT_TRUE(ecs::street_lanterns::Refused(k_At, std::array {glm::vec3(100.0f, 50.0f, 100.3f)}));
	// One near thing among far ones is enough
	EXPECT_TRUE(
	    ecs::street_lanterns::Refused(k_At, std::array {glm::vec3(108.0f, 0.0f, 100.0f), glm::vec3(99.8f, 0.0f, 99.8f)}));
	EXPECT_FALSE(ecs::street_lanterns::Refused(k_At, {}));
}

TEST_F(EffectReceivers, OnlyTheHelpReadsTheRowOfTheVortex)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto vortex = Made();
	registry.Assign<Vortex>(vortex);
	_info->mobileStatic.at(static_cast<size_t>(MobileStaticInfo::Vortex)).helpStartEnum = static_cast<HelpText>(2917);
	Locator::infoConstants::emplace(*_info);
	const auto& row = Locator::infoConstants::value().mobileStatic.at(static_cast<size_t>(MobileStaticInfo::Vortex));
	EXPECT_EQ(ecs::world_objects::HelpInfoOf(vortex), &row);
	EXPECT_EQ(ecs::world_objects::HelpInfoOf(vortex)->helpStartEnum, static_cast<HelpText>(2917));
	// Miracles, heat and the other users of the rows do not know the vortex
	EXPECT_EQ(ecs::world_objects::InfoOf(vortex), nullptr);
	EXPECT_EQ(ecs::world_objects::EffectInfoOf(vortex), nullptr);
	// A thing with a row of its own gives that row to the help too
	const auto ball = Made();
	registry.Assign<Ball>(ball);
	EXPECT_EQ(ecs::world_objects::HelpInfoOf(ball), &Locator::infoConstants::value().ball);
	EXPECT_EQ(ecs::world_objects::HelpInfoOf(Made()), nullptr);
}
