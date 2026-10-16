/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <IFRFile.h>
#include <gtest/gtest.h>

#include "ForceFeedback/ForceFeedbackBackend.h"
#include "ForceFeedback/ForceFeedbackRules.h"

#define LOCATOR_IMPLEMENTATIONS
#include "ECS/Systems/Implementations/ForceFeedbackSystem.h"

using namespace openblack;
using namespace openblack::force_feedback;
using openblack::ecs::systems::ForceFeedbackSystem;

namespace
{

/// A device that keeps what it gets
struct DeviceLog
{
	std::vector<std::pair<ImmersionEffectType, bool>> calls;
	float strength {0.0f};
};

class FakeBackend final: public Backend
{
public:
	explicit FakeBackend(std::shared_ptr<DeviceLog> log)
	    : _log(std::move(log))
	{
	}
	[[nodiscard]] std::string_view Name() const override { return "fake"; }
	[[nodiscard]] DeviceKind Kind() const override { return DeviceKind::Tactile; }
	void Vibrate(float strength) override { _log->strength = strength; }
	void Started(ImmersionEffectType type, std::string_view /*name*/) override { _log->calls.emplace_back(type, true); }
	void Stopped(ImmersionEffectType type, std::string_view /*name*/) override { _log->calls.emplace_back(type, false); }

private:
	std::shared_ptr<DeviceLog> _log;
};

void PutU32(std::vector<uint8_t>& out, uint32_t value)
{
	for (int i = 0; i < 4; ++i)
	{
		out.push_back(static_cast<uint8_t>(value >> (8 * i)));
	}
}

void PutString(std::vector<uint8_t>& out, const std::string& text)
{
	out.insert(out.end(), text.begin(), text.end());
	out.push_back(0);
}

using Keys = std::vector<std::pair<std::string, std::string>>;

void AddObject(std::vector<uint8_t>& file, const std::string& name, const std::string& type, const Keys& keys)
{
	std::vector<uint8_t> body;
	PutString(body, name);
	PutString(body, type);
	PutString(body, "");
	std::vector<uint8_t> first;
	PutString(first, "ID");
	PutString(first, "{" + name + "}");
	PutU32(body, static_cast<uint32_t>(first.size() + 4));
	body.insert(body.end(), first.begin(), first.end());
	for (const auto& [key, value] : keys)
	{
		PutString(body, key);
		PutString(body, value);
	}
	PutU32(file, static_cast<uint32_t>(body.size() + 4));
	file.insert(file.end(), body.begin(), body.end());
}

/// A small effect file with the effects that the tests play, with values of the game's files
ifr::IFRFile MakeEffects()
{
	std::vector<uint8_t> data {'i', 'f', 'p', 'r'};
	PutU32(data, 6);
	// A slap: 20 ms of a quick square wave at 7209, full strength from its start
	AddObject(data, "CreatureSlap", "Periodic",
	          {{"Magnitude", "7209"},
	           {"Period", "9343"},
	           {"Waveform", "1"},
	           {"Duration", "20000"},
	           {"AttackDuration", "0"},
	           {"FadeDuration", "0"}});
	AddObject(
	    data, "LeashPull", "Periodic",
	    {{"Magnitude", "1627"}, {"Period", "6295"}, {"Duration", "200000"}, {"AttackDuration", "0"}, {"FadeDuration", "0"}});
	// The fur: bumps on the y axis, which play until something stops them
	AddObject(data, "Fur", "Texture",
	          {{"Axes", "1"},
	           {"Offset", "512"},
	           {"PosMagnitude", "460"},
	           {"NegMagnitude", "3000"},
	           {"PosBumpWidth", "4"},
	           {"PosBumpSpace", "5"},
	           {"NegBumpWidth", "5"},
	           {"NegBumpSpace", "20"}});
	AddObject(data, "TradeMark", "Periodic", {{"Duration", "1000000"}});
	AddObject(data, "MovingHandStraw", "Texture", {});
	// A beat of 1.01 s that plays a thousand times
	AddObject(data, "Heart", "Periodic",
	          {{"Period", "1010000"}, {"Duration", "1000000"}, {"Iterations", "1000"}, {"StartDelay", "430000"}});
	ifr::IFRFile file;
	EXPECT_EQ(file.Open(data), ifr::IFRResult::Success);
	return file;
}

struct Connected
{
	ForceFeedbackSystem system;
	std::shared_ptr<DeviceLog> log = std::make_shared<DeviceLog>();

	Connected() { system.Connect(std::make_unique<FakeBackend>(log), MakeEffects()); }
};

} // namespace

