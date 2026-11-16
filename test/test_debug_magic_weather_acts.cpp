/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <limits>
#include <optional>
#include <string>
#include <vector>

#include <fmt/format.h>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "Common/DebugActs.h"
#include "Common/DebugMagicWeatherActs.h"

using namespace openblack;
using namespace openblack::debug_acts;
using player_commands::DebugMagic;
using player_commands::DebugSky;
using player_commands::DebugTownMiracle;
using player_commands::DebugWeather;

namespace
{

/// A fake world of miracles: it writes each call as a line
class FakeMagic final: public MagicTarget
{
public:
	void SetIgnoreInfluence(bool ignore) override { calls.push_back(fmt::format("ignore {}", ignore)); }
	[[nodiscard]] bool HasSeed(MagicType type) const override { return type != MagicType::None && type != noSeed; }
	void PlaceDispenser(glm::vec3 position, MagicType type) override
	{
		calls.push_back(fmt::format("dispenser {} {} {} {}", static_cast<int>(type), position.x, position.y, position.z));
	}
	void PlaceBubble(glm::vec3 position, MagicType type) override
	{
		calls.push_back(fmt::format("bubble {} {} {} {}", static_cast<int>(type), position.x, position.y, position.z));
	}
	void GiveToHand(PlayerNames player, MagicType type) override
	{
		calls.push_back(fmt::format("give {} {}", static_cast<int>(player), static_cast<int>(type)));
	}
	void SummonFromWorship(PlayerNames player, MagicType type) override
	{
		calls.push_back(fmt::format("summon {} {}", static_cast<int>(player), static_cast<int>(type)));
	}
	void CastAtPoint(PlayerNames player, MagicType type, glm::vec3 hand, glm::vec3 target, glm::vec3 forward) override
	{
		calls.push_back(fmt::format("cast {} {} hand {} target {} forward {}", static_cast<int>(player), static_cast<int>(type),
		                            hand.y, target.y, forward.z));
	}
	[[nodiscard]] bool IsCreature(entt::entity entity) const override { return entity == creature; }
	void CastOnCreature(PlayerNames player, MagicType type, entt::entity target, glm::vec3 hand, glm::vec3 /*forward*/) override
	{
		calls.push_back(fmt::format("cast on {} {} {} hand {}", static_cast<int>(player), static_cast<int>(type),
		                            entt::to_integral(target), hand.y));
	}
	void DropSeed(PlayerNames player) override { calls.push_back(fmt::format("drop {}", static_cast<int>(player))); }
	bool SetPrayerPower(PlayerNames player, float chants) override
	{
		calls.push_back(fmt::format("prayer {} {}", static_cast<int>(player), chants));
		return player != noPrayer;
	}
	bool SetInfinitePrayer(PlayerNames player, bool infinite) override
	{
		calls.push_back(fmt::format("infinite {} {}", static_cast<int>(player), infinite));
		return player != noPrayer;
	}

	std::vector<std::string> calls;
	MagicType noSeed {MagicType::None};
	entt::entity creature {static_cast<entt::entity>(5)};
	PlayerNames noPrayer {PlayerNames::NEUTRAL};
};

class FakeWeather final: public WeatherTarget
{
public:
	void ClearStorms() override { calls.emplace_back("clear"); }
	void SetStormCreation(bool enabled) override { calls.push_back(fmt::format("creation {}", enabled)); }
	void SetClimates(bool enabled) override { calls.push_back(fmt::format("climates {}", enabled)); }
	void ForceStorm(const ecs::systems::ForcedStorm& storm) override
	{
		forced = storm;
		calls.emplace_back("force");
	}
	void StrikeLightning(bool bolt) override { calls.push_back(fmt::format("lightning {}", bolt)); }
	void MeltSnow() override { calls.emplace_back("melt"); }
	[[nodiscard]] bool IsStorm(entt::entity entity) const override { return entity == storm; }
	void EndStorm(entt::entity entity) override { calls.push_back(fmt::format("end {}", entt::to_integral(entity))); }

	std::vector<std::string> calls;
	std::optional<ecs::systems::ForcedStorm> forced;
	entt::entity storm {static_cast<entt::entity>(9)};
};

class FakeSky final: public SkyTarget
{
public:
	void SetScriptHour(float value) override { hour = value; }
	void SetClockRuns(bool value) override { runs = value; }
	void SetMoonDate(std::optional<int64_t> value) override { date = value; }

