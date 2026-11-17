/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// A saved world that loads into a registry with fresh ids keeps the store of the ids that it had at the save. The
// entities that the world makes after the load then get the ids that they got after the save.

#include <cstdint>

#include <vector>

#include <entt/entity/registry.hpp>
#include <entt/meta/context.hpp>
#include <gtest/gtest.h>

#include "ECS/Components/Transform.h"
#include "ECS/Reflection/ComponentReflection.h"
#include "ECS/Reflection/WorldArchive.h"
#include "ECS/Registry.h"

using namespace openblack::ecs;

namespace
{

std::vector<entt::entity> MakeSome(Registry& registry, int count)
{
	std::vector<entt::entity> made;
	for (int i = 0; i < count; ++i)
	{
		const auto entity = registry.Create();
		registry.Assign<components::Transform>(entity, components::Transform {.position = {static_cast<float>(i), 0.0f, 0.0f}});
		made.push_back(entity);
	}
	return made;
}

/// A world with entities that went, so that the store of the ids has free places
void MakeSavedWorld(Registry& registry)
{
	const auto made = MakeSome(registry, 12);
	registry.Destroy(made[3]);
	registry.Destroy(made[7]);
	registry.Destroy(made[1]);
}

/// A registry that made more entities than the saved world before the load
void UseMore(Registry& registry)
{
	const auto made = MakeSome(registry, 30);
	for (size_t i = 0; i < made.size(); i += 4)
	{
		registry.Destroy(made[i]);
	}
}

} // namespace

TEST(SavedWorldIds, ALoadWithFreshIdsKeepsTheStoreOfTheIdsOfTheSave)
{
	entt::meta_ctx context;
	reflection::RegisterComponents(context);
	Registry saved;
	MakeSavedWorld(saved);
	const auto bytes = reflection::SaveWorld(saved.Underlying(), context);
	const auto afterSave = MakeSome(saved, 5);

	Registry loaded;
	UseMore(loaded);
	loaded.SetFreshIdsAtNextReset();
	loaded.Reset();
	ASSERT_TRUE(reflection::LoadWorld(loaded.Underlying(), context, bytes, reflection::WorldLoadMode::Exact).has_value());
	EXPECT_EQ(reflection::SaveWorld(loaded.Underlying(), context), bytes);
	EXPECT_EQ(MakeSome(loaded, 5), afterSave);
}

TEST(SavedWorldIds, APlainResetKeepsThePlacesOfTheEntitiesBeforeTheLoad)
{
	entt::meta_ctx context;
	reflection::RegisterComponents(context);
	Registry saved;
	MakeSavedWorld(saved);
	const auto bytes = reflection::SaveWorld(saved.Underlying(), context);

	Registry loaded;
	UseMore(loaded);
	loaded.Reset();
	ASSERT_TRUE(reflection::LoadWorld(loaded.Underlying(), context, bytes, reflection::WorldLoadMode::Exact).has_value());
	// The store keeps the places of the entities that the registry made before the load
	EXPECT_NE(reflection::SaveWorld(loaded.Underlying(), context), bytes);
}