TEST(ForceFeedbackRules, TableIsInTheOrderOfTheTypes)
{
	for (size_t i = 0; i < k_Effects.size(); ++i)
	{
		EXPECT_EQ(static_cast<size_t>(k_Effects.at(i).type), i);
	}
	EXPECT_EQ(EntryFor(ImmersionEffectType::Fur)->name, "Fur");
	EXPECT_EQ(EntryFor(ImmersionEffectType::LeashPull)->name, "LeashPull");
	EXPECT_EQ(EntryFor(ImmersionEffectType::CommandSuccess)->name, "MiscCommandSuccess");
	EXPECT_EQ(EntryFor(ImmersionEffectType::HandOverTeleport)->name, "SpellTeleport");
	// The special crops and the types past them have no effect
	EXPECT_EQ(EntryFor(ImmersionEffectType::HandOverSpecialCrops), nullptr);
	EXPECT_EQ(EntryFor(ImmersionEffectType::None), nullptr);
}

TEST(ForceFeedbackRules, DetailLevels)
{
	// The logo and the mushroom challenge play at every level; the hand's surfaces only at the highest
	EXPECT_TRUE(LevelAllows(ImmersionEffectType::Trademark, 0));
	EXPECT_TRUE(LevelAllows(ImmersionEffectType::MushroomChallenge, 0));
	EXPECT_FALSE(LevelAllows(ImmersionEffectType::Fur, 0));
	EXPECT_TRUE(LevelAllows(ImmersionEffectType::Fur, k_DefaultLevel));
	EXPECT_FALSE(LevelAllows(ImmersionEffectType::CommandSuccess, k_DefaultLevel));
	EXPECT_TRUE(LevelAllows(ImmersionEffectType::CommandSuccess, k_HighestLevel));
	EXPECT_FALSE(LevelAllows(ImmersionEffectType::HandOverSpecialCrops, k_HighestLevel));
	EXPECT_EQ(FileFor(DeviceKind::FullForce), "FullFeedback.ifr");
	EXPECT_EQ(FileFor(DeviceKind::Tactile), "Tactile.ifr");
}

TEST(ForceFeedbackRules, PlayTime)
{
	const auto file = MakeEffects();
	const auto heart = EffectFrom(file, *file.Find("Heart"));
	ASSERT_EQ(heart.parts.size(), 1);
	// The delay, then a thousand plays of a second; once is as long as the delay and a second
	EXPECT_DOUBLE_EQ(*PlaySeconds(heart.parts.front(), std::nullopt), 0.43 + 1000.0);
	EXPECT_DOUBLE_EQ(*PlaySeconds(heart.parts.front(), 1u), 1.43);
	const auto fur = EffectFrom(file, *file.Find("Fur"));
	EXPECT_FALSE(PlaySeconds(fur.parts.front(), std::nullopt).has_value());
	EXPECT_TRUE(Plays(fur, 1.0e6, std::nullopt));
	const auto slap = EffectFrom(file, *file.Find("CreatureSlap"));
	EXPECT_TRUE(Plays(slap, 0.019, std::nullopt));
	EXPECT_FALSE(Plays(slap, 0.021, std::nullopt));
}

TEST(ForceFeedbackRules, QuickerBeatKeepsTheNumberOfPeriods)
{
	const auto file = MakeEffects();
	auto heart = EffectFrom(file, *file.Find("Heart"));
	EXPECT_EQ(heart.periodMs, 1010u);
	const auto ratio = 1000000.0f / 1010000.0f;
	EXPECT_FLOAT_EQ(*heart.durationRatio, ratio);

	QuickenBeat(heart, 0.5f);
	const auto& quick = std::get<ifr::Periodic>(heart.parts.front());
	EXPECT_EQ(quick.period, 505000u);
	EXPECT_EQ(quick.timing.duration, static_cast<uint32_t>(static_cast<float>(505) * ratio) * 1000u);

	// The change is from the file's period each time, never shorter than 3 ms
	QuickenBeat(heart, 1.0f);
	EXPECT_EQ(std::get<ifr::Periodic>(heart.parts.front()).period, 3000u);
	QuickenBeat(heart, 0.0f);
	EXPECT_EQ(std::get<ifr::Periodic>(heart.parts.front()).period, 1010000u);
}

