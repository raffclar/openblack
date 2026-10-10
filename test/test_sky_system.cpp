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
#include "ECS/Systems/Implementations/SkySystem.h"

using namespace openblack;
using namespace openblack::ecs;

class SkySystemTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		Locator::entitiesRegistry::emplace<Registry>();
		_sky = std::make_unique<systems::SkySystem>();
	}
	void TearDown() override
	{
		_sky.reset();
		Locator::entitiesRegistry::reset();
	}

	[[nodiscard]] static Registry& Entities() { return Locator::entitiesRegistry::value(); }

	std::unique_ptr<systems::SkySystem> _sky;
};

TEST_F(SkySystemTest, TheSkyStartsAtNoonWithItsThreeEntities)
{
	EXPECT_TRUE((Entities().AllOf<components::SkyDome, components::DayNightCycle>(_sky->GetDome())));
	EXPECT_TRUE((Entities().AllOf<components::Sun, components::CelestialBody>(_sky->GetSun())));
	EXPECT_TRUE((Entities().AllOf<components::Moon, components::CelestialBody, components::CelestialGlow>(_sky->GetMoon())));
	EXPECT_FLOAT_EQ(_sky->GetClock().GetScriptTime(), 12.0f);
	EXPECT_FLOAT_EQ(_sky->GetCurrentSkyType(), 2.0f);
}

TEST_F(SkySystemTest, SettingTheTimeBuildsTheWholeDomeAgain)
{
	_sky->SetTime(0.0f);
	EXPECT_FLOAT_EQ(_sky->GetClock().GetScriptTime(), 0.0f);
	const auto& dome = Entities().Get<components::SkyDome>(_sky->GetDome());
	EXPECT_FLOAT_EQ(dome.follow.Following(), _sky->GetCurrentSkyType());
	EXPECT_EQ(dome.follow.RowsDone(), 0);
}

TEST_F(SkySystemTest, TheSkyGoesOnAsItWasIntoTheNextLand)
{
	_sky->GetClock().SetCycle(600.0f, 0.2f, 0.1f);
	_sky->SetTime(20.0f);
	_sky->ProcessTurn();
	const auto before = _sky->GetClock().GetVisualTime();
	const auto times = _sky->GetDayNightTimes();

	_sky->KeepForNextLand();
	Entities().Reset();
	_sky->Initialize();

	EXPECT_TRUE(Entities().Valid(_sky->GetDome()));
	EXPECT_FLOAT_EQ(_sky->GetClock().GetVisualTime(), before);
	EXPECT_FLOAT_EQ(_sky->GetDayNightTimes().duskStart, times.duskStart);
	EXPECT_FLOAT_EQ(_sky->GetDayNightTimes().dayFull, times.dayFull);
}

TEST_F(SkySystemTest, ATurnMovesTheClockOn)
{
	const auto before = _sky->GetClock().GetVisualTime();
	_sky->ProcessTurn();
	EXPECT_GT(_sky->GetClock().GetVisualTime(), before);
	_sky->GetClock().SetRunning(false);
	const auto stopped = _sky->GetClock().GetVisualTime();
	_sky->ProcessTurn();
	EXPECT_FLOAT_EQ(_sky->GetClock().GetVisualTime(), stopped);
}
