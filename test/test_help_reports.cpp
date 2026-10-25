/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include <cstdint>

#include <memory>
#include <optional>
#include <tuple>
#include <vector>

#include <entt/meta/context.hpp>
#include <gtest/gtest.h>

#include "Commands/PlayerCommands.h"
#include "Common/GameRandom.h"
#include "ECS/Reflection/BinaryArchive.h"
#include "ECS/Reflection/ComponentReflection.h"
#include "ECS/Systems/Implementations/HelpMessageSystem.h"
#include "ECS/Systems/Implementations/HelpProfileSystem.h"
#include "Help/HelpMessages.h"
#include "Help/HelpProfile.h"

using namespace openblack;
using namespace openblack::ecs::systems;
namespace profile = openblack::help::profile;
namespace reflection = openblack::ecs::reflection;

namespace
{

const entt::meta_ctx& Context()
{
	static const auto context = []() {
		auto made = std::make_unique<entt::meta_ctx>();
		reflection::RegisterComponents(*made);
		return made;
	}();
	return *context;
}

/// A world camera that sees the land near and looks steeply down
constexpr profile::WorldCameraView k_NearAndSteep {.centreDistance = 10.0f, .pitch = 0.8f};

} // namespace

TEST(HelpReports, AnEventGoesOnceIntoTheNextReportAndThenGoes)
{
	profile::ReportedEvents events;
	events.Add(profile::k_MouseMoved);
	events.Add(profile::k_MouseMoved);
	events.Add(profile::k_CameraDrag);
	// An event past the last is not kept
	events.Add(profile::k_EventCount);
	const auto bits = events.Take();
	EXPECT_EQ(bits, (uint64_t {1} << profile::k_MouseMoved) | (uint64_t {1} << profile::k_CameraDrag));
	EXPECT_EQ(events.Take(), 0u);
	EXPECT_EQ(profile::EventsOf(bits), (std::vector<uint32_t> {profile::k_MouseMoved, profile::k_CameraDrag}));
	// The bits past the last event give no event
	EXPECT_TRUE(profile::EventsOf(uint64_t {1} << 63).empty());
}

TEST(HelpReports, TheInputOfThePlayerCountsOnlyAtTheTurnThatDoesTheReport)
{
	HelpProfileSystem help;
	help.Report(profile::k_MouseMoved);
	help.Report(profile::k_CameraTurn);
	// The input of the frames does not count yet
	EXPECT_EQ(help.Get().Count(profile::k_MouseMoved).Total(), 0u);
	help.ProcessTurn();
	EXPECT_EQ(help.Get().Count(profile::k_MouseMoved).Total(), 0u);

	const auto report = help.MakeReport(false, std::nullopt, false);
	ASSERT_TRUE(report.has_value());
	EXPECT_EQ(help.Get().Count(profile::k_MouseMoved).Total(), 0u);
	help.DoReport(*report);
	EXPECT_EQ(help.Get().Count(profile::k_MouseMoved).Total(), 1u);
	EXPECT_EQ(help.Get().Count(profile::k_CameraTurn).Total(), 1u);
	// The events also count as the event for any help and the event for any event
	EXPECT_EQ(help.Get().Count(help_messages::k_AnyHelpEvent).Total(), 1u);
	EXPECT_EQ(help.Get().Count(profile::k_AnyEvent).Total(), 1u);
}

TEST(HelpReports, AReportGoesOnlyWhenItHasEventsOrItsValuesChange)
{
	HelpProfileSystem help;
	// The first report has the values of a profile with no report: nothing to send
	EXPECT_FALSE(help.MakeReport(false, std::nullopt, false).has_value());
	const auto first = help.MakeReport(false, k_NearAndSteep, true);
	ASSERT_TRUE(first.has_value());
	EXPECT_TRUE(first->cameraSeesLand);
	EXPECT_TRUE(first->cameraNearAndSteep);
	EXPECT_FALSE(first->cameraSeesSky);
	EXPECT_TRUE(first->cursorOnPoint);
	EXPECT_EQ(first->events, 0u);
	// The same values again: no report
	EXPECT_FALSE(help.MakeReport(false, k_NearAndSteep, true).has_value());
	// A camera that moves but gives the same events of the view: no report
	EXPECT_FALSE(help.MakeReport(false, profile::WorldCameraView {.centreDistance = 12.0f, .pitch = 0.9f}, true).has_value());
	// A camera that looks up at the sky: a report
	const auto sky = help.MakeReport(false, profile::WorldCameraView {.centreDistance = std::nullopt, .pitch = -0.5f}, true);
	ASSERT_TRUE(sky.has_value());
	EXPECT_TRUE(sky->cameraSeesSky);
	EXPECT_FALSE(sky->cameraSeesLand);
	// An event: a report with the same values
	help.Report(profile::k_TapThing);
	const auto withEvent = help.MakeReport(false, k_NearAndSteep, true);
	ASSERT_TRUE(withEvent.has_value());
	EXPECT_EQ(withEvent->events, uint64_t {1} << profile::k_TapThing);
	// A front-end box: a report
	EXPECT_TRUE(help.MakeReport(true, k_NearAndSteep, true).has_value());
}

