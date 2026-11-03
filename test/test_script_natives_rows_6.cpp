/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>

#include <algorithm>
#include <functional>
#include <vector>

#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "Creature/CreatureDecisionTree.h"
#include "Creature/CreatureIdleMind.h"
#include "Creature/CreatureWatching.h"
#include "ECS/ScriptCreatureCounts.h"
#include "ECS/VillagerWorkRules.h"
#include "Input/MouseWheel.h"

using namespace openblack;

namespace
{

creature_watching::Knowledge StartKnowledge()
{
	const std::vector<creature_watching::SkillRule> skills(6);
	const std::vector<creature_watching::MiracleRule> miracles(42);
	return creature_watching::StartKnowledge(skills, miracles);
}

bool AllKnown(const std::vector<bool>& known)
{
	return std::ranges::all_of(known, std::identity {});
}

bool NoneKnown(const std::vector<bool>& known)
{
	return std::ranges::none_of(known, std::identity {});
}

} // namespace

TEST(ScriptNativesRows6, TheCountsOfActionsAreTheMealsTheDestroyedObjectsAndAZero)
{
	ecs::components::Creature creature {};
	creature.timesEaten = 3.0f;
	creature.objectsDestroyed = 7.0f;
	EXPECT_EQ(ecs::script_creature_counts::Count(creature, 0), 3.0f);
	EXPECT_EQ(ecs::script_creature_counts::Count(creature, 1), 7.0f);
	EXPECT_EQ(ecs::script_creature_counts::Count(creature, 2), 0.0f);
	EXPECT_FALSE(ecs::script_creature_counts::Count(creature, 3).has_value());
}

TEST(ScriptNativesRows6, AClearSetsOnlyThatCountToZero)
{
	ecs::components::Creature creature {};
	creature.timesEaten = 3.0f;
	creature.objectsDestroyed = 7.0f;
	EXPECT_TRUE(ecs::script_creature_counts::Clear(creature, 0));
	EXPECT_EQ(creature.timesEaten, 0.0f);
	EXPECT_EQ(creature.objectsDestroyed, 7.0f);
	EXPECT_TRUE(ecs::script_creature_counts::Clear(creature, 1));
	EXPECT_EQ(creature.objectsDestroyed, 0.0f);
	EXPECT_TRUE(ecs::script_creature_counts::Clear(creature, 2));
	EXPECT_FALSE(ecs::script_creature_counts::Clear(creature, 9));
}

TEST(ScriptNativesRows6, ACreatureInteractsOnlyWithTheTotemOfAStartedTotemStep)
{
	creature_mind::IdleMind mind {};
	mind.agenda.push_back({.kind = creature_mind::Step::Kind::Move, .object = 4});
	mind.agenda.push_back({.kind = creature_mind::Step::Kind::RaiseTotem, .object = 9});
	EXPECT_FALSE(creature_mind::InteractingWith(mind).has_value());
	mind.step = 1;
	EXPECT_FALSE(creature_mind::InteractingWith(mind).has_value());
	mind.stepStarted = true;
	EXPECT_EQ(creature_mind::InteractingWith(mind), 9u);
	mind.agenda[1].kind = creature_mind::Step::Kind::LowerTotem;
	EXPECT_EQ(creature_mind::InteractingWith(mind), 9u);
	mind.step = 0;
	EXPECT_FALSE(creature_mind::InteractingWith(mind).has_value());
	mind.step = 2;
	EXPECT_FALSE(creature_mind::InteractingWith(mind).has_value());
}

TEST(ScriptNativesRows6, ADistinctionTreeHasALeafForEachValueAndOnlyTheValueIsGood)
{
	const auto tree = creature_tree::DistinctionTree(creature_tree::Attribute::Allegiance, 1);
	ASSERT_EQ(tree.nodes.size(), 4u);
	EXPECT_EQ(tree.nodes[0].test, creature_tree::Attribute::Allegiance);
	ASSERT_EQ(tree.nodes[0].children.size(), 3u);
	for (uint32_t value = 0; value < 3; ++value)
	{
		const auto& [branch, node] = tree.nodes[0].children[value];
		EXPECT_EQ(branch, value);
		EXPECT_FALSE(tree.nodes[node].test.has_value());
		EXPECT_EQ(tree.nodes[node].bucket, value == 1 ? creature_tree::k_Buckets.size() - 1 : 0u);
		EXPECT_EQ(tree.nodes[node].examples, 0u);
	}
}

