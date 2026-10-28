/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <latch>
#include <memory>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include "Profiler.h"

using openblack::Profiler;

namespace
{

const Profiler::Scope& StageOf(const Profiler& profiler, Profiler::Stage stage, int8_t offset = 0)
{
	return profiler.GetEntries().at(profiler.GetEntryIndex(offset)).stages.at(static_cast<size_t>(stage));
}

} // namespace

TEST(ProfilerThreads, TheMainThreadsScopesGoIntoTheEntryAtOnce)
{
	auto profiler = std::make_unique<Profiler>();
	profiler->Frame();
	{
		auto logic = profiler->BeginScoped(Profiler::Stage::GameLogic);
		auto minds = profiler->BeginScoped(Profiler::Stage::CreatureMindUpdate);
	}
	const auto& logic = StageOf(*profiler, Profiler::Stage::GameLogic);
	const auto& minds = StageOf(*profiler, Profiler::Stage::CreatureMindUpdate);
	EXPECT_TRUE(logic.finalized);
	EXPECT_EQ(logic.calls, 1U);
	EXPECT_EQ(logic.level, 0U);
	EXPECT_EQ(minds.level, 1U);
}

TEST(ProfilerThreads, TheScopesOfAnotherThreadWaitUntilTheMainThreadMergesThem)
{
	auto profiler = std::make_unique<Profiler>();
	profiler->Frame();
	std::thread worker([&profiler] {
		auto logic = profiler->BeginScoped(Profiler::Stage::GameLogic);
		auto minds = profiler->BeginScoped(Profiler::Stage::CreatureMindUpdate);
	});
	worker.join();
	EXPECT_EQ(StageOf(*profiler, Profiler::Stage::GameLogic).calls, 0U);
	// The main thread merges them inside its own open scope, one level below it
	{
		auto turns = profiler->BeginScoped(Profiler::Stage::UpdateEntities);
		profiler->MergeThreads();
	}
	const auto& logic = StageOf(*profiler, Profiler::Stage::GameLogic);
	const auto& minds = StageOf(*profiler, Profiler::Stage::CreatureMindUpdate);
	EXPECT_TRUE(logic.finalized);
	EXPECT_EQ(logic.calls, 1U);
	EXPECT_EQ(logic.level, 1U);
	EXPECT_EQ(minds.calls, 1U);
	EXPECT_EQ(minds.level, 2U);
	EXPECT_LE(logic.start, minds.start);
	EXPECT_LE(minds.end, logic.end);
	EXPECT_GE(logic.total, minds.total);
	// A second merge finds nothing more
	profiler->MergeThreads();
	EXPECT_EQ(StageOf(*profiler, Profiler::Stage::GameLogic).calls, 1U);
}

TEST(ProfilerThreads, TheNextFrameTakesTheScopesIntoTheEntryOfTheFrameThatEnds)
{
	auto profiler = std::make_unique<Profiler>();
	profiler->Frame();
	std::thread worker([&profiler] { auto pose = profiler->BeginScoped(Profiler::Stage::CreatureTurnPose); });
	worker.join();
	profiler->Frame();
	EXPECT_EQ(StageOf(*profiler, Profiler::Stage::CreatureTurnPose, -1).calls, 1U);
	EXPECT_EQ(StageOf(*profiler, Profiler::Stage::CreatureTurnPose, 0).calls, 0U);
}

TEST(ProfilerThreads, AScopeThatIsStillOpenStaysWithItsThread)
{
	auto profiler = std::make_unique<Profiler>();
	profiler->Frame();
	std::latch opened(1);
	std::latch merged(1);
	std::thread worker([&] {
		auto logic = profiler->BeginScoped(Profiler::Stage::GameLogic);
		opened.count_down();
		merged.wait();
	});
	opened.wait();
	profiler->MergeThreads();
	EXPECT_EQ(StageOf(*profiler, Profiler::Stage::GameLogic).calls, 0U);
	merged.count_down();
	worker.join();
	profiler->MergeThreads();
	EXPECT_EQ(StageOf(*profiler, Profiler::Stage::GameLogic).calls, 1U);
}

TEST(ProfilerThreads, ManyThreadsAtOnceLoseNoScope)
{
	auto profiler = std::make_unique<Profiler>();
	profiler->Frame();
	constexpr int k_Threads = 4;
	constexpr int k_Scopes = 500;
	std::vector<std::thread> workers;
	for (int t = 0; t < k_Threads; ++t)
	{
		workers.emplace_back([&profiler] {
			for (int i = 0; i < k_Scopes; ++i)
			{
				auto fire = profiler->BeginScoped(Profiler::Stage::FireUpdate);
			}
		});
	}
	// The main thread merges while the threads still close scopes
	for (int i = 0; i < 20; ++i)
	{
		profiler->MergeThreads();
	}
	for (auto& worker : workers)
	{
		worker.join();
	}
	profiler->MergeThreads();
	EXPECT_EQ(StageOf(*profiler, Profiler::Stage::FireUpdate).calls, static_cast<uint16_t>(k_Threads * k_Scopes));
}