TEST(HelpReports, EachTurnCountsTheViewOfTheLastReport)
{
	HelpProfileSystem help;
	const auto report = help.MakeReport(false, k_NearAndSteep, false);
	ASSERT_TRUE(report.has_value());
	// Before the turn that does the report, the turns count no view
	help.ProcessTurn();
	EXPECT_EQ(help.Get().Count(profile::k_CameraSeesLand).Total(), 0u);
	help.DoReport(*report);
	help.ProcessTurn();
	help.ProcessTurn();
	EXPECT_EQ(help.Get().Count(profile::k_CameraSeesLand).Total(), 2u);
	EXPECT_EQ(help.Get().Count(profile::k_CameraNearAndSteep).Total(), 2u);
	EXPECT_EQ(help.Get().Clock(), 3 * profile::k_MillisecondsPerTurn);

	// A report with a front-end box stops the count and the clock until the next report
	help.DoReport(player_commands::HelpReport {.boxShows = true});
	help.ProcessTurn();
	EXPECT_EQ(help.Get().Count(profile::k_CameraSeesLand).Total(), 2u);
	EXPECT_EQ(help.Get().Clock(), 3 * profile::k_MillisecondsPerTurn);
	// A camera that does not show the world gives no view
	help.DoReport(player_commands::HelpReport {});
	help.ProcessTurn();
	EXPECT_EQ(help.Get().Count(profile::k_CameraSeesLand).Total(), 2u);
	EXPECT_EQ(help.Get().Clock(), 4 * profile::k_MillisecondsPerTurn);
	EXPECT_FALSE(help.GetLastReport().boxShows);
}

TEST(HelpReports, TheRandomNumbersOfTheHelpComeFromTheirOwnSeed)
{
	HelpMessageSystem help;
	// The draws are those of the game's generator on a seed that starts as the game's seeds start
	uint32_t seed = game_random::k_InitialSeed;
	for (int32_t n : {7, 100, 1, 15})
	{
		EXPECT_EQ(help.HelpRand(n), game_random::LHRand(static_cast<uint32_t>(n), seed));
	}
	// 0 gives 0 and does not draw
	const auto before = seed;
	EXPECT_EQ(help.HelpRand(0), 0u);
	EXPECT_EQ(help.HelpFloatRand(2.0f), game_random::FloatRand(2.0f, seed));
	EXPECT_NE(seed, before);
}

TEST(HelpReports, ASavedGameKeepsTheSeedOfTheHelp)
{
	HelpMessageSystem saved;
	std::ignore = saved.HelpRand(1000);
	std::ignore = saved.HelpRand(1000);
	reflection::BinaryWriter out(Context());
	saved.SaveState(out);
	const auto bytes = out.Bytes();

	HelpMessageSystem loaded;
	const reflection::EntityRemap identity;
	reflection::BinaryReader in(Context(), bytes, identity);
	ASSERT_TRUE(loaded.LoadState(in));
	EXPECT_EQ(in.Remaining(), 0u);
	EXPECT_EQ(loaded.HelpRand(1000), saved.HelpRand(1000));

	// A save of an older build ends before the seed: the load reads it, and the seed stays as it was
	const std::vector<uint8_t> older(bytes.begin(), bytes.end() - sizeof(uint32_t));
	HelpMessageSystem fromOlder;
	reflection::BinaryReader olderIn(Context(), older, identity);
	ASSERT_TRUE(fromOlder.LoadState(olderIn));
	HelpMessageSystem fresh;
	EXPECT_EQ(fromOlder.HelpRand(1000), fresh.HelpRand(1000));
}
