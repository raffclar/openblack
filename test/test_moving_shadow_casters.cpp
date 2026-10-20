/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <gtest/gtest.h>

#include "ECS/Components/Animal.h"
#include "ECS/Components/DeadTree.h"
#include "ECS/Components/Flowers.h"
#include "ECS/Components/Forest.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/Villager.h"
#include "ECS/MovingShadowCasters.h"
#include "ECS/Registry.h"

using openblack::ecs::MovingShadow;
using openblack::ecs::MovingShadowOf;
using openblack::ecs::Registry;
namespace components = openblack::ecs::components;

namespace
{

template <typename Component>
entt::entity ThingWith(Registry& registry)
{
	const auto entity = registry.Create();
	registry.Assign<Component>(entity);
	return entity;
}

} // namespace

// Trees, big forests, dead trees and pots cast through their textures' alpha
TEST(MovingShadowCasters, ThingsWithLeavesCastThroughTheAlpha)
{
	Registry registry;
	EXPECT_EQ(MovingShadowOf(registry, ThingWith<components::Tree>(registry)), MovingShadow::Texture);
	EXPECT_EQ(MovingShadowOf(registry, ThingWith<components::BigForest>(registry)), MovingShadow::Texture);
	EXPECT_EQ(MovingShadowOf(registry, ThingWith<components::DeadTree>(registry)), MovingShadow::Texture);
	EXPECT_EQ(MovingShadowOf(registry, ThingWith<components::Pot>(registry)), MovingShadow::Texture);
}

// The villagers, the animals and the things that cast into the land cast with their triangles
TEST(MovingShadowCasters, OtherCastersCastTheirShape)
{
	Registry registry;
	EXPECT_EQ(MovingShadowOf(registry, ThingWith<components::Villager>(registry)), MovingShadow::Shape);
	EXPECT_EQ(MovingShadowOf(registry, ThingWith<components::Animal>(registry)), MovingShadow::Shape);
	EXPECT_EQ(MovingShadowOf(registry, ThingWith<components::MobileStatic>(registry)), MovingShadow::Shape);
}

// A thing that casts nothing into the land and does not move by itself casts no moving shadow
TEST(MovingShadowCasters, OtherThingsCastNone)
{
	Registry registry;
	EXPECT_EQ(MovingShadowOf(registry, ThingWith<components::Flowers>(registry)), MovingShadow::None);
	EXPECT_EQ(MovingShadowOf(registry, registry.Create()), MovingShadow::None);
}
