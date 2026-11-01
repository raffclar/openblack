/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <gtest/gtest.h>

#include "ECS/Components/Abode.h"
#include "ECS/Components/AnimatedStatic.h"
#include "ECS/Components/Feature.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/Flowers.h"
#include "ECS/Components/Forest.h"
#include "ECS/Components/MapCellResident.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/NoLandShadow.h"
#include "ECS/Components/Physics.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/VillageTotem.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/LandShadowCasters.h"
#include "ECS/Registry.h"

using openblack::AbodeNumber;
using openblack::ecs::LandShadow;
using openblack::ecs::LandShadowCastersVersion;
using openblack::ecs::LandShadowChangesInPlace;
using openblack::ecs::LandShadowOf;
using openblack::ecs::Registry;
namespace components = openblack::ecs::components;

namespace
{

entt::entity AbodeOf(Registry& registry, AbodeNumber type)
{
	const auto entity = registry.Create();
	registry.Assign<components::Abode>(entity).type = type;
	return entity;
}

} // namespace

TEST(LandShadowCasters, BuildingsCastOnceBuilt)
{
	Registry registry;
	const auto house = AbodeOf(registry, AbodeNumber::A);
	EXPECT_EQ(LandShadowOf(registry, house), LandShadow::Shape);
	registry.Assign<components::BuildProgress>(house, 0.5f);
	EXPECT_EQ(LandShadowOf(registry, house), LandShadow::None);
	registry.Get<components::BuildProgress>(house).built = 1.0f;
	EXPECT_EQ(LandShadowOf(registry, house), LandShadow::Shape);
}

TEST(LandShadowCasters, ALookupGivesTheShadowOfEachThingAsTheirComponentsChange)
{
	Registry registry;
	const auto house = AbodeOf(registry, AbodeNumber::A);
	const auto field = AbodeOf(registry, AbodeNumber::Field);
	const auto tree = registry.Create();
	registry.Assign<components::Tree>(tree);
	registry.Assign<components::BuildProgress>(house, 0.5f);
	// A store that is not there yet gives nothing for its kind
	const auto nothing = registry.Create();
	const openblack::ecs::LandShadowLookup lookup(registry);
	EXPECT_EQ(lookup.Of(house), LandShadow::None);
	EXPECT_EQ(lookup.Of(field), LandShadow::None);
	EXPECT_EQ(lookup.Of(tree), LandShadow::Leaves);
	EXPECT_EQ(lookup.Of(nothing), LandShadow::None);
	// The stores stay, so the lookup sees the changes of the things in them
	registry.Get<components::BuildProgress>(house).built = 1.0f;
	EXPECT_EQ(lookup.Of(house), LandShadow::Shape);
	registry.Remove<components::BuildProgress>(house);
	EXPECT_EQ(lookup.Of(house), LandShadow::Shape);
	for (const auto entity : {house, field, tree, nothing})
	{
		EXPECT_EQ(lookup.Of(entity), LandShadowOf(registry, entity));
	}
}

TEST(LandShadowCasters, AFootballPitchCastsFromTheStart)
{
	Registry registry;
	const auto pitch = AbodeOf(registry, AbodeNumber::FootballPitch);
	registry.Assign<components::BuildProgress>(pitch, 0.0f);
	EXPECT_EQ(LandShadowOf(registry, pitch), LandShadow::Shape);
}

TEST(LandShadowCasters, FieldsCastNothing)
{
	Registry registry;
	EXPECT_EQ(LandShadowOf(registry, AbodeOf(registry, AbodeNumber::Field)), LandShadow::None);
}

TEST(LandShadowCasters, TheTempleCastsEveryTriangleOnceBuilt)
{
	Registry registry;
	const auto temple = registry.Create();
	registry.Assign<components::Temple>(temple);
	registry.Assign<components::BuildProgress>(temple, 0.9f);
	EXPECT_EQ(LandShadowOf(registry, temple), LandShadow::None);
	registry.Remove<components::BuildProgress>(temple);
	EXPECT_EQ(LandShadowOf(registry, temple), LandShadow::ShapeBothSides);
}

TEST(LandShadowCasters, AWorshipSiteCastsWhileItGoesUp)
{
	Registry registry;
	const auto site = registry.Create();
	registry.Assign<components::WorshipSite>(site);
	registry.Assign<components::BuildProgress>(site, 0.0f);
	EXPECT_EQ(LandShadowOf(registry, site), LandShadow::Shape);
}

TEST(LandShadowCasters, TreesCastThroughTheirLeaves)
{
	Registry registry;
	const auto tree = registry.Create();
	registry.Assign<components::Tree>(tree);
	const auto forest = registry.Create();
	registry.Assign<components::BigForest>(forest);
	EXPECT_EQ(LandShadowOf(registry, tree), LandShadow::Leaves);
	EXPECT_EQ(LandShadowOf(registry, forest), LandShadow::Leaves);
}

TEST(LandShadowCasters, FeaturesAndRocksCast)
{
	Registry registry;
	const auto feature = registry.Create();
	registry.Assign<components::Feature>(feature);
	const auto rock = registry.Create();
	registry.Assign<components::MobileStatic>(rock);
	EXPECT_EQ(LandShadowOf(registry, feature), LandShadow::Shape);
	EXPECT_EQ(LandShadowOf(registry, rock), LandShadow::Shape);
}

