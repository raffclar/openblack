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
#include "ECS/Components/Mobile.h"
#include "ECS/Components/Physics.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/VillageTotem.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/LandShadowCasters.h"
#include "ECS/Registry.h"

using openblack::AbodeNumber;
using openblack::ecs::LandShadow;
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
