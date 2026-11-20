/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>

#include <array>
#include <limits>
#include <string>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "Common/DebugActs.h"

using namespace openblack;
using namespace openblack::debug_acts;
using player_commands::DebugAdvisor;
using player_commands::DebugConsoleLine;
using player_commands::DebugWorld;

namespace
{

/// The screen of a fake controller of the advisors
struct FakeScreen
{
	uint16_t width {800};
	uint16_t height {600};
};

/// An advisor of the fake controller: it keeps the last emotion
struct FakeDude
{
	uint32_t emotion {0};
	float peak {0.0f};
	void SetEmotion(uint32_t newEmotion, float newPeak)
	{
		emotion = newEmotion;
		peak = newPeak;
	}
};

/// A controller of the advisors that writes down each call
struct FakeControl
{
	std::vector<std::string> calls;
	FakeScreen screen;
	std::array<FakeDude, 2> dudes;

	void SpiritEject(int32_t type, bool isHelp) { calls.push_back(Line("eject", type, isHelp)); }
	void SpiritHome(int32_t type, bool isHelp) { calls.push_back(Line("home", type, isHelp)); }
	void SpiritFly(int32_t type, float x, float y) { calls.push_back(Line("fly", type, x, y)); }
	void SpiritCling(int32_t type, float x, float y) { calls.push_back(Line("cling", type, x, y)); }
	void SpiritScreenPoint(int32_t type, glm::ivec2 pixel) { calls.push_back(Line("screen", type, pixel.x, pixel.y)); }
	void SpiritPointPosition(int32_t type, const glm::vec3& position, bool inWorld)
	{
		calls.push_back(Line("point", type, position.x, inWorld));
	}
	void SpiritLookAtPosition(int32_t type, const glm::vec3& position) { calls.push_back(Line("look", type, position.x)); }
	void SpiritStopPointing(int32_t type) { calls.push_back(Line("stop pointing", type)); }
	void SpiritStopLooking(int32_t type) { calls.push_back(Line("stop looking", type)); }
	void SpiritPlayAnim(int32_t type, float x, float y, uint32_t anim, float speed)
	{
		calls.push_back(Line("anim", type, x, y, anim, speed));
	}
	[[nodiscard]] const FakeScreen& GetScreen() const { return screen; }
	[[nodiscard]] FakeDude& Dude(int dude) { return dudes.at(static_cast<size_t>(dude)); }

	template <typename... Args>
	static std::string Line(const char* name, Args... args)
	{
		std::string text = name;
		((text += " " + std::to_string(args)), ...);
		return text;
	}
};

/// A world that writes down each change
struct FakeWorld final: WorldTarget
{
	std::vector<std::string> calls;
	void SetTimeOfDay(float hours) override { calls.push_back("time " + std::to_string(hours)); }
	void SetPlayerAlignment(PlayerNames player, float alignment) override
	{
		calls.push_back("alignment " + std::to_string(static_cast<int>(player)) + " " + std::to_string(alignment));
	}
	void SetBonusUnlocked(size_t bonus, bool unlocked) override
	{
		calls.push_back("bonus " + std::to_string(bonus) + " " + std::to_string(static_cast<int>(unlocked)));
	}
};

} // namespace

TEST(DebugActs, AnActOnBothAdvisorsCallsTheGoodOneAndThenTheEvilOne)
{
	FakeControl control;
	ASSERT_TRUE(ApplyAdvisorAct(control, DebugAdvisor {.act = DebugAdvisor::Act::Out, .advisor = k_BothAdvisors}));
	ASSERT_TRUE(ApplyAdvisorAct(control, DebugAdvisor {.act = DebugAdvisor::Act::Vanish, .advisor = k_EvilAdvisor}));
	EXPECT_EQ(control.calls, (std::vector<std::string> {"eject 1 0", "eject 2 0", "home 2 1"}));
}

