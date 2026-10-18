/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstddef>

#include <ECS/AlignmentHistory.h>
#include <ECS/Components/AlignmentHistory.h>
#include <gtest/gtest.h>

namespace
{
namespace rules = openblack::ecs::alignment_history;
using openblack::ecs::components::AlignmentCause;
using openblack::ecs::components::AlignmentHistory;

constexpr auto k_Script = static_cast<std::size_t>(AlignmentCause::Script);
constexpr auto k_Tree = static_cast<std::size_t>(AlignmentCause::Tree);
} // namespace

TEST(AlignmentHistory, ScriptChangeCountsAsItIs)
{
	AlignmentHistory history;
	rules::Add(history, AlignmentCause::Script, 0.25f, 3.0f);
	rules::Add(history, AlignmentCause::Script, -0.5f, 3.0f);
	EXPECT_FLOAT_EQ(history.totals.at(k_Script), -0.25f);
	EXPECT_EQ(history.counts.at(k_Script), 2);
}

TEST(AlignmentHistory, OtherCausesUseThePlayerScale)
{
	AlignmentHistory history;
	rules::Add(history, AlignmentCause::Tree, 0.25f, 2.0f);
	EXPECT_FLOAT_EQ(history.totals.at(k_Tree), 0.5f);
	EXPECT_EQ(history.counts.at(k_Tree), 1);
	EXPECT_EQ(history.counts.at(k_Script), 0);
}

TEST(AlignmentHistory, ZeroChangeDoesNotCount)
{
	AlignmentHistory history;
	rules::Add(history, AlignmentCause::Script, 0.0f, 1.0f);
	rules::Add(history, AlignmentCause::Tree, 0.0f, 1.0f);
	EXPECT_EQ(history.counts.at(k_Script), 0);
	EXPECT_EQ(history.counts.at(k_Tree), 0);
}

TEST(AlignmentHistory, LabelsMatchTheCauses)
{
	EXPECT_EQ(rules::k_Labels.at(k_Script), "Script  ");
	EXPECT_EQ(rules::k_Labels.at(k_Tree), "Tree    ");
}
