/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <chrono>

#include <gtest/gtest.h>

#include "ECS/AbodeSounds.h"
#include "Physics/TurnRules.h"

using namespace openblack;
using namespace openblack::ecs;
using audio::SoundAction;

namespace
{
using Milliseconds = std::chrono::duration<float, std::milli>;

// Hours that are easy to check, not the hours of a land
constexpr DayNightTimes k_Times {.nightFull = 4.0f, .duskStart = 6.0f, .duskEnd = 8.0f, .dayFull = 10.0f};
} // namespace

TEST(WorldSounds, AFelledTreeToppledOnceItsUpAxisTipsPastUpright)
{
	EXPECT_FALSE(physics::turn::FelledTreeToppled(1.0f));
	EXPECT_FALSE(physics::turn::FelledTreeToppled(0.98f));
	EXPECT_TRUE(physics::turn::FelledTreeToppled(0.97f));
	EXPECT_TRUE(physics::turn::FelledTreeToppled(0.0f));
}

TEST(WorldSounds, OnlyAFelledTreeTallerThanTenMakesTheSoundOfATreeThatFalls)
{
	EXPECT_FALSE(physics::turn::FelledTreeFallSounds(5.0f));
	EXPECT_FALSE(physics::turn::FelledTreeFallSounds(10.0f));
	EXPECT_TRUE(physics::turn::FelledTreeFallSounds(10.5f));
}

TEST(WorldSounds, TheSkyIsDarkestAtMidnightAndClearAtNoon)
{
	EXPECT_FLOAT_EQ(abode_sounds::Darkness(0.0f, k_Times), 2.0f);
	EXPECT_FLOAT_EQ(abode_sounds::Darkness(5.0f, k_Times), 1.5f);
	EXPECT_FLOAT_EQ(abode_sounds::Darkness(7.0f, k_Times), 1.0f);
	EXPECT_FLOAT_EQ(abode_sounds::Darkness(9.0f, k_Times), 0.5f);
	EXPECT_FLOAT_EQ(abode_sounds::Darkness(12.0f, k_Times), 0.0f);
	// The hours after noon count back from midnight
	EXPECT_FLOAT_EQ(abode_sounds::Darkness(19.0f, k_Times), 1.5f);
	EXPECT_FLOAT_EQ(abode_sounds::Darkness(23.0f, k_Times), 2.0f);
}

TEST(WorldSounds, ABuildingMakesTheSoundOfTheTimeOfDay)
{
	EXPECT_EQ(abode_sounds::ActionAt(0.0f, 12.0f), SoundAction::AbodeDay);
	EXPECT_EQ(abode_sounds::ActionAt(0.69f, 9.0f), SoundAction::AbodeDay);
	EXPECT_EQ(abode_sounds::ActionAt(0.7f, 9.0f), SoundAction::AbodeDawn);
	EXPECT_EQ(abode_sounds::ActionAt(1.29f, 5.0f), SoundAction::AbodeDawn);
	EXPECT_EQ(abode_sounds::ActionAt(1.0f, 12.0f), SoundAction::AbodeDusk);
	EXPECT_EQ(abode_sounds::ActionAt(1.0f, 18.0f), SoundAction::AbodeDusk);
	EXPECT_EQ(abode_sounds::ActionAt(1.3f, 18.0f), SoundAction::AbodeNight);
	EXPECT_EQ(abode_sounds::ActionAt(2.0f, 1.0f), SoundAction::AbodeNight);

	const auto keys = abode_sounds::KeysAt(2.0f, 1.0f);
	EXPECT_EQ(keys.object, audio::SoundObject::Abode);
	EXPECT_EQ(keys.size, audio::SoundSize::None);
	EXPECT_EQ(keys.surface, audio::SoundSurface::None);
}

TEST(WorldSounds, FieldsPitchesDispensersAndTotemsMakeNoBuildingSound)
{
	EXPECT_TRUE(abode_sounds::MakesSounds(AbodeType::LivingQuarters));
	EXPECT_TRUE(abode_sounds::MakesSounds(AbodeType::TownCentre));
	EXPECT_TRUE(abode_sounds::MakesSounds(AbodeType::StoragePit));
	EXPECT_TRUE(abode_sounds::MakesSounds(AbodeType::Creche));
	EXPECT_TRUE(abode_sounds::MakesSounds(AbodeType::Workshop));
	EXPECT_TRUE(abode_sounds::MakesSounds(AbodeType::Graveyard));
	EXPECT_TRUE(abode_sounds::MakesSounds(AbodeType::Wonder));
	EXPECT_FALSE(abode_sounds::MakesSounds(AbodeType::Field));
	EXPECT_FALSE(abode_sounds::MakesSounds(AbodeType::FootballPitch));
	EXPECT_FALSE(abode_sounds::MakesSounds(AbodeType::SpellDispenser));
	EXPECT_FALSE(abode_sounds::MakesSounds(AbodeType::Totem));
}

TEST(WorldSounds, ABuildingSoundsAboutOnceInEightySecondsOfGameTime)
{
	// At 60 frames a second of 16 ms of game time each, a roll of 0 to 4999
	EXPECT_EQ(abode_sounds::Chance(Milliseconds(16.0f)), 5000u);
	EXPECT_EQ(abode_sounds::Chance(Milliseconds(100.0f)), 800u);
	// With no game time, no building sounds
	EXPECT_EQ(abode_sounds::Chance(Milliseconds(0.0f)), 0u);
	EXPECT_EQ(abode_sounds::Chance(Milliseconds(0.0000001f)), 0u);
	// Only a roll of 1 lets it sound
	EXPECT_FALSE(abode_sounds::RollSounds(0));
	EXPECT_TRUE(abode_sounds::RollSounds(1));
	EXPECT_FALSE(abode_sounds::RollSounds(2));
}

TEST(WorldSounds, ABuildingSoundsOnlyWhenTheCameraIsNearerThanTwoHundred)
{
	EXPECT_TRUE(abode_sounds::InReach(0.0f));
	EXPECT_TRUE(abode_sounds::InReach(199.9f));
	EXPECT_FALSE(abode_sounds::InReach(200.0f));
	EXPECT_FALSE(abode_sounds::InReach(500.0f));
}
