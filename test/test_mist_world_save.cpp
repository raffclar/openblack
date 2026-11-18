/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <vector>

#include <entt/entity/registry.hpp>
#include <entt/meta/context.hpp>
#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "ECS/Components/Cloud.h"
#include "ECS/Components/Mist.h"
#include "ECS/Components/Transform.h"
#include "ECS/Reflection/ComponentReflection.h"
#include "ECS/Reflection/WorldArchive.h"
#include "ECS/Registry.h"
#include "ECS/WorldSave.h"

using namespace openblack;
using namespace openblack::ecs;
using openblack::ecs::components::Cloud;
using openblack::ecs::components::Mist;
using openblack::ecs::components::Transform;

namespace
{
entt::entity AddMist(Registry& registry, const glm::vec3& position, uint32_t colour, float size, float edgeShrink)
{
	const auto entity = registry.Create();
	registry.Assign<Transform>(entity, position, glm::mat3(1.0f), glm::vec3(size));
	registry.Assign<Mist>(entity, Mist {
	                                  .size = size,
	                                  .colour = colour,
	                                  .shrinksEdgeOn = edgeShrink != 1.0f,
	                                  .edgeShrink = edgeShrink != 1.0f ? edgeShrink : 3.0f,
	                                  .counter = 7,
	                              });
	return entity;
}

size_t CountMists(Registry& registry)
{
	size_t count = 0;
	registry.Each<const Mist>([&count](entt::entity, const Mist&) { ++count; });
	return count;
}

/// The world as a load of a saved game reads it: a new registry that gets the saved entities, then the sky's clouds of
/// an older save go
void LoadInto(Registry& loaded, const entt::meta_ctx& context, const std::vector<uint8_t>& bytes)
{
	ASSERT_TRUE(reflection::LoadWorld(loaded.Underlying(), context, bytes, reflection::WorldLoadMode::Exact).has_value());
	world_save::RemoveSavedSkyClouds(loaded.Underlying());
}
} // namespace

// The mists of a cursed place of a generated land are in the save. The load gives each mist one time, with its colour,
// its size, its place and its animation.
TEST(MistWorldSave, ALoadKeepsEachMistOneTime)
{
	entt::meta_ctx context;
	reflection::RegisterComponents(context);
	Registry saved;
	const auto dark = AddMist(saved, {2930.0f, 120.0f, 2640.0f}, 0x80202020u, 4.0f, 1.0f);
	const auto smoke = AddMist(saved, {2990.0f, 95.0f, 2700.0f}, 0x6D505050u, 0.578f, 2.642f);
	saved.Get<Mist>(smoke).fading = true;
	saved.Get<Mist>(smoke).fadeTurns = 30.0f;
	const auto bytes = reflection::SaveWorld(saved.Underlying(), context);

	Registry loaded;
	LoadInto(loaded, context, bytes);
	ASSERT_EQ(CountMists(loaded), 2u);
	// The load gives the same world again, so a second save is the same as the first
	EXPECT_EQ(reflection::SaveWorld(loaded.Underlying(), context), bytes);

	size_t found = 0;
	loaded.Each<const Mist, const Transform>([&](entt::entity, const Mist& mist, const Transform& transform) {
		const auto& original = mist.colour == 0x6D505050u ? saved.Get<const Mist>(smoke) : saved.Get<const Mist>(dark);
		const auto& place = mist.colour == 0x6D505050u ? saved.Get<const Transform>(smoke) : saved.Get<const Transform>(dark);
		EXPECT_EQ(mist.size, original.size);
		EXPECT_EQ(mist.shrinksEdgeOn, original.shrinksEdgeOn);
		EXPECT_EQ(mist.edgeShrink, original.edgeShrink);
		EXPECT_EQ(mist.counter, original.counter);
		EXPECT_EQ(mist.fading, original.fading);
		EXPECT_EQ(mist.fadeTurns, original.fadeTurns);
		EXPECT_EQ(transform.position, place.position);
		++found;
	});
	EXPECT_EQ(found, 2u);
}

// Two saves and loads in a sequence do not add a mist: each load starts from a new registry
TEST(MistWorldSave, ASecondSaveAndLoadDoNotAddAMist)
{
	entt::meta_ctx context;
	reflection::RegisterComponents(context);
	Registry saved;
	for (int i = 0; i < 5; ++i)
	{
		AddMist(saved, {100.0f * static_cast<float>(i), 10.0f, 50.0f}, 0xA0101010u, 3.0f, 1.0f);
	}
	Registry first;
	LoadInto(first, context, reflection::SaveWorld(saved.Underlying(), context));
	Registry second;
	LoadInto(second, context, reflection::SaveWorld(first.Underlying(), context));
	EXPECT_EQ(CountMists(second), 5u);
}

// An older save keeps the sky's clouds, which are mists too. The load removes them, but it keeps each mist of the land.
TEST(MistWorldSave, TheLoadRemovesTheSavedCloudsAndKeepsTheLandMists)
{
	entt::meta_ctx context;
	reflection::RegisterComponents(context);
	Registry saved;
	AddMist(saved, {10.0f, 5.0f, 10.0f}, 0x80202020u, 4.0f, 1.0f);
	const auto cloud = AddMist(saved, {0.0f, 500.0f, 0.0f}, 0x40000000u, 30.0f, 2.0f);
	saved.Assign<Cloud>(cloud, glm::vec3(0.0f), false);

	Registry loaded;
	LoadInto(loaded, context, reflection::SaveWorld(saved.Underlying(), context));
	EXPECT_EQ(CountMists(loaded), 1u);
	size_t clouds = 0;
	loaded.Each<const Cloud>([&clouds](entt::entity, const Cloud&) { ++clouds; });
	EXPECT_EQ(clouds, 0u);
}
