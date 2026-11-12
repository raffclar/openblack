/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <memory>
#include <tuple>

#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "ECS/Components/ForestMember.h"
#include "ECS/Components/LandForest.h"
#include "ECS/Components/MagicForest.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Registry.h"
#include "Locator.h"
#include "Magic/ForestRules.h"

#define LOCATOR_IMPLEMENTATIONS
#include "ECS/Systems/Implementations/ForestSystem.h"

using namespace openblack;
using namespace openblack::ecs::components;
using openblack::magic::forest::MiracleForest;

namespace
{
constexpr uint32_t k_ForestId = 3;
constexpr uint32_t k_OtherForestId = 4;
/// The miracle's entity: the forest system only compares it
constexpr auto k_Spell = entt::entity {900};

/// A forest miracle's forest with two magic trees, made on a registry of its own. The system is made after the
/// registry, as the game makes it.
class ForestMiracleEmpty: public ::testing::Test
{
protected:
	void SetUp() override
	{
		Locator::entitiesRegistry::emplace<ecs::Registry>();
		auto& registry = Locator::entitiesRegistry::value();
		land = Thing();
		registry.Assign<LandForest>(land, LandForest {.id = k_ForestId});
		record = Thing();
		first = MagicTreeIn(k_ForestId);
		second = MagicTreeIn(k_ForestId);
		registry.Assign<MagicForest>(record, MagicForest {.spell = k_Spell,
		                                                  .player = PlayerNames::PLAYER_ONE,
		                                                  .centre = glm::vec3(0.0f),
		                                                  .trees = {first, second},
		                                                  .landForest = land,
		                                                  .gone = false});
		forests = std::make_unique<ecs::systems::ForestSystem>();
	}

	void TearDown() override
	{
		forests.reset();
		Locator::entitiesRegistry::reset();
	}

	static entt::entity Thing()
	{
		auto& registry = Locator::entitiesRegistry::value();
		const auto thing = registry.Create();
		registry.Assign<Transform>(thing, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
		return thing;
	}

	/// A tree of a forest of the land, with no part in the miracle
	static entt::entity TreeIn(uint32_t forest)
	{
		auto& registry = Locator::entitiesRegistry::value();
		const auto tree = Thing();
		registry.Assign<Tree>(tree, Tree {.type = TreeInfo::Beech, .maxSize = 1.0f});
		registry.Assign<ForestMember>(tree, ForestMember {.forest = forest, .growing = false, .listed = 0});
		return tree;
	}

	/// A tree that the miracle planted
	entt::entity MagicTreeIn(uint32_t forest) const
	{
		const auto tree = TreeIn(forest);
		Locator::entitiesRegistry::value().Assign<MagicTree>(tree, MagicTree {.forest = record});
		return tree;
	}

	entt::entity land {entt::null};
	entt::entity record {entt::null};
	entt::entity first {entt::null};
	entt::entity second {entt::null};
	std::unique_ptr<ecs::systems::ForestSystem> forests;
};
} // namespace

TEST_F(ForestMiracleEmpty, TheForestStaysWhileATreeIsLeft)
{
	Locator::entitiesRegistry::value().Destroy(first);
	EXPECT_EQ(forests->ForestOf(k_Spell), MiracleForest::Standing);
}

TEST_F(ForestMiracleEmpty, TheForestGoesWithItsLastMagicTree)
{
	auto& registry = Locator::entitiesRegistry::value();
	registry.Destroy(first);
	registry.Destroy(second);
	EXPECT_EQ(forests->ForestOf(k_Spell), MiracleForest::Gone);

	// At its turn the miracle lets go of the forest: the record and the forest of the land go
	forests->LetGoOfGoneForest(k_Spell);
	EXPECT_EQ(forests->ForestOf(k_Spell), MiracleForest::None);
	EXPECT_FALSE(registry.Valid(record));
	EXPECT_FALSE(registry.Valid(land));
	EXPECT_FALSE(forests->TallestTreeOf(k_Spell).has_value());
}

TEST_F(ForestMiracleEmpty, ATreeThatSpreadIntoTheForestKeepsIt)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto young = TreeIn(k_ForestId);
	registry.Destroy(first);
	registry.Destroy(second);
	EXPECT_EQ(forests->ForestOf(k_Spell), MiracleForest::Standing);

	// The young tree is not the miracle's: when it goes last, the forest stays on the land, empty
	registry.Destroy(young);
	EXPECT_EQ(forests->ForestOf(k_Spell), MiracleForest::Standing);
	EXPECT_TRUE(registry.Valid(land));
}

TEST_F(ForestMiracleEmpty, ATreeOfAnotherForestDoesNotCount)
{
	auto& registry = Locator::entitiesRegistry::value();
	std::ignore = TreeIn(k_OtherForestId);
	registry.Destroy(first);
	registry.Destroy(second);
	EXPECT_EQ(forests->ForestOf(k_Spell), MiracleForest::Gone);
}