TEST(DebugActs, EachActCallsTheFunctionOfTheScripts)
{
	FakeControl control;
	const auto act = [&control](DebugAdvisor command) {
		command.advisor = k_GoodAdvisor;
		EXPECT_TRUE(ApplyAdvisorAct(control, command)) << NameOf(command.act);
	};
	act({.act = DebugAdvisor::Act::Appear});
	act({.act = DebugAdvisor::Act::Home});
	act({.act = DebugAdvisor::Act::Fly, .place = {0.5f, 0.25f}});
	act({.act = DebugAdvisor::Act::Cling, .place = {1.0f, 0.0f}});
	act({.act = DebugAdvisor::Act::PointAt, .position = {7.0f, 0.0f, 0.0f}, .inWorld = true});
	act({.act = DebugAdvisor::Act::LookAt, .position = {9.0f, 0.0f, 0.0f}});
	act({.act = DebugAdvisor::Act::StopPointing});
	act({.act = DebugAdvisor::Act::StopLooking});
	act({.act = DebugAdvisor::Act::PlayAnim, .place = {0.5f, 0.5f}, .anim = 79, .speed = 3.0f});
	EXPECT_EQ(control.calls,
	          (std::vector<std::string> {"eject 1 1", "home 1 0", "fly 1 0.500000 0.250000", "cling 1 1.000000 0.000000",
	                                     "point 1 7.000000 1", "look 1 9.000000", "stop pointing 1", "stop looking 1",
	                                     "anim 1 0.500000 0.500000 79 3.000000"}));
}

TEST(DebugActs, APointOnTheScreenUsesTheScreenOfEachComputer)
{
	FakeControl control;
	control.screen = {.width = 1000, .height = 500};
	ASSERT_TRUE(ApplyAdvisorAct(
	    control, DebugAdvisor {.act = DebugAdvisor::Act::ScreenPoint, .advisor = k_EvilAdvisor, .place = {0.25f, 0.5f}}));
	EXPECT_EQ(control.calls, (std::vector<std::string> {"screen 2 250 250"}));
}

TEST(DebugActs, AFeelGivesTheEmotionToTheAdvisorAtTheFullPeak)
{
	FakeControl control;
	ASSERT_TRUE(
	    ApplyAdvisorAct(control, DebugAdvisor {.act = DebugAdvisor::Act::Feel, .advisor = k_EvilAdvisor, .emotion = 5}));
	EXPECT_EQ(control.dudes[0].emotion, 0u);
	EXPECT_EQ(control.dudes[1].emotion, 5u);
	EXPECT_FLOAT_EQ(control.dudes[1].peak, 1.0f);
}

TEST(DebugActs, ARefusedActOnTheAdvisorsChangesNothing)
{
	FakeControl control;
	const auto nan = std::numeric_limits<float>::quiet_NaN();
	const std::vector<DebugAdvisor> bad {
	    {.act = DebugAdvisor::Act::Out, .advisor = 3},
	    {.act = DebugAdvisor::Act::Fly, .advisor = 1, .place = {1.5f, 0.0f}},
	    {.act = DebugAdvisor::Act::Fly, .advisor = 1, .place = {nan, 0.0f}},
	    {.act = DebugAdvisor::Act::PointAt, .advisor = 1, .position = {0.0f, nan, 0.0f}},
	    {.act = DebugAdvisor::Act::PlayAnim, .advisor = 1, .anim = k_AdvisorAnims},
	    {.act = DebugAdvisor::Act::PlayAnim, .advisor = 1, .speed = 5.0f},
	    {.act = DebugAdvisor::Act::Feel, .advisor = 1, .emotion = 8},
	    {.act = static_cast<DebugAdvisor::Act>(13), .advisor = 1},
	};
	for (const auto& act : bad)
	{
		EXPECT_TRUE(AdvisorActRefusal(act).has_value());
		EXPECT_FALSE(ApplyAdvisorAct(control, act));
	}
	EXPECT_TRUE(control.calls.empty());
}