	float hour {-1.0f};
	bool runs {false};
	std::optional<int64_t> date {0};
};

} // namespace

TEST(DebugMagicWeatherActs, EachButtonOfTheMagicWindowActsForThePlayerOfTheCommand)
{
	FakeMagic magic;
	const glm::vec3 point {10.0f, 4.0f, 20.0f};
	const auto act = [&](DebugMagic::Act kind) {
		return ApplyMagicAct(magic, PlayerNames::PLAYER_TWO,
		                     DebugMagic {.act = kind, .magic = 3, .position = point, .forward = {0.0f, 0.0f, 1.0f}});
	};
	EXPECT_FALSE(act(DebugMagic::Act::PlaceDispenser).has_value());
	EXPECT_FALSE(act(DebugMagic::Act::PlaceBubble).has_value());
	EXPECT_FALSE(act(DebugMagic::Act::GiveToHand).has_value());
	EXPECT_FALSE(act(DebugMagic::Act::SummonFromWorship).has_value());
	EXPECT_FALSE(act(DebugMagic::Act::CastAtPoint).has_value());
	EXPECT_FALSE(act(DebugMagic::Act::DropSeed).has_value());
	EXPECT_FALSE(ApplyMagicAct(magic, PlayerNames::PLAYER_TWO, DebugMagic {.act = DebugMagic::Act::IgnoreInfluence, .on = true})
	                 .has_value());
	EXPECT_FALSE(
	    ApplyMagicAct(magic, PlayerNames::PLAYER_TWO, DebugMagic {.act = DebugMagic::Act::PrayerPower, .value = 5000.0f})
	        .has_value());
	EXPECT_FALSE(ApplyMagicAct(magic, PlayerNames::PLAYER_TWO, DebugMagic {.act = DebugMagic::Act::InfinitePrayer, .on = true})
	                 .has_value());
	// The bubble comes 3 above the point, and the hand casts from 15 above it
	const std::vector<std::string> expected {"dispenser 3 10 4 20",
	                                         "bubble 3 10 7 20",
	                                         "give 1 3",
	                                         "summon 1 3",
	                                         "cast 1 3 hand 19 target 4 forward 1",
	                                         "drop 1",
	                                         "ignore true",
	                                         "prayer 1 5000",
	                                         "infinite 1 true"};
	EXPECT_EQ(magic.calls, expected);
}

TEST(DebugMagicWeatherActs, TheMagicWindowRefusesAMiracleWithNoSeedOrACastOnAThingThatIsNoCreature)
{
	FakeMagic magic;
	magic.noSeed = static_cast<MagicType>(4);
	const auto refusal = [&](DebugMagic act) { return ApplyMagicAct(magic, PlayerNames::PLAYER_ONE, act); };
	EXPECT_TRUE(refusal({.act = DebugMagic::Act::GiveToHand, .magic = 4}).has_value());
	EXPECT_TRUE(refusal({.act = DebugMagic::Act::SummonFromWorship, .magic = 4}).has_value());
	// A dispenser and a cast do not need a seed, as in the window
	EXPECT_FALSE(refusal({.act = DebugMagic::Act::PlaceDispenser, .magic = 4}).has_value());
	EXPECT_TRUE(
	    refusal({.act = DebugMagic::Act::CastOnCreature, .magic = 4, .creature = static_cast<entt::entity>(6)}).has_value());
	EXPECT_FALSE(
	    refusal({.act = DebugMagic::Act::CastOnCreature, .magic = 4, .creature = static_cast<entt::entity>(5)}).has_value());
	magic.noPrayer = PlayerNames::PLAYER_ONE;
	EXPECT_TRUE(refusal({.act = DebugMagic::Act::PrayerPower, .value = 1.0f}).has_value());
	const std::vector<std::string> expected {"dispenser 4 0 0 0", "cast on 0 4 5 hand 15", "prayer 0 1"};
	EXPECT_EQ(magic.calls, expected);
}

