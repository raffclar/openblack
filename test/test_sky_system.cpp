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
	EXPECT_FLOAT_EQ(dome.follow.Following(), Clock().GetCurrentSkyType());
	EXPECT_EQ(dome.follow.RowsDone(), 0);
}
