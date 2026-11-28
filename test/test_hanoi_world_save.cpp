/*******************************************************************************
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

#include "ECS/Components/Mesh.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/NoLandShadow.h"
#include "ECS/Components/PuzzleGame.h"
#include "ECS/Components/RepaintsLandWhenFiled.h"
#include "ECS/Components/Transform.h"
#include "ECS/LandShadowCasters.h"
#include "ECS/Reflection/ComponentReflection.h"
#include "ECS/Reflection/WorldArchive.h"
#include "ECS/Registry.h"

using namespace openblack;
using namespace openblack::ecs;

namespace
{
entt::entity AddLooseThing(Registry& registry)
{
	const auto entity = registry.Create();
	registry.Assign<components::Transform>(entity, glm::vec3(10.0f, 0.0f, 20.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	registry.Assign<components::Mesh>(entity);
	registry.Assign<components::MobileObject>(entity);
	return entity;
}
} // namespace

// The game keeps the land shadow and the paint flag of a ring on its model. A load makes the models again, so after a
// load every ring casts and no ring asks for a paint. A thing that never casts by its kind still casts nothing.
TEST(HanoiWorldSave, ALoadGivesEachRingItsLandShadowBack)
{
	entt::meta_ctx context;
	reflection::RegisterComponents(context);
	Registry saved;
	const auto smallRing = AddLooseThing(saved);
	saved.Assign<components::HanoiBlock>(smallRing);
	saved.Assign<components::ModelCastsNoLandShadow>(smallRing);
	const auto largeRing = AddLooseThing(saved);
	saved.Assign<components::HanoiBlock>(largeRing);
	saved.Assign<components::RepaintsLandWhenFiled>(largeRing, 2u);
	const auto glow = AddLooseThing(saved);
	saved.Assign<components::NoLandShadow>(glow);
	ASSERT_EQ(LandShadowOf(saved, smallRing), LandShadow::None);
	const auto bytes = reflection::SaveWorld(saved.Underlying(), context);

	Registry loaded;
	ASSERT_TRUE(reflection::LoadWorld(loaded.Underlying(), context, bytes, reflection::WorldLoadMode::Exact).has_value());
	size_t rings = 0;
	loaded.Each<const components::HanoiBlock>([&](entt::entity ring, const components::HanoiBlock& /*block*/) {
		++rings;
		EXPECT_FALSE(loaded.AllOf<components::ModelCastsNoLandShadow>(ring));
		EXPECT_FALSE(loaded.AllOf<components::RepaintsLandWhenFiled>(ring));
		EXPECT_EQ(LandShadowOf(loaded, ring), LandShadow::Shape);
	});
	EXPECT_EQ(rings, 2u);
	size_t glows = 0;
	loaded.Each<const components::MobileObject>([&](entt::entity thing, const components::MobileObject& /*object*/) {
		if (!loaded.AllOf<components::HanoiBlock>(thing))
		{
			++glows;
			EXPECT_EQ(LandShadowOf(loaded, thing), LandShadow::None);
		}
	});
	EXPECT_EQ(glows, 1u);
}