TEST(DebugMagicWeatherActs, TheValuesOfAnActMustBeInTheLimitsOfTheWindows)
{
	EXPECT_TRUE(MagicActRefusal({.act = DebugMagic::Act::PlaceBubble, .magic = 0}).has_value());
	EXPECT_TRUE(MagicActRefusal({.act = DebugMagic::Act::PlaceBubble, .magic = 42}).has_value());
	EXPECT_FALSE(MagicActRefusal({.act = DebugMagic::Act::PlaceBubble, .magic = 41}).has_value());
	// The switches and the prayer power need no miracle
	EXPECT_FALSE(MagicActRefusal({.act = DebugMagic::Act::DropSeed}).has_value());
	EXPECT_TRUE(
	    MagicActRefusal({.act = DebugMagic::Act::PrayerPower, .value = std::numeric_limits<float>::infinity()}).has_value());
	EXPECT_TRUE(MagicActRefusal({.act = static_cast<DebugMagic::Act>(10)}).has_value());
	EXPECT_TRUE(TownMiracleRefusal({.magic = 0}).has_value());
	EXPECT_FALSE(TownMiracleRefusal({.magic = 1}).has_value());

	const DebugWeather storm {.act = DebugWeather::Act::ForceStorm, .temperature = 45, .rain = 100, .windX = -100};
	EXPECT_FALSE(WeatherActRefusal(storm).has_value());
	auto hot = storm;
	hot.temperature = 46;
	EXPECT_TRUE(WeatherActRefusal(hot).has_value());
	auto brief = storm;
	brief.seconds = 9.0f;
	EXPECT_TRUE(WeatherActRefusal(brief).has_value());
	auto flash = storm;
	flash.boltWait = {2.0f, 121.0f};
	EXPECT_TRUE(WeatherActRefusal(flash).has_value());
	// An act with no storm does not read the values of a storm
	EXPECT_FALSE(WeatherActRefusal({.act = DebugWeather::Act::Bolt, .temperature = 100}).has_value());

	EXPECT_FALSE(SkyActRefusal({.act = DebugSky::Act::ScriptHour, .hour = 24.0f}).has_value());
	EXPECT_TRUE(SkyActRefusal({.act = DebugSky::Act::ScriptHour, .hour = -0.5f}).has_value());
	EXPECT_FALSE(SkyActRefusal({.act = DebugSky::Act::ClockRuns, .hour = -0.5f}).has_value());

	// The game server checks the same refusals
	EXPECT_TRUE(RefusalOf(DebugMagic {.act = DebugMagic::Act::PlaceBubble, .magic = 0}).has_value());
	EXPECT_TRUE(RefusalOf(DebugTownMiracle {.magic = 0}).has_value());
	EXPECT_TRUE(RefusalOf(hot).has_value());
	EXPECT_TRUE(RefusalOf(DebugSky {.hour = 30.0f}).has_value());
	EXPECT_FALSE(RefusalOf(storm).has_value());
}

TEST(DebugMagicWeatherActs, APresetClearsTheStormsAndStopsTheClimatesThenItsStormComes)
{
	FakeWeather weather;
	EXPECT_FALSE(ApplyWeatherAct(weather, {.act = DebugWeather::Act::Preset}).has_value());
	const std::vector<std::string> clear {"clear", "creation false"};
	EXPECT_EQ(weather.calls, clear);
	EXPECT_FALSE(weather.forced.has_value());

	weather.calls.clear();
	const DebugWeather snow {.act = DebugWeather::Act::Preset,
	                         .temperature = -15,
	                         .snow = 100,
	                         .overcast = 100,
	                         .windX = 20,
	                         .windZ = -50,
	                         .seconds = 300.0f,
	                         .fadeSeconds = 2.0f,
	                         .thunderWait = {3.0f, 15.0f},
	                         .boltWait = {2.0f, 8.0f},
	                         .cloudHeight = 100.0f,
	                         .rainSpeed = 0.5f};
	EXPECT_FALSE(ApplyWeatherAct(weather, snow).has_value());
	const std::vector<std::string> snowed {"clear", "creation false", "force"};
	EXPECT_EQ(weather.calls, snowed);
	ASSERT_TRUE(weather.forced.has_value());
	EXPECT_EQ(weather.forced->effect.temperature, -15);
	EXPECT_EQ(weather.forced->effect.snow, 100);
	EXPECT_EQ(weather.forced->effect.windZ, -50);
	EXPECT_EQ(weather.forced->effect.snowCover, 0);
	EXPECT_EQ(weather.forced->seconds, 300.0f);
	EXPECT_EQ(weather.forced->thunderWait, glm::vec2(3.0f, 15.0f));
	EXPECT_EQ(weather.forced->boltWait, glm::vec2(2.0f, 8.0f));
	EXPECT_EQ(weather.forced->cloudHeight, 100.0f);
	EXPECT_EQ(weather.forced->rainSpeed, 0.5f);
	// The window does not change the count of the clouds
	EXPECT_EQ(weather.forced->cloudCount, ecs::systems::ForcedStorm {}.cloudCount);
}