TEST(DebugActs, TheChangesOfTheDebugMenusGoToTheWorld)
{
	FakeWorld world;
	EXPECT_TRUE(ApplyWorldAct(world, {.act = DebugWorld::Act::TimeOfDay, .value = 13.5f}));
	// 24 hours is the start of the day, as the slider of the World menu gives it
	EXPECT_TRUE(ApplyWorldAct(world, {.act = DebugWorld::Act::TimeOfDay, .value = 24.0f}));
	EXPECT_TRUE(ApplyWorldAct(world, {.act = DebugWorld::Act::PlayerAlignment, .player = 1, .value = -0.25f}));
	EXPECT_TRUE(ApplyWorldAct(world, {.act = DebugWorld::Act::BonusCreature, .index = 2, .value = 1.0f}));
	EXPECT_TRUE(ApplyWorldAct(world, {.act = DebugWorld::Act::BonusCreature, .index = 2, .value = 0.0f}));
	EXPECT_EQ(world.calls, (std::vector<std::string> {"time 13.500000", "time 0.000000", "alignment 1 -0.250000", "bonus 2 1",
	                                                  "bonus 2 0"}));
}

TEST(DebugActs, ARefusedChangeOfTheDebugMenusChangesNothing)
{
	FakeWorld world;
	const std::vector<DebugWorld> bad {
	    {.act = DebugWorld::Act::TimeOfDay, .value = -1.0f},
	    {.act = DebugWorld::Act::TimeOfDay, .value = 24.5f},
	    {.act = DebugWorld::Act::TimeOfDay, .value = std::numeric_limits<float>::infinity()},
	    {.act = DebugWorld::Act::PlayerAlignment, .player = 1, .value = 1.5f},
	    {.act = DebugWorld::Act::PlayerAlignment, .player = static_cast<uint8_t>(PlayerNames::NEUTRAL), .value = 0.0f},
	    {.act = DebugWorld::Act::BonusCreature, .index = 200, .value = 1.0f},
	    {.act = DebugWorld::Act::BonusCreature, .index = 0, .value = 0.5f},
	    {.act = static_cast<DebugWorld::Act>(3), .value = 0.0f},
	};
	for (const auto& act : bad)
	{
		EXPECT_TRUE(WorldActRefusal(act).has_value()) << static_cast<int>(act.act);
		EXPECT_FALSE(ApplyWorldAct(world, act));
	}
	EXPECT_TRUE(world.calls.empty());
}

TEST(DebugActs, ALineOfTheConsoleHasOneToTheMostBytesAndNoZeroByte)
{
	EXPECT_FALSE(ConsoleLineRefusal({.text = "CREATE_TOWN(1, \"1,1\", \"Celtic\")"}).has_value());
	EXPECT_FALSE(ConsoleLineRefusal({.text = std::string(player_commands::k_MostConsoleLineBytes, 'a')}).has_value());
	EXPECT_TRUE(ConsoleLineRefusal({.text = ""}).has_value());
	EXPECT_TRUE(ConsoleLineRefusal({.text = std::string(player_commands::k_MostConsoleLineBytes + 1, 'a')}).has_value());
	EXPECT_TRUE(ConsoleLineRefusal({.text = std::string("a\0b", 3)}).has_value());
}

TEST(DebugActs, TheRefusalOfACommandIsOnlyForTheActsOfTheDebugWindows)
{
	EXPECT_TRUE(RefusalOf(DebugAdvisor {.advisor = 9}).has_value());
	EXPECT_TRUE(RefusalOf(DebugWorld {.act = DebugWorld::Act::TimeOfDay, .value = 30.0f}).has_value());
	EXPECT_TRUE(RefusalOf(DebugConsoleLine {}).has_value());
	EXPECT_FALSE(RefusalOf(DebugAdvisor {.advisor = 1}).has_value());
	EXPECT_FALSE(RefusalOf(player_commands::DebugCheat {}).has_value());
	EXPECT_TRUE(player_commands::OnlyDebuggerSends(DebugConsoleLine {.text = "a"}));
	EXPECT_FALSE(player_commands::OnlyDebuggerSends(player_commands::DebugCheat {}));
	EXPECT_FALSE(player_commands::OnlyEditorSends(DebugWorld {}));
	// An observer does not send an act of the debug windows
	EXPECT_FALSE(player_commands::ServerTakesFromObserver(DebugAdvisor {}, true));
}