TEST(LandShadowCasters, SceneryThatCastsNothing)
{
	Registry registry;
	const auto animated = registry.Create();
	registry.Assign<components::AnimatedStatic>(animated);
	registry.Assign<components::Feature>(animated);
	const auto flowers = registry.Create();
	registry.Assign<components::Flowers>(flowers);
	const auto pot = registry.Create();
	registry.Assign<components::Pot>(pot);
	const auto totem = registry.Create();
	registry.Assign<components::VillageTotem>(totem);
	const auto nothing = registry.Create();
	EXPECT_EQ(LandShadowOf(registry, animated), LandShadow::None);
	EXPECT_EQ(LandShadowOf(registry, flowers), LandShadow::None);
	EXPECT_EQ(LandShadowOf(registry, pot), LandShadow::None);
	EXPECT_EQ(LandShadowOf(registry, totem), LandShadow::None);
	EXPECT_EQ(LandShadowOf(registry, nothing), LandShadow::None);
}

TEST(LandShadowCasters, OnlyBuildingsChangeTheirShadowInPlace)
{
	Registry registry;
	const auto house = AbodeOf(registry, AbodeNumber::A);
	const auto altar = registry.Create();
	registry.Assign<components::WorshipAltar>(altar);
	registry.Assign<components::BuildProgress>(altar, 0.5f);
	const auto tree = registry.Create();
	registry.Assign<components::Tree>(tree);
	const auto rock = registry.Create();
	registry.Assign<components::MobileStatic>(rock);
	EXPECT_TRUE(LandShadowChangesInPlace(registry, house));
	EXPECT_TRUE(LandShadowChangesInPlace(registry, altar));
	EXPECT_FALSE(LandShadowChangesInPlace(registry, tree));
	EXPECT_FALSE(LandShadowChangesInPlace(registry, rock));
}

TEST(LandShadowCasters, TheCountGoesUpWhenAThingThatCanCastChanges)
{
	Registry registry;
	auto version = LandShadowCastersVersion(registry);
	const auto tree = registry.Create();
	registry.Assign<components::Mesh>(tree);
	EXPECT_EQ(LandShadowCastersVersion(registry), version);
	registry.Assign<components::Transform>(tree);
	EXPECT_GT(LandShadowCastersVersion(registry), version);
	version = LandShadowCastersVersion(registry);
	registry.Assign<components::Tree>(tree);
	EXPECT_GT(LandShadowCastersVersion(registry), version);
	version = LandShadowCastersVersion(registry);
	registry.Assign<components::MapCellResident>(tree);
	EXPECT_GT(LandShadowCastersVersion(registry), version);
	version = LandShadowCastersVersion(registry);
	registry.Remove<components::MapCellResident>(tree);
	EXPECT_GT(LandShadowCastersVersion(registry), version);
	version = LandShadowCastersVersion(registry);
	registry.Destroy(tree);
	EXPECT_GT(LandShadowCastersVersion(registry), version);
}

TEST(LandShadowCasters, TheCountStaysForThingsThatCannotCast)
{
	Registry registry;
	const auto version = LandShadowCastersVersion(registry);
	// An effect has a transform and no mesh
	const auto effect = registry.Create();
	registry.Assign<components::Transform>(effect);
	registry.Assign<components::Feature>(effect);
	registry.Destroy(effect);
	// The count does not include a change in place: the bake checks these changes itself
	const auto house = AbodeOf(registry, AbodeNumber::A);
	registry.Assign<components::Mesh>(house);
	registry.Assign<components::BuildProgress>(house, 0.5f);
	const auto made = LandShadowCastersVersion(registry);
	EXPECT_EQ(made, version);
	registry.Get<components::BuildProgress>(house).built = 1.0f;
	EXPECT_EQ(LandShadowCastersVersion(registry), made);
}

TEST(LandShadowCasters, TheCountKeepsCountingAfterTheRegistryIsEmptied)
{
	Registry registry;
	const auto thing = registry.Create();
	registry.Assign<components::Mesh>(thing);
	registry.Assign<components::Transform>(thing);
	const auto version = LandShadowCastersVersion(registry);
	registry.Reset();
	const auto emptied = LandShadowCastersVersion(registry);
	EXPECT_GT(emptied, version);
	const auto again = registry.Create();
	registry.Assign<components::Mesh>(again);
	registry.Assign<components::Transform>(again);
	EXPECT_GT(LandShadowCastersVersion(registry), emptied);
}

TEST(LandShadowCasters, AThingWithNoLandShadowCastsNothing)
{
	Registry registry;
	const auto ring = registry.Create();
	registry.Assign<components::MobileObject>(ring);
	registry.Assign<components::Mesh>(ring);
	registry.Assign<components::Transform>(ring);
	EXPECT_EQ(LandShadowOf(registry, ring), LandShadow::Shape);
	const auto version = LandShadowCastersVersion(registry);
	registry.Assign<components::NoLandShadow>(ring);
	EXPECT_EQ(LandShadowOf(registry, ring), LandShadow::None);
	EXPECT_GT(LandShadowCastersVersion(registry), version);
}
