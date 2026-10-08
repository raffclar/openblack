/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The creature systems on a registry with no creature: their per-turn and per-frame calls make no storage and no
// entity, so the state hash's pools stay as they were when the game calls them on a land with no creature

#define LOCATOR_IMPLEMENTATIONS

#include <cstddef>

#include <chrono>
#include <utility>
#include <vector>

#include <entt/core/fwd.hpp>
#include <entt/entity/sparse_set.hpp>
#include <gtest/gtest.h>

#include "ECS/Systems/Implementations/CreatureAnimationSystem.h"
#include "ECS/Systems/Implementations/CreatureHairSystem.h"
#include "ECS/Systems/Implementations/CreatureLocomotionSystem.h"
#include "ECS/Systems/Implementations/CreaturePhysiologySystem.h"
#include "ECS/Systems/Implementations/CreatureSkinSystem.h"
#include "ECS/Systems/Implementations/FootprintSystem.h"
#include "creature/CreatureSystemWorld.h"

using namespace openblack;
using namespace openblack::ecs::systems;
using namespace std::chrono_literals;

namespace
{
/// Every storage of the registry, the entities' own first, with how many it holds
using Storages = std::vector<std::pair<entt::id_type, size_t>>;

Storages StoragesOf(const ecs::Registry& registry)
{
	Storages storages;
	registry.EachStorage(
	    [&storages](entt::id_type id, const entt::sparse_set& storage) { storages.emplace_back(id, storage.size()); });
	return storages;
}

class CreatureSystemsEmptyRegistryTest: public ::testing::Test
{
protected:
	/// The registry's storages, after a call that should leave them as they were
	template <typename Call>
	static Storages After(Call call)
	{
		call();
		return StoragesOf(test::creature_world::World::Registry());
	}

	test::creature_world::World _world;
};
} // namespace

TEST_F(CreatureSystemsEmptyRegistryTest, AnimationMakesNoStorage)
{
	const auto before = StoragesOf(test::creature_world::World::Registry());
	CreatureAnimationSystem system;
	EXPECT_EQ(After([&system]() { system.ProcessTurn(); }), before);
	EXPECT_EQ(After([&system]() { system.Update(16ms); }), before);
}

TEST_F(CreatureSystemsEmptyRegistryTest, SkinMakesNoStorage)
{
	const auto before = StoragesOf(test::creature_world::World::Registry());
	CreatureSkinSystem system;
	EXPECT_EQ(After([&system]() { system.ProcessTurn(); }), before);
	EXPECT_EQ(After([&system]() { system.Update(); }), before);
}

TEST_F(CreatureSystemsEmptyRegistryTest, HairMakesNoStorageShownOrHidden)
{
	const auto before = StoragesOf(test::creature_world::World::Registry());
	CreatureHairSystem system;
	EXPECT_EQ(After([&system]() { system.Update(16ms); }), before);
	EXPECT_EQ(After([&system]() { system.SetShown(false); }), before);
	EXPECT_EQ(After([&system]() { system.Update(16ms); }), before);
}

TEST_F(CreatureSystemsEmptyRegistryTest, FootprintsMakeNoStorage)
{
	const auto before = StoragesOf(test::creature_world::World::Registry());
	FootprintSystem system([]() { return CalendarDate {}; });
	EXPECT_EQ(After([&system]() { system.Update(16ms); }), before);
}

TEST_F(CreatureSystemsEmptyRegistryTest, LocomotionMakesNoStorage)
{
	const auto before = StoragesOf(test::creature_world::World::Registry());
	CreatureLocomotionSystem system;
	EXPECT_EQ(After([&system]() { system.ProcessTurn(); }), before);
	EXPECT_EQ(After([&system]() { system.Update(0.5f); }), before);
}

TEST_F(CreatureSystemsEmptyRegistryTest, PhysiologyMakesNoStorage)
{
	const auto before = StoragesOf(test::creature_world::World::Registry());
	CreaturePhysiologySystem system;
	EXPECT_EQ(After([&system]() { system.ProcessTurn(); }), before);
	EXPECT_EQ(After([&system]() { system.Update(0.016f); }), before);
}