TEST(ForceFeedbackRules, HandTexture)
{
	HandTextureState state;
	bool strawPlays = false;
	const auto plays = [&strawPlays](ImmersionEffectType type) {
		return type == ImmersionEffectType::HandOverStraw && strawPlays;
	};
	// A new surface starts
	auto step = NextHandTexture(state, ImmersionEffectType::HandOverStraw, plays);
	EXPECT_FALSE(step.stop.has_value());
	EXPECT_EQ(step.start, ImmersionEffectType::HandOverStraw);
	strawPlays = true;
	// The same surface does nothing
	step = NextHandTexture(state, ImmersionEffectType::HandOverStraw, plays);
	EXPECT_FALSE(step.start.has_value());
	// Another surface stops the first and starts
	step = NextHandTexture(state, ImmersionEffectType::HandOverWood, plays);
	EXPECT_EQ(step.stop, ImmersionEffectType::HandOverStraw);
	EXPECT_EQ(step.start, ImmersionEffectType::HandOverWood);
	// An effect that played before the hand took it is not stopped by the hand, and starts again when it ends
	step = NextHandTexture(state, ImmersionEffectType::HandOverStraw, plays);
	EXPECT_TRUE(state.startedElsewhere);
	strawPlays = false;
	step = NextHandTexture(state, ImmersionEffectType::HandOverStraw, plays);
	EXPECT_EQ(step.start, ImmersionEffectType::HandOverStraw);
	EXPECT_FALSE(state.startedElsewhere);
	step = NextHandTexture(state, std::nullopt, plays);
	EXPECT_EQ(step.stop, ImmersionEffectType::HandOverStraw);
	EXPECT_FALSE(step.start.has_value());
}

TEST(ForceFeedbackRules, FurStartsAndStopsInTurnWhileTheHandTouches)
{
	EXPECT_EQ(FurStep(true, false), true);
	EXPECT_EQ(FurStep(true, true), false);
	EXPECT_EQ(FurStep(false, true), false);
	EXPECT_FALSE(FurStep(false, false).has_value());
}

TEST(ForceFeedbackRules, PeriodicStrength)
{
	ifr::Periodic slap {.timing = {.duration = 20000}, .waveform = ifr::Waveform::Square, .magnitude = 7209, .period = 9343};
	// A quick wave shows as its strength while it plays, and nothing after
	EXPECT_NEAR(PeriodicStrength(slap, 0.01, std::nullopt), 0.7209f, 1e-4f);
	EXPECT_EQ(PeriodicStrength(slap, 0.03, std::nullopt), 0.0f);
	// The start of the envelope climbs to the strength
	slap.envelope = ifr::Envelope {.attackLevel = 0, .attackTime = 10000, .fadeLevel = 7209, .fadeTime = 0};
	EXPECT_NEAR(PeriodicStrength(slap, 0.005, std::nullopt), 0.36045f, 1e-4f);
	// A slow wave shows as its value: a sine at a quarter of its period is at the top, at half it is at nothing
	const ifr::Periodic slow {.timing = {}, .waveform = ifr::Waveform::Sine, .magnitude = 10000, .period = 1000000};
	EXPECT_NEAR(PeriodicStrength(slow, 0.25, std::nullopt), 1.0f, 1e-4f);
	EXPECT_NEAR(PeriodicStrength(slow, 0.5, std::nullopt), 0.0f, 1e-4f);
	// The delay plays nothing
	const ifr::Periodic late {.timing = {.startDelay = 100000}, .magnitude = 10000, .period = 1000};
	EXPECT_EQ(PeriodicStrength(late, 0.05, std::nullopt), 0.0f);
	EXPECT_GT(PeriodicStrength(late, 0.15, std::nullopt), 0.0f);
}

TEST(ForceFeedbackRules, TextureStrength)
{
	const auto file = MakeEffects();
	const auto fur = std::get<ifr::Texture>(EffectFrom(file, *file.Find("Fur")).parts.front());
	// Only movement on the y axis feels the fur; a still cursor feels nothing
	EXPECT_EQ(TextureStrength(fur, {0.0f, 100.0f}, {50.0f, 100.0f}), 0.0f);
	EXPECT_EQ(TextureStrength(fur, {0.0f, 100.0f}, {0.0f, 100.0f}), 0.0f);
	// Down the screen the bumps are 4 wide every 5 from 512: 512 is a bump, 516 is not
	EXPECT_NEAR(TextureStrength(fur, {0.0f, 511.0f}, {0.0f, 512.5f}), 0.046f, 1e-4f);
	EXPECT_EQ(TextureStrength(fur, {0.0f, 516.1f}, {0.0f, 516.9f}), 0.0f);
	// Up the screen the bumps are 5 wide every 20, and stronger
	EXPECT_NEAR(TextureStrength(fur, {0.0f, 534.0f}, {0.0f, 533.0f}), 0.3f, 1e-4f);
	EXPECT_EQ(TextureStrength(fur, {0.0f, 539.0f}, {0.0f, 538.0f}), 0.0f);
}

TEST(ForceFeedbackSystem, WithoutDeviceNothingPlays)
{
	ForceFeedbackSystem system;
	EXPECT_FALSE(system.DeviceExists());
	system.Start(ImmersionEffectType::Slap);
	EXPECT_FALSE(system.IsPlaying(ImmersionEffectType::Slap));
	EXPECT_TRUE(system.Events().empty());
}

