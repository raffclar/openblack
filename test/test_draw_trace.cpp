/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The draw trace is a developer tool: a build without the developer tools has nothing to test
#if defined(OPENBLACK_DEVELOPER_TOOLS)

#define LOCATOR_IMPLEMENTATIONS

#include <cstdint>

#include <memory>
#include <source_location>
#include <sstream>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "Common/DrawTrace.h"
#include "Common/GameRandomProduction.h"

using namespace openblack;
using namespace openblack::draw_trace;

namespace
{

/// The lines that a trace wrote
std::vector<std::string> LinesOf(const std::stringstream& stream)
{
	std::vector<std::string> lines;
	std::istringstream in(stream.str());
	for (std::string line; std::getline(in, line);)
	{
		lines.push_back(line);
	}
	return lines;
}

/// A trace that writes to a string stream, and the stream to read it back
struct StringTrace
{
	std::shared_ptr<DrawTrace> trace;
	std::stringstream* stream {nullptr};
};

StringTrace MakeTrace()
{
	auto stream = std::make_unique<std::stringstream>();
	auto* raw = stream.get();
	return {.trace = std::make_shared<DrawTrace>(std::move(stream)), .stream = raw};
}

/// The file and line of a site, as the trace writes them
std::string WhereOf(const std::source_location& site)
{
	return ShortFile(site.file_name()) + ":" + std::to_string(site.line());
}

} // namespace

TEST(DrawTrace, ShortFileKeepsThePathFromTheSourceFolder)
{
	EXPECT_EQ(ShortFile("E:\\work\\openblack\\src\\Common\\GameRandom.cpp"), "src/Common/GameRandom.cpp");
	EXPECT_EQ(ShortFile("/home/a/src/x/src/ECS/Forest.cpp"), "src/ECS/Forest.cpp");
	EXPECT_EQ(ShortFile("test\\test_draw_trace.cpp"), "test/test_draw_trace.cpp");
}

TEST(DrawTrace, DrawLineHasTheTurnPhaseIndexAndPlace)
{
	const auto site = std::source_location::current();
	const auto line = DrawLine(42, Phase::Frame, 7, site);
	EXPECT_EQ(line.rfind("draw\t42\tframe\t7\t" + WhereOf(site) + "\t", 0), 0u) << line;
}

TEST(DrawTrace, TurnScopeSetsThePhaseOfTheDraws)
{
	auto [trace, stream] = MakeTrace();
	const auto site = std::source_location::current();
	trace->Draw(1, site);
	{
		const TurnScope scope(trace.get(), 5);
		EXPECT_EQ(trace->CurrentPhase(), Phase::Turn);
		trace->Draw(2, site);
	}
	EXPECT_EQ(trace->CurrentPhase(), Phase::Frame);
	trace->Draw(3, site);
	const auto lines = LinesOf(*stream);
	ASSERT_EQ(lines.size(), 3u);
	EXPECT_EQ(lines[0].rfind("draw\t0\tframe\t1\t", 0), 0u) << lines[0];
	EXPECT_EQ(lines[1].rfind("draw\t5\tturn\t2\t", 0), 0u) << lines[1];
	// A draw after the turn keeps the number of the last turn
	EXPECT_EQ(lines[2].rfind("draw\t5\tframe\t3\t", 0), 0u) << lines[2];
}

TEST(DrawTrace, TurnScopeWithNoTraceDoesNothing)
{
	const TurnScope scope(nullptr, 3);
	SUCCEED();
}

TEST(DrawTrace, GeneratorWritesEachSyncedDrawWithThePlaceOfItsCall)
{
	auto [trace, stream] = MakeTrace();
	GameRandomProduction random;
	random.SetDrawTrace(trace);
	ASSERT_EQ(random.GetDrawTrace(), trace.get());

	const auto first = std::source_location::current();
	random.GameRand(10);
	const auto second = std::source_location::current();
	random.GameFloatRand(2.0f);
	// No draw: a range of 0, a float of 0, and the local seed
	random.GameRand(0);
	random.GameFloatRand(0.0f);
	random.LocalRand(10);
	random.LocalFloatRand(1.0f);

	const auto lines = LinesOf(*stream);
	ASSERT_EQ(lines.size(), 2u);
	// Each call is on the line after its site
	const auto file = ShortFile(first.file_name());
	EXPECT_NE(lines[0].find("\t1\t" + file + ":" + std::to_string(first.line() + 1) + "\t"), std::string::npos) << lines[0];
	EXPECT_NE(lines[1].find("\t2\t" + file + ":" + std::to_string(second.line() + 1) + "\t"), std::string::npos) << lines[1];
	EXPECT_EQ(random.GetSyncedDraws(), 2u);
}

TEST(DrawTrace, ParticleDrawsShowTheCallerOfTheParticleFunction)
{
	auto [trace, stream] = MakeTrace();
	GameRandomProduction random;
	random.SetDrawTrace(trace);
	{
		const ParticleRandomStep step(random, true);
		const auto site = std::source_location::current();
		random.ParticleRand(4);
		const auto lines = LinesOf(*stream);
		ASSERT_EQ(lines.size(), 1u);
		EXPECT_NE(lines[0].find(":" + std::to_string(site.line() + 1) + "\t"), std::string::npos) << lines[0];
	}
	{
		// A local effect draws on the local seed, which the trace does not show
		const ParticleRandomStep step(random, false);
		random.ParticleRand(4);
		random.ParticleFloatRand(1.0f);
	}
	EXPECT_EQ(LinesOf(*stream).size(), 1u);
}

TEST(DrawTrace, GeneratorWritesRestartsAndLoadedCounts)
{
	auto [trace, stream] = MakeTrace();
	GameRandomProduction random;
	random.SetDrawTrace(trace);
	random.GameRand(3);
	random.Restart();
	random.SetSyncedDraws(12);
	const auto lines = LinesOf(*stream);
	ASSERT_EQ(lines.size(), 3u);
	EXPECT_EQ(lines[1], "restart");
	EXPECT_EQ(lines[2], "set\t12");
	// The restart keeps the trace
	EXPECT_EQ(random.GetDrawTrace(), trace.get());
}

TEST(DrawTrace, NoTraceKeepsTheSameDraws)
{
	auto [trace, stream] = MakeTrace();
	GameRandomProduction traced;
	traced.SetDrawTrace(trace);
	GameRandomProduction plain;
	for (uint32_t n = 1; n < 50; ++n)
	{
		EXPECT_EQ(traced.GameRand(n), plain.GameRand(n));
		EXPECT_EQ(traced.GameFloatRand(static_cast<float>(n)), plain.GameFloatRand(static_cast<float>(n)));
	}
	EXPECT_EQ(traced.GetSeeds(), plain.GetSeeds());
	EXPECT_EQ(traced.GetSyncedDraws(), plain.GetSyncedDraws());
	traced.SetDrawTrace(nullptr);
	traced.GameRand(5);
	EXPECT_EQ(LinesOf(*stream).size(), 98u);
}

#endif
