/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The towns' and the villagers' shared state services, and the town and villager functions that reach them

#define LOCATOR_IMPLEMENTATIONS

#include <vector>

#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "ECS/Systems/Implementations/TownStateSystem.h"
#include "ECS/Systems/Implementations/VillagerStateSystem.h"
#include "ECS/Town/TownBelief.h"
#include "ECS/Town/TownDesire.h"
#include "ECS/Town/TownVillagers.h"
#include "Locator.h"

using namespace openblack;
using openblack::ecs::systems::TownStateSystem;
using openblack::ecs::systems::VillagerStateSystem;

TEST(TownStateSystem, startsEmpty)
{
	TownStateSystem state;
	EXPECT_TRUE(state.BeliefSprites().empty());
	EXPECT_TRUE(state.Vagrants().empty());
	for (const auto turns : state.NoTownAlignmentTurns())
	{
		EXPECT_EQ(turns, 0);
	}
}

TEST(TownStateSystem, beliefSpritesComeOutLastInFirstOut)
{
	ecs::town_belief::QueueBeliefSprite(glm::vec3(1.0f), 5, 0x11u);
	ecs::town_belief::QueueBeliefSprite(glm::vec3(2.0f), 6, 0x22u);
	EXPECT_EQ(ecs::town_belief::BeliefSpriteCount(), 2u);
	const auto last = ecs::town_belief::PopBeliefSprite();
	ASSERT_TRUE(last.has_value());
	EXPECT_EQ(last->amount, 6);
	EXPECT_EQ(Locator::townStateSystem::value().BeliefSprites().size(), 1u);
}

TEST(TownStateSystem, vagrantsGoInFrontOnce)
{
	EXPECT_TRUE(ecs::town_villagers::AddToVagrants(entt::entity {1}));
	EXPECT_TRUE(ecs::town_villagers::AddToVagrants(entt::entity {2}));
	EXPECT_FALSE(ecs::town_villagers::AddToVagrants(entt::entity {1}));
	EXPECT_EQ(ecs::town_villagers::Vagrants(), (std::vector<entt::entity> {entt::entity {2}, entt::entity {1}}));
	EXPECT_TRUE(ecs::town_villagers::RemoveFromVagrants(entt::entity {2}));
	EXPECT_FALSE(ecs::town_villagers::IsVagrant(entt::entity {2}));
	ecs::town_villagers::ClearVagrants();
	EXPECT_TRUE(Locator::townStateSystem::value().Vagrants().empty());
}

TEST(VillagerStateSystem, startsWithNoVillagerEndingPhysicsAndNoTakers)
{
	VillagerStateSystem state;
	EXPECT_EQ(state.EndingPhysics(), entt::entity {entt::null});
	EXPECT_TRUE(state.MourningTakers().empty());
	++state.MourningTakers()[3];
	EXPECT_EQ(state.MourningTakers().at(3), 1u);
}

// ---- the shuffle's turn (characterisation of the formula) ----------------------------------------------------------

TEST(TownShuffleDue, IdTimesTwentyPlusTurnModuloEvery)
{
	using openblack::ecs::town_villagers::ShuffleDueEvery;
	EXPECT_TRUE(ShuffleDueEvery(0, 0, 10));
	EXPECT_TRUE(ShuffleDueEvery(1, 0, 10));  // 20
	EXPECT_FALSE(ShuffleDueEvery(1, 5, 10)); // 25
	EXPECT_TRUE(ShuffleDueEvery(3, 40, 50)); // 100
	EXPECT_FALSE(ShuffleDueEvery(7, 3, 7));  // 143
	EXPECT_FALSE(ShuffleDueEvery(2, 0, 0));  // never without a period
}
