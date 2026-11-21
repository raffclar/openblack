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
#include <vector>

#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#include "ECS/Components/ForestMember.h"
#include "ECS/Components/LandForest.h"
#include "ECS/Components/MagicForest.h"
#include "ECS/Components/Spell.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Registry.h"
#include "Locator.h"
#include "Magic/ForestRules.h"

#define LOCATOR_IMPLEMENTATIONS 1
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
		// The forest system reports to the game's log
		if (spdlog::get("game") == nullptr)
		{
			spdlog::create<spdlog::sinks::null_sink_st>("game");
		}
		Locator::entitiesRegistry::emplace<ecs::Registry>();
		auto& registry = Locator::entitiesRegistry::value();
		_land = Thing();
		registry.Assign<LandForest>(_land, LandForest {.id = k_ForestId});
		_record = Thing();
		_first = MagicTreeIn(k_ForestId);
		_second = MagicTreeIn(k_ForestId);
		registry.Assign<MagicForest>(_record, MagicForest {.spell = k_Spell,
		                                                   .player = PlayerNames::PLAYER_ONE,
		                                                   .centre = glm::vec3(0.0f),
		                                                   .trees = {_first, _second},
		                                                   .landForest = _land,
		                                                   .gone = false});
		_forests = std::make_unique<ecs::systems::ForestSystem>();
	}

	void TearDown() override
	{
		_forests.reset();
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
	[[nodiscard]] entt::entity MagicTreeIn(uint32_t forest) const
	{
		const auto tree = TreeIn(forest);
		Locator::entitiesRegistry::value().Assign<MagicTree>(tree, MagicTree {.forest = _record});
		return tree;
	}

	entt::entity _land {entt::null};
	entt::entity _record {entt::null};
	entt::entity _first {entt::null};
	entt::entity _second {entt::null};
	std::unique_ptr<ecs::systems::ForestSystem> _forests;
};
} // namespace

TEST_F(ForestMiracleEmpty, TheForestStaysWhileATreeIsLeft)
{
	Locator::entitiesRegistry::value().Destroy(_first);
	EXPECT_EQ(_forests->ForestOf(k_Spell), MiracleForest::Standing);
}

TEST_F(ForestMiracleEmpty, TheForestGoesWithItsLastMagicTree)
{
	auto& registry = Locator::entitiesRegistry::value();
	registry.Destroy(_first);
	registry.Destroy(_second);
	EXPECT_EQ(_forests->ForestOf(k_Spell), MiracleForest::Gone);

	// At its turn the miracle lets go of the forest: the record and the forest of the land go
	_forests->LetGoOfGoneForest(k_Spell);
	EXPECT_EQ(_forests->ForestOf(k_Spell), MiracleForest::None);
	EXPECT_FALSE(registry.Valid(_record));
	EXPECT_FALSE(registry.Valid(_land));
	EXPECT_FALSE(_forests->TallestTreeOf(k_Spell).has_value());
}

TEST_F(ForestMiracleEmpty, ATreeThatSpreadIntoTheForestKeepsIt)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto young = TreeIn(k_ForestId);
	registry.Destroy(_first);
	registry.Destroy(_second);
	EXPECT_EQ(_forests->ForestOf(k_Spell), MiracleForest::Standing);

	// The young tree is not the miracle's: when it goes last, the forest stays on the land, empty
	registry.Destroy(young);
	EXPECT_EQ(_forests->ForestOf(k_Spell), MiracleForest::Standing);
	EXPECT_TRUE(registry.Valid(_land));
}

TEST_F(ForestMiracleEmpty, ATreeOfAnotherForestDoesNotCount)
{
	auto& registry = Locator::entitiesRegistry::value();
	std::ignore = TreeIn(k_OtherForestId);
	registry.Destroy(_first);
	registry.Destroy(_second);
	EXPECT_EQ(_forests->ForestOf(k_Spell), MiracleForest::Gone);
}

// The forest of a saved game takes the magic trees of its forest of the land again, the grown trees first. A tree that
// the spell did not plant stays out of it.
TEST_F(ForestMiracleEmpty, AForestOfASavedGameTakesItsMagicTreesAgain)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto young = TreeIn(k_ForestId);
	registry.Get<ForestMember>(_second).growing = true;
	registry.Remove<MagicForest>(_record);
	constexpr auto k_SavedSpell = entt::entity {901};
	Spell miracle {.castPosition = glm::vec3(4.0f, 0.0f, 5.0f)};
	miracle.caster.player = PlayerNames::PLAYER_TWO;
	EXPECT_EQ(_forests->RestoreForest(k_SavedSpell, miracle, _land), 2U);
	EXPECT_EQ(miracle.objectCount, 2U);
	const auto made = registry.Get<const MagicTree>(_first).forest;
	ASSERT_TRUE(made != entt::null && made != _record);
	EXPECT_EQ(registry.Get<const MagicTree>(_second).forest, made);
	EXPECT_FALSE(registry.AllOf<MagicTree>(young));
	const auto& forest = registry.Get<const MagicForest>(made);
	EXPECT_EQ(forest.spell, k_SavedSpell);
	EXPECT_EQ(forest.player, PlayerNames::PLAYER_TWO);
	EXPECT_EQ(forest.centre, glm::vec3(4.0f, 0.0f, 5.0f));
	EXPECT_EQ(forest.landForest, _land);
	EXPECT_EQ(forest.trees, (std::vector<entt::entity> {_first, _second}));
	EXPECT_EQ(_forests->ForestOf(k_SavedSpell), MiracleForest::Standing);
	// A spell with no forest of the land takes no trees
	Spell other;
	EXPECT_EQ(_forests->RestoreForest(entt::entity {902}, other, entt::null), 0U);
}
