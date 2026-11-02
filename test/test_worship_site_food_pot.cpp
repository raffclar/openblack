/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <memory>
#include <optional>

#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#include "ECS/Components/Pot.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/Registry.h"
#include "Enums.h"
#include "InfoConstants.h"
#include "Locator.h"

#define LOCATOR_IMPLEMENTATIONS
#include "ECS/Systems/Implementations/ResourceStoreSystem.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;

namespace
{
/// A built worship site with its empty food pot, which takes food as a pile does
class WorshipSiteFoodPotTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		if (spdlog::get("game") == nullptr)
		{
			spdlog::create<spdlog::sinks::null_sink_st>("game");
		}
		Locator::entitiesRegistry::emplace<Registry>();
		info = std::make_unique<InfoConstants>();
		// The row of the food pot holds food and has no next pile
		auto& row = info->pot.at(static_cast<size_t>(PotInfo::StoragePitFoodPile));
		row.resourceType = ResourceType::Food;
		row.nextPotForResource = PotInfo::_COUNT;
		Locator::infoConstants::emplace(*info);

		auto& registry = Locator::entitiesRegistry::value();
		pot = registry.Create();
		registry.Assign<Pot>(pot, Pot {.amount = 0, .maxAmount = 1000, .type = PotInfo::StoragePitFoodPile});
		registry.Assign<Transform>(
		    pot, Transform {.position = glm::vec3(4.0f, 0.0f, 6.0f), .rotation = glm::mat3(1.0f), .scale = glm::vec3(1.0f)});
		site = registry.Create();
		registry.Assign<WorshipSite>(site, WorshipSite {.foodPot = pot});
		registry.Assign<Transform>(
		    site, Transform {.position = glm::vec3(0.0f), .rotation = glm::mat3(1.0f), .scale = glm::vec3(1.0f)});
	}
	void TearDown() override
	{
		Locator::infoConstants::reset();
		Locator::entitiesRegistry::reset();
	}

	[[nodiscard]] const Pot& FoodPot() const { return Locator::entitiesRegistry::value().Get<const Pot>(pot); }

	std::unique_ptr<InfoConstants> info;
	ResourceStoreSystem stores;
	entt::entity site {entt::null};
	entt::entity pot {entt::null};
};
} // namespace

// Food that a worship site takes goes into its food pot, with its poison
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(WorshipSiteFoodPotTest, FoodGivenToTheSiteGoesIntoItsPot)
{
	EXPECT_EQ(stores.AddToStore(site, ResourceType::Food, 300, std::nullopt, true, std::nullopt), 300u);
	EXPECT_EQ(FoodPot().amount, 300u);
	EXPECT_TRUE(FoodPot().poisoned);
}

// A food pot whose row has no next pile takes all the food, also more than its maximum, as any such pile
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(WorshipSiteFoodPotTest, APotWithNoNextPileTakesAllTheFood)
{
	EXPECT_EQ(stores.AddToStore(site, ResourceType::Food, 800, std::nullopt, false, std::nullopt), 800u);
	EXPECT_EQ(stores.AddToStore(site, ResourceType::Food, 800, std::nullopt, false, std::nullopt), 800u);
	EXPECT_EQ(FoodPot().amount, 1600u);
}

// A food pot whose row has a next pile takes no more food than its maximum, as any such pile
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(WorshipSiteFoodPotTest, APotWithANextPileTakesNoMoreThanItsMaximum)
{
	info->pot.at(static_cast<size_t>(PotInfo::StoragePitFoodPile)).nextPotForResource = PotInfo::WoodPile_1;
	Locator::infoConstants::reset();
	Locator::infoConstants::emplace(*info);
	EXPECT_EQ(stores.AddToStore(site, ResourceType::Food, 800, std::nullopt, false, std::nullopt), 800u);
	EXPECT_EQ(stores.AddToStore(site, ResourceType::Food, 800, std::nullopt, false, std::nullopt), 200u);
	EXPECT_EQ(FoodPot().amount, 1000u);
}

// Wood does not go into the food pot of a built site
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(WorshipSiteFoodPotTest, ABuiltSiteTakesNoWood)
{
	EXPECT_EQ(stores.AddToStore(site, ResourceType::Wood, 300, std::nullopt, false, std::nullopt), 0u);
	EXPECT_EQ(FoodPot().amount, 0u);
}

// The food pot loses its poison when the last of its food goes, and keeps it while food is left
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(WorshipSiteFoodPotTest, AnEmptiedPotLosesItsPoison)
{
	stores.AddToStore(site, ResourceType::Food, 300, std::nullopt, true, std::nullopt);
	EXPECT_EQ(stores.RemoveResource(site, ResourceType::Food, 100), 100u);
	EXPECT_TRUE(FoodPot().poisoned);
	EXPECT_EQ(stores.RemoveResource(site, ResourceType::Food, 500), 200u);
	EXPECT_EQ(FoodPot().amount, 0u);
	EXPECT_FALSE(FoodPot().poisoned);
}
