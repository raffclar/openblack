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

/// A profiler whose clock times the stages from its first frame, as when a tool reads them
std::unique_ptr<Profiler> TimedProfiler()
{
	auto profiler = std::make_unique<Profiler>();
	profiler->SetReader(Profiler::Reader::FrameStats, true);
	profiler->Frame();
	return profiler;
}

} // namespace

TEST(ProfilerThreads, TheMainThreadsScopesGoIntoTheEntryAtOnce)
{
	auto profiler = TimedProfiler();
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
	auto profiler = TimedProfiler();
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
	auto profiler = TimedProfiler();
	std::thread worker([&profiler] { auto pose = profiler->BeginScoped(Profiler::Stage::CreatureTurnPose); });
	worker.join();
	profiler->Frame();
	EXPECT_EQ(StageOf(*profiler, Profiler::Stage::CreatureTurnPose, -1).calls, 1U);
	EXPECT_EQ(StageOf(*profiler, Profiler::Stage::CreatureTurnPose, 0).calls, 0U);
}

TEST(ProfilerThreads, AScopeThatIsStillOpenStaysWithItsThread)
{
	auto profiler = TimedProfiler();
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
	auto profiler = TimedProfiler();
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

TEST(ProfilerThreads, TheClockDoesNotTimeTheStagesWhenNoToolReadsThem)
{
	auto profiler = std::make_unique<Profiler>();
	profiler->Frame();
	EXPECT_FALSE(profiler->TimesStages());
	{
		auto logic = profiler->BeginScoped(Profiler::Stage::GameLogic);
	}
	std::thread worker([&profiler] { auto pose = profiler->BeginScoped(Profiler::Stage::CreatureTurnPose); });
	worker.join();
	profiler->MergeThreads();
	EXPECT_FALSE(profiler->GetEntries().at(profiler->GetEntryIndex(0)).stagesTimed);
	EXPECT_EQ(StageOf(*profiler, Profiler::Stage::GameLogic).calls, 0U);
	EXPECT_FALSE(StageOf(*profiler, Profiler::Stage::GameLogic).finalized);
	EXPECT_EQ(StageOf(*profiler, Profiler::Stage::CreatureTurnPose).calls, 0U);
	// The frame times stay correct without a reader
	profiler->Frame();
	const auto& last = profiler->GetEntries().at(profiler->GetEntryIndex(-1));
	EXPECT_LE(last.frameStart, last.frameEnd);
	EXPECT_NE(last.frameEnd.time_since_epoch().count(), 0);
}

TEST(ProfilerThreads, AReaderGetsTheTimesFromTheNextFrame)
{
	auto profiler = std::make_unique<Profiler>();
	profiler->Frame();
	profiler->SetReader(Profiler::Reader::DebugWindow, true);
	// The frame that started before the reader has no stage times, not a part of them
	{
		auto logic = profiler->BeginScoped(Profiler::Stage::GameLogic);
	}
	EXPECT_EQ(StageOf(*profiler, Profiler::Stage::GameLogic).calls, 0U);
	profiler->Frame();
	EXPECT_TRUE(profiler->TimesStages());
	EXPECT_TRUE(profiler->GetEntries().at(profiler->GetEntryIndex(0)).stagesTimed);
	{
		auto logic = profiler->BeginScoped(Profiler::Stage::GameLogic);
	}
	EXPECT_EQ(StageOf(*profiler, Profiler::Stage::GameLogic).calls, 1U);
	EXPECT_TRUE(StageOf(*profiler, Profiler::Stage::GameLogic).finalized);
}

TEST(ProfilerThreads, TheClockRunsWhileAnyReaderReads)
{
	auto profiler = std::make_unique<Profiler>();
	profiler->SetReader(Profiler::Reader::FrameStats, true);
	profiler->SetReader(Profiler::Reader::RunRecord, true);
	profiler->Frame();
	EXPECT_TRUE(profiler->TimesStages());
	profiler->SetReader(Profiler::Reader::FrameStats, false);
	profiler->Frame();
	EXPECT_TRUE(profiler->TimesStages());
	profiler->SetReader(Profiler::Reader::RunRecord, false);
	// The stop starts at the next frame too
	EXPECT_TRUE(profiler->TimesStages());
	profiler->Frame();
	EXPECT_FALSE(profiler->TimesStages());
}

TEST(ProfilerThreads, TheToolOptionKeepsTheClockOffWhileToolsRead)
{
	auto profiler = std::make_unique<Profiler>();
	profiler->SetReader(Profiler::Reader::Benchmark, true);
	profiler->SetStageClockOff(true);
	profiler->Frame();
	EXPECT_FALSE(profiler->TimesStages());
	{
		auto logic = profiler->BeginScoped(Profiler::Stage::GameLogic);
	}
	EXPECT_EQ(StageOf(*profiler, Profiler::Stage::GameLogic).calls, 0U);
	profiler->SetStageClockOff(false);
	profiler->Frame();
	EXPECT_TRUE(profiler->TimesStages());
}

TEST(ProfilerThreads, AScopeThatStartedTimedEndsTimedAfterTheReaderStops)
{
	auto profiler = TimedProfiler();
	std::latch opened(1);
	std::latch stopped(1);
	std::thread worker([&] {
		{
			auto logic = profiler->BeginScoped(Profiler::Stage::GameLogic);
			opened.count_down();
			stopped.wait();
		}
		// The next scope of the thread is again at the top level
		auto minds = profiler->BeginScoped(Profiler::Stage::CreatureMindUpdate);
	});
	opened.wait();
	profiler->SetReader(Profiler::Reader::FrameStats, false);
	profiler->Frame();
	EXPECT_FALSE(profiler->TimesStages());
	stopped.count_down();
	worker.join();
	profiler->SetReader(Profiler::Reader::FrameStats, true);
	profiler->Frame();
	// The scope that started timed closed and went into the frame that had no stage times
	EXPECT_EQ(StageOf(*profiler, Profiler::Stage::GameLogic, -1).calls, 1U);
	EXPECT_EQ(StageOf(*profiler, Profiler::Stage::CreatureMindUpdate, -1).calls, 0U);
	std::thread next([&profiler] { auto pose = profiler->BeginScoped(Profiler::Stage::CreatureTurnPose); });
	next.join();
	profiler->MergeThreads();
	EXPECT_EQ(StageOf(*profiler, Profiler::Stage::CreatureTurnPose).level, 0U);
}

TEST(ProfilerThreads, AScopeThatStartedUntimedStaysUntimedAfterTheReaderStarts)
{
	auto profiler = std::make_unique<Profiler>();
	profiler->Frame();
	std::latch opened(1);
	std::latch started(1);
	std::thread worker([&] {
		{
			auto logic = profiler->BeginScoped(Profiler::Stage::GameLogic);
			opened.count_down();
			started.wait();
		}
		auto pose = profiler->BeginScoped(Profiler::Stage::CreatureTurnPose);
	});
	opened.wait();
	profiler->SetReader(Profiler::Reader::FrameStats, true);
	profiler->Frame();
	started.count_down();
	worker.join();
	profiler->MergeThreads();
	EXPECT_EQ(StageOf(*profiler, Profiler::Stage::GameLogic).calls, 0U);
	// The thread has no open scope left over, so its next scope is at the top level
	EXPECT_EQ(StageOf(*profiler, Profiler::Stage::CreatureTurnPose).calls, 1U);
	EXPECT_EQ(StageOf(*profiler, Profiler::Stage::CreatureTurnPose).level, 0U);
}
