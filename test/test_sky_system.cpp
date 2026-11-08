/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <memory>

#include <gtest/gtest.h>

#include "ECS/Components/Sky.h"
#include "ECS/Registry.h"
#include "Locator.h"

#define LOCATOR_IMPLEMENTATIONS
#include "ECS/Systems/Implementations/SkyClockSystem.h"
#include "ECS/Systems/Implementations/SkySystem.h"

using namespace openblack;
using namespace openblack::ecs;

namespace
{

/// The clock of the sky alone, as the dedicated server has it: no system shows the sky
class SkyClockSystemTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		Locator::entitiesRegistry::emplace<Registry>();
		_clock = std::make_unique<systems::SkyClockSystem>();
	}
	void TearDown() override
	{
		_clock.reset();
		Locator::entitiesRegistry::reset();
	}

	[[nodiscard]] static Registry& Entities() { return Locator::entitiesRegistry::value(); }

	std::unique_ptr<systems::SkyClockSystem> _clock;
};

/// The clock of the sky and the sky that a computer shows. The shown sky finds the clock through the locator.
class SkySystemTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		Locator::entitiesRegistry::emplace<Registry>();
		Locator::skyClockSystem::emplace<systems::SkyClockSystem>();
		Locator::skySystem::emplace<systems::SkySystem>();
	}
	void TearDown() override
	{
		Locator::skySystem::reset();
		Locator::skyClockSystem::reset();
		Locator::entitiesRegistry::reset();
	}

	[[nodiscard]] static Registry& Entities() { return Locator::entitiesRegistry::value(); }
	[[nodiscard]] static systems::SkyClockSystemInterface& Clock() { return Locator::skyClockSystem::value(); }
	[[nodiscard]] static systems::SkySystemInterface& Sky() { return Locator::skySystem::value(); }
};

} // namespace

TEST_F(SkyClockSystemTest, TheSkyStartsAtNoonWithItsThreeEntities)
{
	const auto& entities = _clock->GetEntities();
	EXPECT_TRUE((Entities().AllOf<components::SkyDome, components::DayNightCycle>(entities.dome)));
	EXPECT_TRUE((Entities().AllOf<components::Sun, components::CelestialBody>(entities.sun)));
	EXPECT_TRUE((Entities().AllOf<components::Moon, components::CelestialBody, components::CelestialGlow>(entities.moon)));
	EXPECT_FLOAT_EQ(_clock->GetClock().GetScriptTime(), 12.0f);
	EXPECT_FLOAT_EQ(_clock->GetCurrentSkyType(), 2.0f);
	EXPECT_FLOAT_EQ(_clock->GetCurrentNightSkyType(), 0.0f);
}

TEST_F(SkyClockSystemTest, WithNoShownSkyTheClockJumpsAndTheDomeStaysAsItWas)
{
	const auto before = Entities().Get<components::SkyDome>(_clock->GetEntities().dome).follow.Following();
	_clock->SetTime(0.0f);
	EXPECT_FLOAT_EQ(_clock->GetClock().GetScriptTime(), 0.0f);
	EXPECT_FLOAT_EQ(Entities().Get<components::SkyDome>(_clock->GetEntities().dome).follow.Following(), before);
}

TEST_F(SkyClockSystemTest, TheSkyGoesOnAsItWasIntoTheNextLand)
{
	_clock->GetClock().SetCycle(600.0f, 0.2f, 0.1f);
	_clock->SetTime(20.0f);
	_clock->ProcessTurn();
	const auto before = _clock->GetClock().GetVisualTime();
	const auto times = _clock->GetDayNightTimes();

	_clock->KeepForNextLand();
	Entities().Reset();
	_clock->Initialize();

	EXPECT_TRUE(Entities().Valid(_clock->GetEntities().dome));
	EXPECT_FLOAT_EQ(_clock->GetClock().GetVisualTime(), before);
	EXPECT_FLOAT_EQ(_clock->GetDayNightTimes().duskStart, times.duskStart);
	EXPECT_FLOAT_EQ(_clock->GetDayNightTimes().dayFull, times.dayFull);
}