TEST(DebugMagicWeatherActs, EachButtonOfTheWeatherWindowDoesItsAct)
{
	FakeWeather weather;
	for (const auto act :
	     {DebugWeather::Act::ClearStorms, DebugWeather::Act::Bolt, DebugWeather::Act::Thunder, DebugWeather::Act::MeltSnow})
	{
		EXPECT_FALSE(ApplyWeatherAct(weather, {.act = act}).has_value());
	}
	EXPECT_FALSE(ApplyWeatherAct(weather, {.act = DebugWeather::Act::Climates, .on = true}).has_value());
	EXPECT_FALSE(ApplyWeatherAct(weather, {.act = DebugWeather::Act::ClimateStorms, .on = false}).has_value());
	EXPECT_FALSE(
	    ApplyWeatherAct(weather, {.act = DebugWeather::Act::EndStorm, .storm = static_cast<entt::entity>(9)}).has_value());
	// A storm that is gone ends on no computer
	EXPECT_TRUE(
	    ApplyWeatherAct(weather, {.act = DebugWeather::Act::EndStorm, .storm = static_cast<entt::entity>(8)}).has_value());
	const std::vector<std::string> expected {"clear",          "lightning true", "lightning false", "melt", "climates true",
	                                         "creation false", "end 9"};
	EXPECT_EQ(weather.calls, expected);
}

TEST(DebugMagicWeatherActs, TheMoonWindowSetsTheClockAndTheDateOfTheMoon)
{
	FakeSky sky;
	EXPECT_FALSE(ApplySkyAct(sky, {.act = DebugSky::Act::ScriptHour, .hour = 19.5f}).has_value());
	EXPECT_EQ(sky.hour, 19.5f);
	EXPECT_FALSE(ApplySkyAct(sky, {.act = DebugSky::Act::ClockRuns, .on = false}).has_value());
	EXPECT_FALSE(sky.runs);
	EXPECT_FALSE(ApplySkyAct(sky, {.act = DebugSky::Act::MoonDate, .date = -86400}).has_value());
	EXPECT_EQ(sky.date, std::optional<int64_t>(-86400));
	EXPECT_FALSE(ApplySkyAct(sky, {.act = DebugSky::Act::ComputerDate}).has_value());
	EXPECT_FALSE(sky.date.has_value());
	sky.date = 5;
	EXPECT_FALSE(ApplySkyAct(sky, {.act = DebugSky::Act::Reset}).has_value());
	EXPECT_FALSE(sky.date.has_value());
	EXPECT_TRUE(sky.runs);
	EXPECT_TRUE(ApplySkyAct(sky, {.act = DebugSky::Act::ScriptHour, .hour = 24.5f}).has_value());
	EXPECT_EQ(sky.hour, 19.5f);
}

TEST(DebugMagicWeatherActs, EachActHasAName)
{
	for (uint8_t i = 0; i <= static_cast<uint8_t>(DebugMagic::Act::InfinitePrayer); ++i)
	{
		EXPECT_NE(NameOf(static_cast<DebugMagic::Act>(i)), "unknown") << static_cast<int>(i);
	}
	for (uint8_t i = 0; i <= static_cast<uint8_t>(DebugWeather::Act::EndStorm); ++i)
	{
		EXPECT_NE(NameOf(static_cast<DebugWeather::Act>(i)), "unknown") << static_cast<int>(i);
	}
	for (uint8_t i = 0; i <= static_cast<uint8_t>(DebugSky::Act::Reset); ++i)
	{
		EXPECT_NE(NameOf(static_cast<DebugSky::Act>(i)), "unknown") << static_cast<int>(i);
	}
}
