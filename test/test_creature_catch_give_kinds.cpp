/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <gtest/gtest.h>

#include "Creature/CreatureObjectActions.h"
#include "ECS/Components/BuildingDamage.h"
#include "ECS/Components/Reward.h"
#include "ECS/Components/Scaffold.h"
#include "ECS/Components/StreetLantern.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "Locator.h"

#define LOCATOR_IMPLEMENTATIONS

#include "ECS/Systems/Implementations/CreatureObjectActionSystem.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;
using creature_object_actions::KindRule;
using creature_object_actions::Thing;

namespace
{
/// A registry of things that fly, and the system that tells which of them a creature catches
class CreatureCatchKindsTest: public ::testing::Test
{
protected:
	void SetUp() override { Locator::entitiesRegistry::emplace<Registry>(); }
	void TearDown() override { Locator::entitiesRegistry::reset(); }

	template <typename Component>
	entt::entity Make(Component component)
	{
		auto& registry = Locator::entitiesRegistry::value();
		const auto entity = registry.Create();
		registry.Assign<Transform>(entity, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
		registry.Assign<Component>(entity, component);
		return entity;
	}

	CreatureObjectActionSystem hands;
};
} // namespace

TEST_F(CreatureCatchKindsTest, AFlyingBuildingPieceIsCaughtButNeverPickedUp)
{
	const auto piece = Make(BuildingPiece {});
	const auto kind = hands.KindOf(piece);
	EXPECT_EQ(kind.thing, Thing::BuildingPiece);
	EXPECT_EQ(kind.rule, KindRule::Never);
	EXPECT_TRUE(hands.CanBeCaught(piece));
}

TEST_F(CreatureCatchKindsTest, AFlyingRewardChestIsCaught)
{
	const auto chest = Make(Reward {});
	const auto kind = hands.KindOf(chest);
	EXPECT_EQ(kind.thing, Thing::Reward);
	EXPECT_EQ(kind.rule, KindRule::Whole);
	EXPECT_TRUE(hands.CanBeCaught(chest));
}

TEST_F(CreatureCatchKindsTest, AFlyingScaffoldIsCaughtWhetherOrNotAHandCanTakeIt)
{
	const auto scaffold = Make(Scaffold {});
	const auto kind = hands.KindOf(scaffold);
	EXPECT_EQ(kind.thing, Thing::Scaffold);
	EXPECT_EQ(kind.rule, KindRule::Scaffold);
	// With no workshops to ask, no hand can take it
	EXPECT_FALSE(kind.handCanTake);
	EXPECT_TRUE(hands.CanBeCaught(scaffold));
}

TEST_F(CreatureCatchKindsTest, ALanternIsNeverCaught)
{
	const auto lantern = Make(StreetLantern {.country = false});
	EXPECT_FALSE(hands.KindOf(lantern).canBePickedUp);
	EXPECT_FALSE(hands.CanBeCaught(lantern));
}