TEST(ScriptNativesRows6, ADistinctionTreeOfAValueOutOfRangeHasOnlyBadLeaves)
{
	const auto tree = creature_tree::DistinctionTree(creature_tree::Attribute::Sex, 5);
	ASSERT_EQ(tree.nodes.size(), 3u);
	EXPECT_TRUE(
	    std::ranges::all_of(tree.nodes.begin() + 1, tree.nodes.end(), [](const auto& node) { return node.bucket == 0; }));
}

TEST(ScriptNativesRows6, TheOnFireAttributeHasTheRangeOfTheDominantDesire)
{
	const auto onFire = creature_tree::k_ScriptValueRanges.at(static_cast<size_t>(creature_tree::Attribute::OnFire));
	const auto desire = creature_tree::k_ScriptValueRanges.at(static_cast<size_t>(creature_tree::Attribute::DominantDesire));
	EXPECT_EQ(onFire, desire);
	EXPECT_EQ(creature_tree::DistinctionTree(creature_tree::Attribute::OnFire, 0).nodes.size(), 42u);
}

TEST(ScriptNativesRows6, LearnEverythingCanKeepOutTheSkillsOrTheMiracles)
{
	auto noSkills = StartKnowledge();
	creature_watching::LearnEverything(noSkills, false, true);
	EXPECT_TRUE(NoneKnown(noSkills.skillsKnown));
	EXPECT_TRUE(AllKnown(noSkills.miraclesKnown));

	auto noMiracles = StartKnowledge();
	creature_watching::LearnEverything(noMiracles, true, false);
	EXPECT_TRUE(AllKnown(noMiracles.skillsKnown));
	EXPECT_TRUE(NoneKnown(noMiracles.miraclesKnown));
}

TEST(ScriptNativesRows6, AWaitForWoodWalkGoesAwayFromTheBuilding)
{
	const glm::vec3 building {100.0f, 5.0f, 100.0f};
	const glm::vec3 villager {103.0f, 5.0f, 104.0f};
	const auto nearest = ecs::villager_work::WaitForWoodPosition(villager, building, 0.0f);
	EXPECT_FLOAT_EQ(nearest.x, 103.0f + 3.0f * 0.4f);
	EXPECT_FLOAT_EQ(nearest.y, 104.0f + 4.0f * 0.4f);
	const auto farthest = ecs::villager_work::WaitForWoodPosition(villager, building, 2.0f);
	EXPECT_FLOAT_EQ(farthest.x, 103.0f + 3.0f * 0.8f);
	EXPECT_FLOAT_EQ(farthest.y, 104.0f + 4.0f * 0.8f);
}

TEST(ScriptNativesRows6, AWaitForWoodWalkCountsTheHeightInTheDistance)
{
	const glm::vec3 building {0.0f, 0.0f, 0.0f};
	const glm::vec3 villager {0.0f, 3.0f, 4.0f};
	const auto goal = ecs::villager_work::WaitForWoodPosition(villager, building, 3.0f);
	EXPECT_FLOAT_EQ(goal.x, 0.0f);
	EXPECT_FLOAT_EQ(goal.y, 4.0f + 4.0f);
}

TEST(ScriptNativesRows6, OnlyWindowsTellsTheButtonsOfTheMouse)
{
	using platform_mouse::System;
	EXPECT_EQ(platform_mouse::SystemMouseButtonsFor(System::Windows, 3), 3u);
	EXPECT_EQ(platform_mouse::SystemMouseButtonsFor(System::Windows, 0), 0u);
	EXPECT_EQ(platform_mouse::SystemMouseButtonsFor(System::Windows, -1), 0u);
	EXPECT_EQ(platform_mouse::SystemMouseButtonsFor(System::Other, 5), 0u);
}
