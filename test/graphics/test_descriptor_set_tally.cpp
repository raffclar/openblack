/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstddef>
#include <cstdint>

#include <array>
#include <vector>

#include <gtest/gtest.h>

#include "Graphics/DescriptorSetTally.h"
#include "Graphics/DrawEncoder.h"

using namespace openblack::graphics;

namespace
{

constexpr uint16_t k_Main = 13;
constexpr uint16_t k_Translucent = 14;
constexpr uint16_t k_Land = 7;
constexpr uint16_t k_Hole = 7;
constexpr uint16_t k_White = 1;
constexpr uint8_t k_HoleStage = 6;
constexpr uint32_t k_Blocks = 64;

/// The depth of the n-th block in a grid of 8 by 8, seen from above a corner of the middle: its distance squared, so
/// that the blocks submitted row by row come out of the sort in rings around the camera
uint32_t RingDepth(uint32_t block)
{
	const auto dx = static_cast<int32_t>(block % 8) * 2 - 7;
	const auto dz = static_cast<int32_t>(block / 8) * 2 - 7;
	return static_cast<uint32_t>((dx * dx) + (dz * dz)) * 1000u + block;
}

/// The land's blocks submitted row by row, keeping their bindings, with the block at holeBlock binding a texture the
/// others don't. everyBlockBinds: each block binds its hole stage itself.
FrameDescriptorSets LandWithHole(uint32_t holeBlock, bool everyBlockBinds)
{
	DescriptorSetTally tally;
	tally.SetTexture(k_HoleStage, k_White);
	for (uint32_t block = 0; block < k_Blocks; ++block)
	{
		if (everyBlockBinds || block == holeBlock)
		{
			tally.SetTexture(k_HoleStage, everyBlockBinds ? k_Hole : (block == holeBlock ? k_Hole : k_White));
		}
		tally.Submit(k_Main, k_Land, RingDepth(block), false);
	}
	return tally.EndFrame();
}

TEST(DescriptorSetTally, DrawsSharingTheirBindingsShareOneSet)
{
	DescriptorSetTally tally;
	tally.SetTexture(0, 5);
	for (uint32_t i = 0; i < 100; ++i)
	{
		tally.Submit(k_Main, 3, 1000 - i, false);
	}
	const auto frame = tally.EndFrame();
	EXPECT_EQ(frame.draws, 100u);
	EXPECT_EQ(frame.sets, 1u);
	ASSERT_EQ(frame.views.size(), 1u);
	EXPECT_EQ(frame.views[0].view, k_Main);
}

TEST(DescriptorSetTally, ATextureBoundPartWayThroughSortedDrawsAlternates)
{
	// Half the blocks keep the white texture, the rest the hole that the middle block left bound, and the sort by
	// distance mixes the two halves
	const auto leftOver = LandWithHole(k_Blocks / 2, false);
	EXPECT_GT(leftOver.sets, k_Blocks / 4);
	// Every block binding the same textures takes one set, wherever the hole is
	const auto bound = LandWithHole(k_Blocks / 2, true);
	EXPECT_EQ(bound.sets, 1u);
}

TEST(DescriptorSetTally, ASequentialViewKeepsTheSubmittedOrder)
{
	DescriptorSetTally tally;
	tally.SetViewOrder(k_Main, ViewOrder::Sequential);
	tally.SetTexture(k_HoleStage, k_White);
	for (uint32_t block = 0; block < k_Blocks; ++block)
	{
		if (block == k_Blocks / 2)
		{
			tally.SetTexture(k_HoleStage, k_Hole);
		}
		tally.Submit(k_Main, k_Land, RingDepth(block), false);
	}
	EXPECT_EQ(tally.EndFrame().sets, 2u);
}

TEST(DescriptorSetTally, ADepthSortedViewMixesPrograms)
{
	DescriptorSetTally tally;
	tally.SetViewOrder(k_Translucent, ViewOrder::DepthDescending);
	// Two programs drawn in turn by distance: each change of program takes a set
	for (uint32_t i = 0; i < 10; ++i)
	{
		tally.Submit(k_Translucent, static_cast<uint16_t>(1 + (i % 2)), i, false);
	}
	EXPECT_EQ(tally.EndFrame().sets, 10u);

	// The same draws in a view sorted by program take a set for each program
	tally.SetViewOrder(k_Translucent, ViewOrder::ByProgram);
	for (uint32_t i = 0; i < 10; ++i)
	{
		tally.Submit(k_Translucent, static_cast<uint16_t>(1 + (i % 2)), i, false);
	}
	EXPECT_EQ(tally.EndFrame().sets, 2u);
}

TEST(DescriptorSetTally, DroppedBindingsAreNotCarriedOn)
{
	DescriptorSetTally tally;
	tally.SetTexture(0, 9);
	tally.Submit(k_Main, 3, 1, true);
	tally.Submit(k_Main, 3, 2, false);
	// The second draw had nothing bound
	EXPECT_EQ(tally.EndFrame().sets, 2u);

	tally.SetTexture(0, 9);
	tally.DiscardBindings();
	tally.Submit(k_Main, 3, 1, false);
	tally.Submit(k_Main, 3, 2, false);
	EXPECT_EQ(tally.EndFrame().sets, 1u);
}

TEST(DescriptorSetTally, EachFrameStartsAfresh)
{
	DescriptorSetTally tally;
	tally.SetTexture(0, 9);
	tally.Submit(k_Main, 3, 1, false);
	ASSERT_EQ(tally.EndFrame().sets, 1u);
	const auto empty = tally.EndFrame();
	EXPECT_EQ(empty.draws, 0u);
	EXPECT_EQ(empty.sets, 0u);
	EXPECT_TRUE(empty.views.empty());
}

TEST(DescriptorSetTally, TheSetCarriesOverFromViewToView)
{
	DescriptorSetTally tally;
	tally.SetTexture(0, 9);
	tally.Submit(k_Main, 3, 1, false);
	tally.Submit(k_Translucent, 3, 1, false);
	const auto frame = tally.EndFrame();
	EXPECT_EQ(frame.sets, 1u);
	ASSERT_EQ(frame.views.size(), 2u);
	EXPECT_EQ(frame.views[0].sets, 1u);
	EXPECT_EQ(frame.views[1].sets, 0u);
	EXPECT_EQ(frame.views[1].draws, 1u);
}

TEST(DescriptorSetTally, InOrderCountsTheSubmittedOrder)
{
	const auto frame = LandWithHole(k_Blocks / 2, false);
	ASSERT_EQ(frame.views.size(), 1u);
	// White up to the hole, then the hole left bound
	EXPECT_EQ(frame.views[0].setsInOrder, 2u);
}

/// The draws of the jobs of some threads, each job from no bindings: job j binds texture j at stage 0 for its first
/// draw only, then draws three times keeping its bindings, in a sequential view and in a view sorted by program
void DrawJob(DescriptorSetTally& tally, uint16_t job)
{
	tally.SetTexture(0, job);
	tally.Submit(k_Main, 3, 0, false);
	tally.Submit(k_Translucent, static_cast<uint16_t>(1 + (job % 2)), 5, false);
	tally.Submit(k_Main, 3, 0, true);
}

TEST(DescriptorSetTally, AppendedTalliesCountAsTheDrawsInOneTally)
{
	constexpr uint16_t k_Jobs = 5;
	DescriptorSetTally one;
	one.SetViewOrder(k_Main, ViewOrder::Sequential);
	for (uint16_t job = 0; job < k_Jobs; ++job)
	{
		one.DiscardBindings();
		DrawJob(one, job);
	}
	const auto expected = one.EndFrame();

	// Job 0 on the main tally, the others on tallies of their own, put after it in their order
	DescriptorSetTally main;
	main.SetViewOrder(k_Main, ViewOrder::Sequential);
	std::array<DescriptorSetTally, k_Jobs - 1> others;
	DrawJob(main, 0);
	for (uint16_t job = k_Jobs - 1; job > 0; --job)
	{
		DrawJob(others.at(job - 1), job);
	}
	for (auto& other : others)
	{
		main.Append(other);
	}
	const auto frame = main.EndFrame();
	EXPECT_EQ(frame.draws, expected.draws);
	EXPECT_EQ(frame.sets, expected.sets);
	ASSERT_EQ(frame.views.size(), expected.views.size());
	for (size_t i = 0; i < frame.views.size(); ++i)
	{
		EXPECT_EQ(frame.views[i].view, expected.views[i].view);
		EXPECT_EQ(frame.views[i].draws, expected.views[i].draws);
		EXPECT_EQ(frame.views[i].sets, expected.views[i].sets);
		EXPECT_EQ(frame.views[i].setsInOrder, expected.views[i].setsInOrder);
	}
	// An appended tally is empty after it
	EXPECT_EQ(others.front().EndFrame().draws, 0u);
}

TEST(DescriptorSetRelief, AViewThatAlternatesWhenSortedIsDrawnInOrder)
{
	const FrameDescriptorSets frame {
	    .draws = 2100,
	    .sets = 1782,
	    .views = {{.view = 9, .draws = 1024, .sets = 878, .setsInOrder = 2},
	              {.view = k_Main, .draws = 1025, .sets = 879, .setsInOrder = 3},
	              {.view = k_Translucent, .draws = 51, .sets = 25, .setsInOrder = 3}},
	};
	const std::array<uint16_t, 2> reorderable {k_Main, 9};
	const auto relief = PlanDescriptorSetRelief(frame, reorderable);
	EXPECT_EQ(relief.inOrder, (std::vector<uint16_t> {9, k_Main}));
	// What is left is well within the budget: the reflection stays
	EXPECT_FALSE(relief.skipOptional);
}

TEST(DescriptorSetRelief, TexturesThatReallyDifferSkipTheOptionalView)
{
	// Every draw binds textures of its own: the order doesn't matter
	const FrameDescriptorSets frame {
	    .draws = 1000,
	    .sets = 1000,
	    .views = {{.view = 9, .draws = 450, .sets = 450, .setsInOrder = 450},
	              {.view = k_Main, .draws = 550, .sets = 550, .setsInOrder = 550}},
	};
	const std::array<uint16_t, 2> reorderable {k_Main, 9};
	const auto relief = PlanDescriptorSetRelief(frame, reorderable);
	EXPECT_TRUE(relief.inOrder.empty());
	EXPECT_TRUE(relief.skipOptional);
}

TEST(DescriptorSetRelief, OnlyTheViewsThatMayBeReorderedAre)
{
	const FrameDescriptorSets frame {
	    .draws = 1000,
	    .sets = 1000,
	    .views = {{.view = k_Translucent, .draws = 1000, .sets = 1000, .setsInOrder = 2}},
	};
	const std::array<uint16_t, 1> reorderable {k_Main};
	EXPECT_TRUE(PlanDescriptorSetRelief(frame, reorderable).inOrder.empty());
}

TEST(DescriptorSetTally, TheBudgetWarnsWellBeforeThePoolRunsDry)
{
	EXPECT_EQ(JudgeDescriptorSets(0), DescriptorBudget::Fine);
	EXPECT_EQ(JudgeDescriptorSets(k_DescriptorSetsNear - 1), DescriptorBudget::Fine);
	EXPECT_EQ(JudgeDescriptorSets(k_DescriptorSetsNear), DescriptorBudget::Near);
	EXPECT_EQ(JudgeDescriptorSets(k_DescriptorSetsOver), DescriptorBudget::Over);
	static_assert(k_DescriptorSetsNear < k_DescriptorSetsOver);
	static_assert(k_DescriptorSetsOver < k_DescriptorSetsPerFrame);
}

TEST(BoundStages, BindingsStayUntilADiscardDropsThem)
{
	BoundStages stages;
	stages.Set(0);
	stages.Set(5);
	EXPECT_TRUE(stages.IsSet(0));
	EXPECT_TRUE(stages.IsSet(5));
	EXPECT_FALSE(stages.IsSet(1));
	// A discard that keeps the bindings keeps the stages
	stages.Discarded(0);
	EXPECT_TRUE(stages.IsSet(5));
	stages.Discarded(BoundStages::k_DiscardBindings);
	EXPECT_FALSE(stages.IsSet(0));
	EXPECT_FALSE(stages.IsSet(5));
	stages.Set(3);
	stages.Clear();
	EXPECT_FALSE(stages.IsSet(3));
}

TEST(BoundStages, AStageOutOfRangeIsNeverSet)
{
	BoundStages stages;
	stages.Set(255);
	EXPECT_FALSE(stages.IsSet(255));
}

} // namespace