TEST(ForceFeedbackSystem, AnEffectPlaysForItsTime)
{
	Connected device;
	auto& system = device.system;
	EXPECT_TRUE(system.DeviceExists());
	EXPECT_EQ(system.DeviceName(), "fake");
	system.Start(ImmersionEffectType::Slap);
	EXPECT_TRUE(system.IsPlaying(ImmersionEffectType::Slap));
	system.Update(0.01f, {0.5f, 0.5f});
	EXPECT_NEAR(device.log->strength, 0.7209f, 1e-4f);
	system.Update(0.02f, {0.5f, 0.5f});
	EXPECT_FALSE(system.IsPlaying(ImmersionEffectType::Slap));
	EXPECT_EQ(device.log->strength, 0.0f);
	ASSERT_EQ(system.Events().size(), 1);
	EXPECT_TRUE(system.Events().front().start);
	EXPECT_EQ(system.Events().front().type, ImmersionEffectType::Slap);
	// An effect that the file does not have does nothing
	system.Start(ImmersionEffectType::GestureSuccess);
	EXPECT_EQ(system.Events().size(), 1);
}

TEST(ForceFeedbackSystem, TheDetailLevelGatesTheEffects)
{
	Connected device;
	auto& system = device.system;
	system.SetLevel(0);
	system.Start(ImmersionEffectType::LeashPull);
	system.Start(ImmersionEffectType::Trademark);
	EXPECT_FALSE(system.IsPlaying(ImmersionEffectType::LeashPull));
	EXPECT_TRUE(system.IsPlaying(ImmersionEffectType::Trademark));
	// A new level stops every effect
	system.SetLevel(k_HighestLevel);
	EXPECT_FALSE(system.IsPlaying(ImmersionEffectType::Trademark));
	system.Start(ImmersionEffectType::HandOverStraw);
	EXPECT_TRUE(system.IsPlaying(ImmersionEffectType::HandOverStraw));
	system.StopAll();
	EXPECT_FALSE(system.IsPlaying(ImmersionEffectType::HandOverStraw));
}

TEST(ForceFeedbackSystem, LeashPullStartsAgainAndStops)
{
	Connected device;
	auto& system = device.system;
	system.Start(ImmersionEffectType::LeashPull);
	system.Update(0.1f, {});
	system.Start(ImmersionEffectType::LeashPull);
	system.Update(0.15f, {});
	// Started again, it plays its full 0.2 s from the second start
	EXPECT_TRUE(system.IsPlaying(ImmersionEffectType::LeashPull));
	system.Stop(ImmersionEffectType::LeashPull);
	EXPECT_FALSE(system.IsPlaying(ImmersionEffectType::LeashPull));
	const std::vector<std::pair<ImmersionEffectType, bool>> expected {{ImmersionEffectType::LeashPull, true},
	                                                                  {ImmersionEffectType::LeashPull, true},
	                                                                  {ImmersionEffectType::LeashPull, false}};
	EXPECT_EQ(device.log->calls, expected);
}

TEST(ForceFeedbackSystem, TheFurGoesOnAndOffEachFrameOfTouch)
{
	Connected device;
	auto& system = device.system;
	system.TouchCreature(true);
	EXPECT_TRUE(system.IsPlaying(ImmersionEffectType::Fur));
	system.TouchCreature(true);
	EXPECT_FALSE(system.IsPlaying(ImmersionEffectType::Fur));
	system.TouchCreature(true);
	EXPECT_TRUE(system.IsPlaying(ImmersionEffectType::Fur));
	system.TouchCreature(false);
	EXPECT_FALSE(system.IsPlaying(ImmersionEffectType::Fur));
	system.TouchCreature(false);
	EXPECT_EQ(device.log->calls.size(), 4);
}

TEST(ForceFeedbackSystem, TheHandTextureFollowsWhatTheHandTouches)
{
	Connected device;
	auto& system = device.system;
	system.SetLevel(k_HighestLevel);
	system.SetHandTexture(ImmersionEffectType::HandOverStraw);
	EXPECT_TRUE(system.IsPlaying(ImmersionEffectType::HandOverStraw));
	system.SetHandTexture(std::nullopt);
	EXPECT_FALSE(system.IsPlaying(ImmersionEffectType::HandOverStraw));
}

TEST(ForceFeedbackSystem, TheHeartBeatsQuicker)
{
	Connected device;
	auto& system = device.system;
	system.QuickenBeat(ImmersionEffectType::MiscHeart, 0.5f);
	system.StartTimes(ImmersionEffectType::MiscHeart, 1);
	ASSERT_EQ(system.Events().size(), 1);
	EXPECT_EQ(system.Events().front().iterations, 1u);
	// One play: the delay and one duration of the quicker beat
	system.Update(0.43f + 0.49f, {});
	EXPECT_TRUE(system.IsPlaying(ImmersionEffectType::MiscHeart));
	system.Update(0.02f, {});
	EXPECT_FALSE(system.IsPlaying(ImmersionEffectType::MiscHeart));
}
