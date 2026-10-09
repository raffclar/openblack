/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include <chrono>

#include <gtest/gtest.h>

#include "ECS/Systems/Implementations/MoonSystem.h"
#include "Graphics/Moon.h"

using namespace std::chrono_literals;
using openblack::ecs::systems::MoonFrame;
using openblack::ecs::systems::MoonSystem;
namespace moon = openblack::graphics::moon;

namespace
{
constexpr int64_t k_NewMoon = moon::k_NewMoonDay * moon::k_SecondsPerDay;
/// About a full moon: 15 days on
constexpr int64_t k_FullMoon = k_NewMoon + (15 * moon::k_SecondsPerDay);

MoonFrame Night(int64_t wallClock, std::chrono::milliseconds realTime = 10s)
{
	return {.scriptHour = 0.0f, .overcast = 0.0f, .fog = true, .realTime = realTime, .wallClock = wallClock};
}
} // namespace

TEST(MoonSystem, ShowsAtNightWithThePhaseOfTheDate)
{
	MoonSystem system;
	system.Update(Night(k_FullMoon));
	ASSERT_TRUE(system.GetPlacement().has_value());
	EXPECT_FLOAT_EQ(system.GetStrength(), 200.0f);
	EXPECT_FLOAT_EQ(system.GetPhase(), moon::Phase(k_FullMoon));
	EXPECT_FLOAT_EQ(system.GetShownPhase(), system.GetPhase());
	EXPECT_FLOAT_EQ(system.GetScriptPercentage(), moon::ScriptPercentage(moon::Phase(k_FullMoon)));
	EXPECT_LT(system.GetScriptPercentage(), 0.05f);
}

TEST(MoonSystem, ScriptsHearOfThePhaseItLastShowedWith)
{
	MoonSystem system;
	// Before it has ever shown, scripts are told 1
	auto day = Night(k_FullMoon);
	day.scriptHour = 12.0f;
	system.Update(day);
	EXPECT_FALSE(system.GetPlacement().has_value());
	EXPECT_FLOAT_EQ(system.GetScriptPercentage(), 1.0f);

	system.Update(Night(k_FullMoon));
	const float shown = system.GetScriptPercentage();
	// By day on another date the phase moves on, but what scripts are told waits for the moon to show again
	system.SetDateOverride(k_NewMoon + (7 * moon::k_SecondsPerDay));
	system.Update(day);
	EXPECT_NE(system.GetPhase(), system.GetShownPhase());
	EXPECT_FLOAT_EQ(system.GetScriptPercentage(), shown);
}

TEST(MoonSystem, AThickOvercastHidesItAndKeepsTheShownPhase)
{
	MoonSystem system;
	system.Update(Night(k_FullMoon));
	const float shown = system.GetShownPhase();
	system.SetDateOverride(k_NewMoon);
	// Just after it rises it is faint, and a full overcast with the fog option takes it to nothing
	auto frame = Night(k_FullMoon);
	frame.scriptHour = 19.35f;
	frame.overcast = 1.0f;
	system.Update(frame);
	ASSERT_TRUE(system.GetPlacement().has_value());
	EXPECT_FLOAT_EQ(system.GetStrength(), 0.0f);
	EXPECT_FLOAT_EQ(system.GetShownPhase(), shown);
	// Without the fog option the overcast doesn't dim it
	frame.fog = false;
	system.Update(frame);
	EXPECT_GT(system.GetStrength(), 0.0f);
	EXPECT_FLOAT_EQ(system.GetShownPhase(), moon::Phase(k_NewMoon));
}

TEST(MoonSystem, ReadsTheDateAtMostEveryTwoSeconds)
{
	MoonSystem system;
	system.Update(Night(k_NewMoon, 10s));
	EXPECT_EQ(system.GetDate(), k_NewMoon);
	// Two seconds on, the date read before is kept
	system.Update(Night(k_FullMoon, 12s));
	EXPECT_EQ(system.GetDate(), k_NewMoon);
	// After more than two seconds it is read again
	system.Update(Night(k_FullMoon, 12001ms));
	EXPECT_EQ(system.GetDate(), k_FullMoon);
}

TEST(MoonSystem, AnOverriddenDateComesFirst)
{
	MoonSystem system;
	system.Update(Night(k_NewMoon));
	system.SetDateOverride(k_FullMoon);
	EXPECT_EQ(system.GetDate(), k_FullMoon);
	EXPECT_FLOAT_EQ(system.GetPhase(), moon::Phase(k_FullMoon));
	system.Update(Night(k_NewMoon, 20s));
	EXPECT_EQ(system.GetDate(), k_FullMoon);
	system.SetDateOverride(std::nullopt);
	EXPECT_EQ(system.GetDate(), k_NewMoon);
}
