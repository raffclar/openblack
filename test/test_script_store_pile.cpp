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

#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#include "ECS/Components/MagicPile.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/ResourcePile.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "Enums.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/ResourcePiles.h"

#define LOCATOR_IMPLEMENTATIONS
#include "ECS/Systems/Implementations/ResourceStoreSystem.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;

namespace
{
/// The pile that a script makes as a store, with no land: the land is at height 0 everywhere
class ScriptStorePileTest: public ::testing::Test
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
		auto& wood = info->pot.at(static_cast<size_t>(PotInfo::MagicWood));
		wood.resourceType = ResourceType::Wood;
		wood.potType = PotType::PileWood;
		wood.maxAmountInPot = 1000;
		auto& food = info->pot.at(static_cast<size_t>(PotInfo::MagicFood));
		food.resourceType = ResourceType::Food;
		food.potType = PotType::PileFood;
		food.maxAmountInPot = 1000;
		Locator::infoConstants::emplace(*info);
	}
	void TearDown() override
	{
		Locator::infoConstants::reset();
		Locator::entitiesRegistry::reset();
	}

	std::unique_ptr<InfoConstants> info;
	ResourceStoreSystem stores;
};
} // namespace

// A store of wood is a new magic pile of wood on the land, holding what the script asks and belonging to nobody
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(ScriptStorePileTest, AStoreOfWoodIsANewPileOfNobodysOnTheLand)
{
	const auto pile = stores.MakePile(ResourceType::Wood, glm::vec3(5.0f, 9.0f, 7.0f), 1);
	ASSERT_NE(pile, entt::entity {entt::null});
	const auto& registry = Locator::entitiesRegistry::value();
	const auto& pot = registry.Get<const Pot>(pile);
	EXPECT_EQ(pot.type, PotInfo::MagicWood);
	EXPECT_EQ(pot.amount, 1u);
	EXPECT_FALSE(pot.poisoned);
	const auto& magic = registry.Get<const MagicPile>(pile);
	EXPECT_EQ(magic.resource, ResourceType::Wood);
	EXPECT_EQ(magic.player, PlayerNames::NEUTRAL);
	const auto& transform = registry.Get<const Transform>(pile);
	EXPECT_FLOAT_EQ(transform.position.x, 5.0f);
	EXPECT_FLOAT_EQ(transform.position.y, 0.0f);
	EXPECT_FLOAT_EQ(transform.position.z, 7.0f);
	EXPECT_FLOAT_EQ(transform.scale.x, magic::piles::k_MagicWoodScale);
	EXPECT_TRUE(registry.AllOf<ResourcePile>(pile));
}

// A store of food is a new magic pile of food
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(ScriptStorePileTest, AStoreOfFoodIsANewPileOfFood)
{
	const auto pile = stores.MakePile(ResourceType::Food, glm::vec3(5.0f, 0.0f, 7.0f), 1);
	ASSERT_NE(pile, entt::entity {entt::null});
	const auto& registry = Locator::entitiesRegistry::value();
	EXPECT_EQ(registry.Get<const Pot>(pile).type, PotInfo::MagicFood);
	EXPECT_FLOAT_EQ(registry.Get<const Transform>(pile).scale.x, magic::piles::k_MagicFoodScale);
}

// The new pile does not go into a pile of the same resource at the same place, as a pour does
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(ScriptStorePileTest, TheNewPileDoesNotGoIntoAPileNearIt)
{
	const auto first = stores.MakePile(ResourceType::Wood, glm::vec3(5.0f, 0.0f, 7.0f), 1);
	const auto second = stores.MakePile(ResourceType::Wood, glm::vec3(5.0f, 0.0f, 7.0f), 1);
	ASSERT_NE(first, entt::entity {entt::null});
	ASSERT_NE(second, entt::entity {entt::null});
	EXPECT_NE(first, second);
	const auto& registry = Locator::entitiesRegistry::value();
	EXPECT_EQ(registry.Get<const Pot>(first).amount, 1u);
	EXPECT_EQ(registry.Get<const Pot>(second).amount, 1u);
}

// No other resource makes a pile
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(ScriptStorePileTest, NoOtherResourceMakesAPile)
{
	EXPECT_EQ(stores.MakePile(ResourceType::None, glm::vec3(5.0f, 0.0f, 7.0f), 1), entt::entity {entt::null});
	EXPECT_EQ(stores.MakePile(ResourceType::Wood, glm::vec3(5.0f, 0.0f, 7.0f), 0), entt::entity {entt::null});
}
