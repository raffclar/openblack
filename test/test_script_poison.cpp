/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <array>

#include <gtest/gtest.h>

#include "ECS/Components/Animal.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/Poisoned.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/StoragePit.h"
#include "ECS/Components/Villager.h"
#include "ECS/Registry.h"
#include "ECS/ScriptPoison.h"

using namespace openblack;
using namespace openblack::ecs;

namespace
{
entt::entity MakePile(Registry& registry, uint32_t amount, bool poisoned = false)
{
	const auto pile = registry.Create();
	registry.Assign<components::Pot>(pile, components::Pot {.amount = amount, .maxAmount = 5000u, .poisoned = poisoned});
	return pile;
}

entt::entity MakePit(Registry& registry, entt::entity food, std::array<entt::entity, 5> wood)
{
	const auto pit = registry.Create();
	registry.Assign<components::StoragePit>(pit, components::StoragePit {.woodPiles = wood, .foodPile = food});
	return pit;
}

constexpr std::array<entt::entity, 5> k_NoWood {entt::null, entt::null, entt::null, entt::null, entt::null};
} // namespace

TEST(ScriptPoison, ALivingThingTakesTheLowestBitOfTheValue)
{
	Registry registry;
	const auto villager = registry.Create();
	registry.Assign<components::Villager>(villager);
	const auto animal = registry.Create();
	registry.Assign<components::Animal>(animal);

	script_poison::SetPoisoned(registry, villager, 1u);
	script_poison::SetPoisoned(registry, animal, 3u);
	EXPECT_TRUE(script_poison::IsPoisoned(registry, villager));
	EXPECT_TRUE(script_poison::IsPoisoned(registry, animal));

	// An even value cures, and curing a thing that is well does nothing
	script_poison::SetPoisoned(registry, villager, 0u);
	script_poison::SetPoisoned(registry, animal, 2u);
	script_poison::SetPoisoned(registry, animal, 0u);
	EXPECT_FALSE(script_poison::IsPoisoned(registry, villager));
	EXPECT_FALSE(script_poison::IsPoisoned(registry, animal));
}

TEST(ScriptPoison, AStoreIsPoisonedInItsFoodOnly)
{
	Registry registry;
	const auto food = MakePile(registry, 2000);
	const auto wood = MakePile(registry, 500);
	const auto pit = MakePit(registry, food, {wood, entt::null, entt::null, entt::null, entt::null});

	script_poison::SetPoisoned(registry, pit, 1u);
	EXPECT_TRUE(registry.Get<components::Pot>(food).poisoned);
	EXPECT_FALSE(registry.Get<components::Pot>(wood).poisoned);
	EXPECT_TRUE(script_poison::IsPoisoned(registry, pit));
}

TEST(ScriptPoison, AnyValueButNoughtPoisonsAStore)
{
	Registry registry;
	const auto food = MakePile(registry, 10);
	const auto pit = MakePit(registry, food, k_NoWood);
	script_poison::SetPoisoned(registry, pit, 2u);
	EXPECT_TRUE(registry.Get<components::Pot>(food).poisoned);
}

TEST(ScriptPoison, AScriptCuresAStoreWithAValueOfNought)
{
	Registry registry;
	const auto food = MakePile(registry, 2000, true);
	const auto pit = MakePit(registry, food, k_NoWood);
	script_poison::SetPoisoned(registry, pit, 0u);
	EXPECT_FALSE(registry.Get<components::Pot>(food).poisoned);
	EXPECT_FALSE(script_poison::IsPoisoned(registry, pit));
}

TEST(ScriptPoison, AStoreWithNoFoodPileIsLeftAlone)
{
	Registry registry;
	const auto wood = MakePile(registry, 500);
	const auto pit = MakePit(registry, entt::null, {wood, entt::null, entt::null, entt::null, entt::null});
	script_poison::SetPoisoned(registry, pit, 1u);
	EXPECT_FALSE(registry.Get<components::Pot>(wood).poisoned);
	EXPECT_FALSE(script_poison::IsPoisoned(registry, pit));
}

TEST(ScriptPoison, AStoreIsPoisonedByPoisonedWoodToo)
{
	Registry registry;
	const auto food = MakePile(registry, 2000);
	const auto wood = MakePile(registry, 500, true);
	const auto pit = MakePit(registry, food, {entt::null, entt::null, wood, entt::null, entt::null});
	EXPECT_TRUE(script_poison::IsPoisoned(registry, pit));
}

TEST(ScriptPoison, APileOrHandfulCanBePoisonedAndCured)
{
	Registry registry;
	const auto handful = MakePile(registry, 100);
	script_poison::SetPoisoned(registry, handful, 1u);
	EXPECT_TRUE(script_poison::IsPoisoned(registry, handful));
	script_poison::SetPoisoned(registry, handful, 0u);
	EXPECT_FALSE(script_poison::IsPoisoned(registry, handful));
}

TEST(ScriptPoison, AToadstoolIsPoisonedAndOtherThingsAreNot)
{
	Registry registry;
	const auto toadstool = registry.Create();
	registry.Assign<components::MobileObject>(toadstool, components::MobileObject {MobileObjectInfo::Toadstool});
	const auto mushroom = registry.Create();
	registry.Assign<components::MobileObject>(mushroom, components::MobileObject {MobileObjectInfo::MagicMushroom});
	const auto bare = registry.Create();

	EXPECT_TRUE(script_poison::IsPoisoned(registry, toadstool));
	EXPECT_FALSE(script_poison::IsPoisoned(registry, mushroom));
	EXPECT_FALSE(script_poison::IsPoisoned(registry, bare));
	// A script's value changes none of these
	script_poison::SetPoisoned(registry, bare, 1u);
	script_poison::SetPoisoned(registry, mushroom, 1u);
	EXPECT_FALSE(script_poison::IsPoisoned(registry, bare));
	EXPECT_FALSE(script_poison::IsPoisoned(registry, mushroom));
}

TEST(ScriptPoison, AGoneThingIsNotPoisoned)
{
	Registry registry;
	EXPECT_FALSE(script_poison::IsPoisoned(registry, entt::null));
	script_poison::SetPoisoned(registry, entt::null, 1u);
}