// The system keeps the numbers of the cycle that it sets, also into the next land, for a map save
TEST_F(SkyClockSystemTest, TheNumbersOfTheCycleStayForAMapSave)
{
	EXPECT_EQ(_clock->GetCycle().duration, DayNightClock::k_DefaultDuration);
	EXPECT_EQ(_clock->GetCycle().night, DayNightClock::k_DefaultNight);
	EXPECT_EQ(_clock->GetCycle().change, DayNightClock::k_DefaultChange);

	_clock->SetCycle({.duration = 1000.0f, .night = 0.23f, .change = 0.17f});
	DayNightClock expected;
	expected.SetCycle(1000.0f, 0.23f, 0.17f);
	EXPECT_EQ(_clock->GetDayNightTimes().duskStart, expected.GetDayNightTimes().duskStart);

	_clock->KeepForNextLand();
	Entities().Reset();
	_clock->Initialize();
	EXPECT_EQ(_clock->GetCycle().duration, 1000.0f);
	EXPECT_EQ(_clock->GetCycle().night, 0.23f);
	EXPECT_EQ(_clock->GetCycle().change, 0.17f);
}

TEST_F(SkyClockSystemTest, ATurnMovesTheClockOn)
{
	const auto before = _clock->GetClock().GetVisualTime();
	_clock->ProcessTurn();
	EXPECT_GT(_clock->GetClock().GetVisualTime(), before);
	_clock->GetClock().SetRunning(false);
	const auto stopped = _clock->GetClock().GetVisualTime();
	_clock->ProcessTurn();
	EXPECT_FLOAT_EQ(_clock->GetClock().GetVisualTime(), stopped);
}

TEST_F(SkyClockSystemTest, AfterALoadTheClockFindsTheSkysEntities)
{
	const auto entities = _clock->GetEntities();
	_clock->FindAfterLoad();
	EXPECT_EQ(_clock->GetEntities().dome, entities.dome);
	EXPECT_EQ(_clock->GetEntities().sun, entities.sun);
	EXPECT_EQ(_clock->GetEntities().moon, entities.moon);
}

TEST_F(SkySystemTest, TheShownSkyIsTheEntitiesOfTheClock)
{
	EXPECT_EQ(Sky().GetDome(), Clock().GetEntities().dome);
	EXPECT_EQ(Sky().GetSun(), Clock().GetEntities().sun);
	EXPECT_EQ(Sky().GetMoon(), Clock().GetEntities().moon);
}

TEST_F(SkySystemTest, SettingTheTimeBuildsTheWholeDomeAgain)
{
	Clock().SetTime(0.0f);
	EXPECT_FLOAT_EQ(Clock().GetClock().GetScriptTime(), 0.0f);
	const auto& dome = Entities().Get<components::SkyDome>(Sky().GetDome());
	EXPECT_FLOAT_EQ(dome.follow.Following(), Clock().GetCurrentNightSkyType());
	EXPECT_EQ(dome.follow.RowsDone(), 0);
}

// A saved world keeps how far the dome of the computer that saved followed the sky, but not the dome's picture. The
// renderer keeps its picture through the load, so the dome continues as it was before the load.
TEST_F(SkyClockSystemTest, ALoadKeepsHowFarTheShownDomeFollowsTheSky)
{
	auto& shown = Entities().Get<components::SkyDome>(_clock->GetEntities().dome);
	// The first frame builds all of the dome, then the dome follows the sky down to the night
	static_cast<void>(shown.follow.Advance(0.0f));
	static_cast<void>(shown.follow.Advance(1.5f));
	const auto before = shown.follow;
	ASSERT_EQ(before.RowsDone(), sky_dome::k_RowsPerFrame);

	_clock->KeepShownDome();
	Entities().Reset();
	// The saved dome had all of its picture built for the day
	const auto loaded = archetypes::SkyArchetype::Create();
	auto& saved = Entities().Get<components::SkyDome>(loaded.dome);
	static_cast<void>(saved.follow.Advance(0.0f));
	_clock->FindAfterLoad();

	const auto& dome = Entities().Get<components::SkyDome>(_clock->GetEntities().dome);
	EXPECT_FLOAT_EQ(dome.follow.Following(), before.Following());
	EXPECT_EQ(dome.follow.RowsDone(), before.RowsDone());
}

// With no dome before the load, the renderer has no picture of it: the dome builds all of its picture again
TEST_F(SkyClockSystemTest, ALoadWithNoShownDomeBuildsTheWholeDome)
{
	Entities().Reset();
	const auto loaded = archetypes::SkyArchetype::Create();
	auto& saved = Entities().Get<components::SkyDome>(loaded.dome);
	static_cast<void>(saved.follow.Advance(0.0f));
	_clock->FindAfterLoad();

	auto& dome = Entities().Get<components::SkyDome>(_clock->GetEntities().dome);
	const auto frame = dome.follow.Advance(_clock->GetCurrentNightSkyType());
	ASSERT_GE(frame.count, 1);
	EXPECT_EQ(frame.rows.at(0).first, 0);
	EXPECT_EQ(frame.rows.at(0).count, sky_dome::k_Rows);
}
