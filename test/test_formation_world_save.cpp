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

#include "ECS/Components/Feature.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/Transform.h"
#include "ECS/Reflection/ComponentReflection.h"
#include "ECS/Reflection/WorldArchive.h"
#include "ECS/Registry.h"
#include "Enums.h"

using namespace openblack;
using namespace openblack::ecs;
using openblack::ecs::components::Feature;
using openblack::ecs::components::Mobile;
using openblack::ecs::components::MobileObject;
using openblack::ecs::components::Transform;

namespace
{
entt::entity AddMushroom(Registry& registry, const glm::vec3& position, MobileObjectInfo type, float scale)
{
	const auto entity = registry.Create();
	registry.Assign<Transform>(entity, position, glm::mat3(1.0f), glm::vec3(scale));
	registry.Assign<Mobile>(entity);
	registry.Assign<MobileObject>(entity, MobileObject {.type = type});
	return entity;
}

entt::entity AddPillar(Registry& registry, const glm::vec3& position, FeatureInfo type, float scale)
{
	const auto entity = registry.Create();
	registry.Assign<Transform>(entity, position, glm::mat3(1.0f), glm::vec3(scale));
	registry.Assign<Feature>(entity, Feature {.type = type});
	return entity;
}

/// A fairy ring of the first land of the story and a pair of its lone pillars, as the generator puts them on a land
void AddFormations(Registry& registry)
{
	for (int i = 0; i < 10; ++i)
	{
		AddMushroom(registry, {2200.0f + static_cast<float>(i), 27.0f, 2300.0f}, MobileObjectInfo::MagicMushroom, 1.179f);
	}
	AddMushroom(registry, {2198.0f, 27.0f, 2315.0f}, MobileObjectInfo::Champi, 1.0f);
	AddMushroom(registry, {2205.0f, 27.0f, 2305.0f}, MobileObjectInfo::Champi, 1.0f);
	AddPillar(registry, {2111.5f, 40.0f, 2623.9f}, FeatureInfo::FatPilarLime, 0.578f);
	AddPillar(registry, {2111.8f, 41.0f, 2635.3f}, FeatureInfo::Pilar3Lime, 1.0f);
}

size_t Count(Registry& registry, MobileObjectInfo type)
{
	size_t count = 0;
	registry.Each<const MobileObject, const Mobile>(
	    [&](entt::entity, const MobileObject& object, const Mobile&) { count += object.type == type ? 1 : 0; });
	return count;
}

size_t CountPillars(Registry& registry)
{
	size_t count = 0;
	registry.Each<const Feature>([&count](entt::entity, const Feature&) { ++count; });
	return count;
}
} // namespace

// The mushrooms and the pillars of the formations of a generated land are in the save. A load gives each one one time,
// with its kind, its place and its scale, and a second save is the same as the first.
TEST(FormationWorldSave, ALoadKeepsEachMushroomAndPillarOneTime)
{
	entt::meta_ctx context;
	reflection::RegisterComponents(context);
	Registry saved;
	AddFormations(saved);
	const auto bytes = reflection::SaveWorld(saved.Underlying(), context);

	Registry loaded;
	ASSERT_TRUE(reflection::LoadWorld(loaded.Underlying(), context, bytes, reflection::WorldLoadMode::Exact).has_value());
	EXPECT_EQ(Count(loaded, MobileObjectInfo::MagicMushroom), 10u);
	EXPECT_EQ(Count(loaded, MobileObjectInfo::Champi), 2u);
	EXPECT_EQ(CountPillars(loaded), 2u);
	EXPECT_EQ(reflection::SaveWorld(loaded.Underlying(), context), bytes);

	bool fat = false;
	loaded.Each<const Feature, const Transform>([&fat](entt::entity, const Feature& feature, const Transform& transform) {
		if (feature.type == FeatureInfo::FatPilarLime)
		{
			fat = true;
			EXPECT_EQ(transform.position, glm::vec3(2111.5f, 40.0f, 2623.9f));
			EXPECT_EQ(transform.scale, glm::vec3(0.578f));
		}
	});
	EXPECT_TRUE(fat);
}

// Two saves and loads in a sequence do not add a mushroom or a pillar
TEST(FormationWorldSave, ASecondSaveAndLoadDoNotAddAThing)
{
	entt::meta_ctx context;
	reflection::RegisterComponents(context);
	Registry saved;
	AddFormations(saved);
	Registry first;
	ASSERT_TRUE(reflection::LoadWorld(first.Underlying(), context, reflection::SaveWorld(saved.Underlying(), context),
	                                  reflection::WorldLoadMode::Exact)
	                .has_value());
	Registry second;
	ASSERT_TRUE(reflection::LoadWorld(second.Underlying(), context, reflection::SaveWorld(first.Underlying(), context),
	                                  reflection::WorldLoadMode::Exact)
	                .has_value());
	EXPECT_EQ(Count(second, MobileObjectInfo::MagicMushroom), 10u);
	EXPECT_EQ(Count(second, MobileObjectInfo::Champi), 2u);
	EXPECT_EQ(CountPillars(second), 2u);
}
