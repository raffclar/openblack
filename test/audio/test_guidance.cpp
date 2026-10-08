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
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <fmt/format.h>
#include <gtest/gtest.h>

#include "Audio/Audio.h"
#include "Audio/Device/SampleOutput.h"
#include "Audio/Device/Sound.h"
#include "Audio/GameQueries.h"
#include "Audio/Services/Guidance.h"
#include "Audio/Services/Voices.h"
#include "Common/GUtilsDistance.h"
#include "Help/HelpSystem.h"
#include "Help/ScriptControl.h"

// The guidance remarks of the help spirits.
// The random numbers come from SeededRandom on a seed; the expected values of the seeded tests come from an
// independent emulation of the original. A fake
// output stands for the audio device.

using namespace openblack;
using namespace openblack::audio;
using namespace openblack::audio::guidance;

namespace
{
class FakeOutput final: public SampleOutput
{
public:
	std::array<bool, 16> playing {};
	bool Play(size_t channel, Sound&, const Start&) override
	{
		playing[channel] = true;
		return true;
	}
	void Stop(size_t channel) override { playing[channel] = false; }
	void StopRamped(size_t channel) override { playing[channel] = false; }
	[[nodiscard]] int64_t PlayPositionMs(size_t channel) const override { return playing[channel] ? 0 : -1; }
	[[nodiscard]] bool Playing(size_t channel) const override { return playing[channel]; }
	void SetGain(size_t, float) override {}
	void SetPitch(size_t, float) override {}
	void SetPosition(size_t, glm::vec3) override {}
	void ReleaseLoop(size_t) override {}
	void SetListener(glm::vec3) override {}
	void Update() override {}
	[[nodiscard]] size_t Sources() const override { return 0; }
	void DeleteAll() override { playing.fill(false); }
};

/// The texts the fixture gives a Guidance sample (sample n = position + 1)
constexpr std::array<uint32_t, 22> k_GuidanceTexts {4940, 4941, 4942, 4943, 4944, 4945, 4946, 4947, 4948, 4949, 4950,
                                                    4951, 4952, 4953, 4954, 4955, 4956, 4957, 4967, 4968, 4969, 5720};

int GuidanceSampleOf(uint32_t text)
{
	for (size_t i = 0; i < k_GuidanceTexts.size(); ++i)
	{
		if (k_GuidanceTexts.at(i) == text)
		{
			return static_cast<int>(i) + 1;
		}
	}
	return 0;
}

struct Said
{
	uint32_t first;
	uint32_t last;
	std::string script;
};

class GuidanceTest: public ::testing::Test
{
protected:
	FakeOutput output;
	std::map<entt::id_type, Sound> sounds;
	BankId guidanceBank {k_NoBank};
	BankId inGame {k_NoBank};
	static inline uint32_t s_Turn = 100000;
	static inline int s_Land = 2;
	static inline int s_HelpLevel = 3;
	static inline uint32_t s_Seed = 12345;
	static inline std::optional<std::array<float, 3>> s_Needs;
	static inline std::vector<DesireTown> s_Towns;
	static inline HeartBeatInput s_Heart;
	static inline std::vector<Said> s_Said;
	static inline std::vector<int> s_Categories;

	void SetUp() override
	{
		s_Turn = 100000;
		s_Land = 2;
		s_HelpLevel = 3;
		s_Seed = 12345;
		s_Needs.reset();
		s_Towns.clear();
		s_Heart = {};
		s_Said.clear();
		s_Categories.clear();
		GameQueries queries;
		queries.turn = []() { return s_Turn; };
		queries.landNumber = []() { return s_Land; };
		queries.helpLevel = []() { return s_HelpLevel; };
		queries.camera = []() -> std::optional<CameraState> { return CameraState {}; };
		queries.nearestTownAt = [](glm::vec3, float maxDistance) -> std::optional<ThingId> {
			EXPECT_EQ(maxDistance, 100.0f);
			return s_Needs ? std::optional<ThingId>(7) : std::nullopt;
		};
		queries.townResourceNeeds = [](ThingId town) {
			EXPECT_EQ(town, 7u);
			return s_Needs;
		};
		queries.desireTowns = []() { return s_Towns; };
		queries.heartBeat = []() { return s_Heart; };
		queries.helpRunMessage = [](uint32_t first, uint32_t last, std::string_view script) {
			s_Said.push_back({first, last, std::string(script)});
			return true;
		};
		queries.helpTriggerCategory = [](int category) { s_Categories.push_back(category); };
		audio::Init(std::move(queries));
		guidanceBank = RegisterBank("audio/dialogue/Guidance.sad", "Guidance.sad");
		inGame = RegisterBank("audio/sfx/game/ingame.sad", "ingame.sad");
		sample_play::Backend backend;
		backend.output = &output;
		backend.sound = [this](entt::id_type id) -> Sound* {
			const auto found = sounds.find(id);
			return found != sounds.end() ? &found->second : nullptr;
		};
		backend.rand = []() { return 16383; };
		backend.camera = []() -> std::optional<glm::vec3> { return glm::vec3(0.0f); };
		backend.ownerPosition = [](const Owner& owner) { return OwnerSoundPosition(owner); };
		sample_play::SetBackend(std::move(backend));
		sample_play::SetMainVolume(127);
		audio::ClearMap();
		output = FakeOutput {};
		for (int n = 1; n <= static_cast<int>(k_GuidanceTexts.size()); ++n)
		{
			Add(guidanceBank, n);
		}
		Add(inGame, 45);
		// the voice table: text i is named HELP_TEXT_<i>; the Guidance waves are the texts of k_GuidanceTexts
		std::vector<std::string> names;
		for (uint32_t i = 0; i < 6974; ++i)
		{
			names.push_back(fmt::format("HELP_TEXT_{}", i));
		}
		VoiceTable::SampleNames guidance;
		for (const auto text : k_GuidanceTexts)
		{
			guidance.push_back(fmt::format("K:\\x\\HELP_TEXT_{}.wav", text));
		}
		voices::SetTable(VoiceTable::Build(names, {}, {}, guidance));
		std::array<SpriteList, k_SpriteLists> lists {};
		for (size_t k = 0; k < lists.size(); ++k)
		{
			// list k: 1000 k + 1 .. 1000 k + 3
			lists.at(k) = {static_cast<uint32_t>(1000 * k + 1), static_cast<uint32_t>(1000 * k + 2),
			               static_cast<uint32_t>(1000 * k + 3)};
		}
		SetSpriteLists(lists);
		SetRandom([](uint32_t n) { return SeededRandom(n, s_Seed); });
		ResetForTests();
	}
	void TearDown() override
	{
		SetRandom({});
		audio::ClearMap();
		sample_play::SetMainVolume(127); // back to its default (the maximum)
		sample_play::SetBackend({});
		voices::SetTable({});
		audio::Shutdown();
	}

	void Add(BankId bank, int number)
	{
		Sound sound;
		sound.name = fmt::format("{} {}", BankGroup(bank), number);
		sound.id = number;
		sound.bank = bank;
		sound.priority = 20;
		sound.sampleRate = 22050;
		sound.pitch = 100;
		sound.pitchDeviation = 0;
		sound.duration = 2.0f;
		sounds.emplace(SampleId(bank, number), std::move(sound));
	}

	[[nodiscard]] static std::optional<sample_play::ChannelInfo> Playing(BankId bank)
	{
		for (const auto& info : sample_play::Channels())
		{
			if (info.playing && info.bank == bank)
			{
				return info;
			}
		}
		return std::nullopt;
	}
};
} // namespace

TEST(GuidanceTable, TypeTableRows)
{
	// spot checks of the type table: {base, level, always}
	EXPECT_EQ(k_Types.at(0).base, 50u);
	EXPECT_FALSE(k_Types.at(0).always);
	EXPECT_EQ(k_Types.at(4).helpLevel, 0);
	EXPECT_EQ(k_Types.at(8).base, 100u);
	EXPECT_EQ(k_Types.at(16).helpLevel, 4);
	EXPECT_EQ(k_Types.at(28).base, 10000u);
	EXPECT_TRUE(k_Types.at(30).always);
	EXPECT_EQ(k_Types.at(30).helpLevel, 3);
}

TEST(GuidanceRandom, SequenceFromASeed)
{
	// seed * 9377 + 9439, rotated right by 13, % n
	uint32_t seed = 12345;
	const std::array<uint32_t, 5> expected {507, 504, 162, 496, 651};
	for (const auto value : expected)
	{
		EXPECT_EQ(SeededRandom(1000, seed), value);
	}
}

TEST_F(GuidanceTest, IntervalWithASeed)
{
	// base + LocalRand(trunc(5 base (1 - r^3))), r = LocalFloatRand(1)
	s_Seed = 12345;
	EXPECT_EQ(Interval(Type::TownBeingAttacked), 5912u); // base 1000
	EXPECT_EQ(Interval(Type::TownBeingAttacked), 1249u);
	s_Seed = 12345;
	EXPECT_EQ(Interval(Type::TownDesire), 141u); // base 50
	EXPECT_EQ(Interval(Type::TownDesire), 282u);
	s_Seed = 12345;
	EXPECT_EQ(Interval(Type::VeryGood), 45076u); // base 10000
	// base 0: LocalRand(0) is 0 without a draw
	const auto before = s_Seed;
	s_Seed = before;
	EXPECT_EQ(Interval(Type::Disciple), 0u);
	// every interval lies in [base, 6 base)
	for (int i = 0; i < 200; ++i)
	{
		const auto v = Interval(Type::LowOnFood);
		EXPECT_GE(v, 2500u);
		EXPECT_LT(v, 15000u);
	}
}

TEST_F(GuidanceTest, PlayNowGates)
{
	// enough turns since the last play (lastPlayed 0, turn 100000 > any interval)
	EXPECT_TRUE(PlayNow(Type::TownDesire));
	// land 1 of a single-player campaign mutes the types that are not `always`
	s_Land = 1;
	EXPECT_FALSE(PlayNow(Type::TownDesire));
	EXPECT_TRUE(PlayNow(Type::TownAttack)); // always
	s_Land = 2;
	// the help level (0 while the help is off) below the type's
	s_HelpLevel = 2;
	EXPECT_FALSE(PlayNow(Type::CreatureAttackingThem)); // needs 3
	EXPECT_TRUE(PlayNow(Type::TownBeingAttacked));      // needs 2
	s_HelpLevel = 0;
	EXPECT_TRUE(PlayNow(Type::Disciple)); // needs 0
	EXPECT_FALSE(PlayNow(Type::HeartBeat));
	s_HelpLevel = 3;
	// TimeSince must be above the interval (unsigned, strict): just played -> 0 > base fails
	PlaySample(false, 4946, 0, static_cast<int>(Type::ResourceDrop), 127, 100, 90, std::nullopt, 0.0f, true);
	EXPECT_EQ(TimeSinceLastPlayed(Type::ResourceDrop), 0u);
	EXPECT_FALSE(PlayNow(Type::ResourceDrop));
	s_Turn += 25; // 25 > base 25 + LocalRand(...) never
	EXPECT_FALSE(PlayNow(Type::ResourceDrop));
	s_Turn += 1000; // past 6 x 25
	EXPECT_TRUE(PlayNow(Type::ResourceDrop));
	// CanPlaySpiritRemark: the gate type 8 first
	EXPECT_TRUE(CanPlaySpiritRemark(Type::LosingBelief));
}

TEST_F(GuidanceTest, InitDrawsTwiceForAPositiveFirstDraw)
{
	// last = turn - LocalRand(base) when that is > 0, with a second draw; else 0
	std::vector<uint32_t> draws;
	SetRandom([&draws](uint32_t n) {
		draws.push_back(n);
		return n / 2;
	});
	s_Turn = 2000;
	Init();
	const auto& state = GetState();
	// type 0, base 50: 2000 - 25 > 0, drawn again: 2000 - 25
	EXPECT_EQ(state.lastPlayed.at(0), 1975u);
	// type 28, base 10000: 2000 - 5000 <= 0 -> 0, one draw
	EXPECT_EQ(state.lastPlayed.at(28), 0u);
	// type 4, base 0: LocalRand(0) = 0 without asking -> 2000 > 0 -> 2000
	EXPECT_EQ(state.lastPlayed.at(4), 2000u);
	// the draws: two per type with turn - draw > 0 and base > 0, one for the others with base > 0
	EXPECT_EQ(draws.at(0), 50u);
	EXPECT_EQ(draws.at(1), 50u);
	EXPECT_EQ(state.heartBeatPitch, 30.0f);
	EXPECT_EQ(state.options.mode, 2);
	EXPECT_FALSE(state.options.is3D);
	EXPECT_FALSE(state.options.track);
}

TEST_F(GuidanceTest, PlaySampleOptions)
{
	// 3D: max = maxDistance, min = maxDistance x 0.333, mask 0x180, track 0; the sample from the voice table
	const auto channel = PlaySample(true, 4946, 1, static_cast<int>(Type::ResourceDrop), 127, 100, 90,
	                                glm::vec3(10.0f, 0.0f, 10.0f), 200.0f, true);
	EXPECT_NE(channel, k_NoChannel);
	const auto& state = GetState();
	EXPECT_EQ(state.options.maxDistance, 200.0f);
	EXPECT_FLOAT_EQ(state.options.minDistance, 200.0f * 0.333f);
	EXPECT_EQ(state.options.callerMask, 0x180u);
	EXPECT_FALSE(state.options.track);
	EXPECT_EQ(state.options.owner, Owner::Key(1));
	const auto info = Playing(guidanceBank);
	ASSERT_TRUE(info);
	EXPECT_EQ(info->sample, GuidanceSampleOf(4946));
	EXPECT_TRUE(info->is3D);
	EXPECT_EQ(TimeSinceLastPlayed(Type::ResourceDrop), 0u);
	// 2D: is3D 0, the rest as the 3D one left it; owner 0 is no owner
	PlaySample(false, 5720, 0, static_cast<int>(Type::DeathInVillage), 127, 100, 90, std::nullopt, 0.0f, true);
	EXPECT_FALSE(state.options.is3D);
	EXPECT_EQ(state.options.callerMask, 0x180u);
	EXPECT_EQ(state.options.owner, Owner::None());
	// 3D without a point: nothing, and lastPlayed stays
	s_Turn += 10;
	EXPECT_EQ(PlaySample(true, 4947, 0, static_cast<int>(Type::Belief), 127, 100, 90, std::nullopt, 200.0f, true), k_NoChannel);
	EXPECT_EQ(state.lastPlayed.at(static_cast<size_t>(Type::Belief)), 0u);
}

TEST_F(GuidanceTest, ResourceDropSamples)
{
	// ResourceDropSample: >= 0.5 pleased, < 0.25 displeased (food) or pleased again (wood, rain)
	for (int i = 0; i < 20; ++i)
	{
		const auto food = ResourceDropSample(0.5f, RainType::Food);
		EXPECT_GE(food, 4946u);
		EXPECT_LE(food, 4948u);
		const auto hungry = ResourceDropSample(0.1f, RainType::Food);
		EXPECT_GE(hungry, 4955u);
		EXPECT_LE(hungry, 4957u);
		const auto wood = ResourceDropSample(0.0f, RainType::Wood);
		EXPECT_GE(wood, 4949u);
		EXPECT_LE(wood, 4951u);
		const auto rain = ResourceDropSample(0.9f, RainType::Rain);
		EXPECT_GE(rain, 4952u);
		EXPECT_LE(rain, 4954u);
	}
	EXPECT_EQ(ResourceDropSample(0.3f, RainType::Food), 0u);
	EXPECT_EQ(ResourceDropSample(0.25f, RainType::Wood), 0u);
	EXPECT_EQ(ResourceDropSample(1.0f, RainType::None), 0u);
}

TEST_F(GuidanceTest, FoodDroppedOnATownThatWantsItIsPleased)
{
	// PlayResourceDropRemark: no town within 100 -> silence
	PlayResourceDropRemark(glm::vec3(5.0f, 0.0f, 5.0f), RainType::Food);
	EXPECT_FALSE(Playing(guidanceBank));
	// a town whose food values add up to 0.7: PLEASED_FOOD, 3D at the drop, max 200
	s_Needs = std::array<float, 3> {0.7f, 0.0f, 0.0f};
	PlayResourceDropRemark(glm::vec3(5.0f, 0.0f, 5.0f), RainType::Food);
	const auto info = Playing(guidanceBank);
	ASSERT_TRUE(info);
	EXPECT_GE(info->sample, GuidanceSampleOf(4946));
	EXPECT_LE(info->sample, GuidanceSampleOf(4948));
	EXPECT_TRUE(info->is3D);
	EXPECT_EQ(GetState().options.maxDistance, 200.0f);
	// on land 1 of the campaign (ResourceDrop is not `always`): silence
	ClearMap();
	ResetForTests();
	s_Land = 1;
	PlayResourceDropRemark(glm::vec3(5.0f, 0.0f, 5.0f), RainType::Food);
	EXPECT_FALSE(Playing(guidanceBank));
}

TEST_F(GuidanceTest, DesireSampleAndScore)
{
	// DesireSample with no random part (LocalFloatRand(0) for value 0 is 0; here the draws are 0)
	SetRandom([](uint32_t) { return 0u; });
	EXPECT_EQ(DesireSample(0, 0.9f), 4967u);
	EXPECT_EQ(DesireSample(0, 0.7f), 4968u);
	EXPECT_EQ(DesireSample(0, 0.5f), 4969u);
	EXPECT_EQ(DesireSample(0, 0.4f), 0u);
	EXPECT_EQ(DesireSample(5, 0.9f), 4973u);
	EXPECT_EQ(DesireSample(2, 0.9f), 0u);
	// DesireScore: 2 t0 s v^3 (1 - d^2) (1 - t1^3); t0 = 1 (long since the last desire), a new thing t1 = 0, d = 100 / 200
	const float score = DesireScore(77, 100.0f, 0.5f, 4967);
	EXPECT_FLOAT_EQ(score, static_cast<float>(2.0 * 0.125 * 0.75));
	// the same thing 150 turns later: t1 = 0.5
	s_Turn += 150;
	EXPECT_FLOAT_EQ(DesireScore(77, 100.0f, 0.5f, 4967), static_cast<float>(2.0 * 0.125 * 0.75 * (1.0 - 0.125)));
	// past 600 turns the thing is new again
	s_Turn += 600;
	EXPECT_EQ(TimeSinceThingSeen(77), 0u);
}

TEST_F(GuidanceTest, TownDesireEveryTenTurns)
{
	SetRandom([](uint32_t) { return 0u; });
	DesireTown town;
	town.id = 5;
	town.position = glm::vec3(20.0f, 0.0f, 20.0f);
	town.storagePit = glm::vec3(10.0f, 0.0f, 0.0f);
	town.population = 4;
	town.desires.at(0) = {0.9f, 0, 1.0f}; // DESIRE_FOOD, raw 1
	s_Towns = {town};
	s_Turn = 100001; // not a multiple of 10
	UpdateTownDesireRemarks();
	EXPECT_FALSE(Playing(guidanceBank));
	s_Turn = 100000;
	UpdateTownDesireRemarks();
	const auto info = Playing(guidanceBank);
	ASSERT_TRUE(info);
	EXPECT_EQ(info->sample, GuidanceSampleOf(4967));
	EXPECT_EQ(GetState().lastDesireSample, 4967u);
	// max distance 200 x (value - 0): the value is 2 x 1 x 1 x (1 - (d / 200)^2) x 1, d the pit's distance to the
	// camera by gutils::GetDistanceInMetres (the 1 / sqrt table: 10 m comes out a little short)
	const float d = gutils::GetDistanceInMetres(glm::vec3(10.0f, 0.0f, 0.0f), glm::vec3(0.0f));
	EXPECT_NEAR(d, 10.0f, 0.01f);
	const double near = static_cast<double>(d / 200.0f);
	EXPECT_FLOAT_EQ(GetState().options.maxDistance, 200.0f * static_cast<float>(2.0 * (1.0 - near * near)));
	// a town without people or storage pit says nothing
	ClearMap();
	ResetForTests();
	s_Towns.at(0).population = 0;
	UpdateTownDesireRemarks();
	EXPECT_FALSE(Playing(guidanceBank));
}

TEST_F(GuidanceTest, AlignmentRemarks)
{
	SetRandom([](uint32_t) { return 0u; });
	// below 2 x the max change a turn: nothing
	UpdateAlignmentRemarks(0.001f, 0.9f, 0.01f);
	EXPECT_TRUE(s_Said.empty());
	// a good player getting better (same sign), |a| > 0.75: type 29 and list 20 (sic)
	UpdateAlignmentRemarks(0.1f, 0.9f, 0.01f);
	ASSERT_EQ(s_Said.size(), 1u);
	EXPECT_EQ(s_Said.at(0).first, 20001u);
	EXPECT_EQ(s_Said.at(0).last, 20001u);
	EXPECT_EQ(s_Said.at(0).script, "MultiHelpJustTalkWithText");
	EXPECT_EQ(s_Categories.at(0), 8);
	EXPECT_EQ(TimeSinceLastPlayed(Type::VeryEvil), 0u);
	// a good player turning evil, |a| > 0.4: type 25, list 16
	ResetForTests();
	s_Said.clear();
	UpdateAlignmentRemarks(-0.1f, 0.5f, 0.01f);
	ASSERT_EQ(s_Said.size(), 1u);
	EXPECT_EQ(s_Said.at(0).first, 16001u);
	EXPECT_EQ(TimeSinceLastPlayed(Type::GoodBeingEvil), 0u);
	// an evil player turning good: type 26, list 17
	ResetForTests();
	s_Said.clear();
	UpdateAlignmentRemarks(0.1f, -0.5f, 0.01f);
	ASSERT_EQ(s_Said.size(), 1u);
	EXPECT_EQ(s_Said.at(0).first, 17001u);
	// the accumulator decays by 0.95 a call
	EXPECT_FLOAT_EQ(GetState().alignmentChange, 0.1f);
	UpdateAlignmentRemarks(0.0f, -0.5f, 0.01f);
	EXPECT_FLOAT_EQ(GetState().alignmentChange, 0.095f);
}

TEST_F(GuidanceTest, OneOffOnceAndTheNoTextScript)
{
	// OneOff(5): probability 0.1; a draw of 0 passes, then never again this land
	SetRandom([](uint32_t) { return 0u; });
	OneOff(5);
	ASSERT_EQ(s_Said.size(), 1u);
	EXPECT_EQ(s_Said.at(0).first, 3326u);
	EXPECT_EQ(TimeSinceLastPlayed(Type::OneOff), 0u);
	OneOff(5);
	EXPECT_EQ(s_Said.size(), 1u);
	// a draw above the probability: not said, the flag stays clear
	SetRandom([](uint32_t n) { return n - 1; });
	OneOff(1);
	EXPECT_EQ(s_Said.size(), 1u);
	EXPECT_FALSE(GetState().oneOffs.at(1));
	// type 31 uses the other help script
	SpiritSay(42, Type::JustTalkNoText);
	EXPECT_EQ(s_Said.back().script, "MultiHelpJustTalkWithNoText");
}

TEST_F(GuidanceTest, HeartBeatPitchAndTheCitadelHeart)
{
	// HeartBeat: pitch = (30 + 70 v - pitch) x 0.1 + pitch from 30
	HeartBeat(1.0f);
	EXPECT_FLOAT_EQ(GetState().heartBeatPitch, 37.0f);
	EXPECT_FALSE(Playing(inGame)); // no citadel heart
	// the phase: + pitch x 0.025 x 100 ms x 0.001
	EXPECT_FLOAT_EQ(GetState().heartBeatPhase, 37.0f * 0.025f * 0.1f);
	// with a heart: InGame 45, 3D, looping, pitch trunc(the beat's pitch); then the options back to Guidance with loops 0
	s_Heart.citadelHeart = glm::vec3(1.0f, 0.0f, 1.0f);
	HeartBeat(1.0f);
	const auto info = Playing(inGame);
	ASSERT_TRUE(info);
	EXPECT_EQ(info->sample, 45);
	EXPECT_TRUE(info->is3D);
	EXPECT_EQ(GetState().options.loops, 0);
	EXPECT_EQ(GetState().sample.bank, guidanceBank);
	EXPECT_EQ(GetState().options.maxDistance, 500.0f);
	// UpdateHeartBeat every 10 turns: all inputs 0 -> value 0
	UpdateHeartBeat();
	EXPECT_EQ(GetState().heartBeatValue, 0.0f);
	// an enemy creature 200 from my town: 1 - (200 - 100) / 400
	s_Heart.enemyCreatureDistances = {200};
	UpdateHeartBeat();
	EXPECT_FLOAT_EQ(GetState().heartBeatValue, 0.75f);
}

TEST_F(GuidanceTest, HeartBeatRunsEveryTurnTheValueEveryTen)
{
	// HeartBeat(value) also runs off the tenth turns; the value is only read on them
	s_Heart.enemyCreatureDistances = {200};
	s_Turn = 100000;
	UpdateHeartBeat();
	EXPECT_FLOAT_EQ(GetState().heartBeatValue, 0.75f);
	const float pitch10 = GetState().heartBeatPitch;
	EXPECT_FLOAT_EQ(pitch10, (30.0f + 70.0f * 0.75f - 30.0f) * 0.1f + 30.0f);
	const float phase10 = GetState().heartBeatPhase;
	// turn 100001: the input changes but is not read; the beat moves on with the kept 0.75
	s_Heart.enemyCreatureDistances = {};
	s_Turn = 100001;
	UpdateHeartBeat();
	EXPECT_FLOAT_EQ(GetState().heartBeatValue, 0.75f);
	EXPECT_FLOAT_EQ(GetState().heartBeatPitch, (30.0f + 70.0f * 0.75f - pitch10) * 0.1f + pitch10);
	EXPECT_GT(GetState().heartBeatPhase, phase10);
}

TEST(GuidanceMoon, PhaseFromTheRealClock)
{
	// days = time / 86400 - 10962 (2000-01-06, a new moon): phase 2 pi
	EXPECT_FLOAT_EQ(MoonPhase(int64_t {10962} * 86400 + 3600), 6.2831855f);
	// the original's arithmetic with the FPU at 24 bits, emulated:
	// each step rounds to a float, the two double constants do not (the old all-double model gave 3.3044245
	// for day 10976 and 5.0752940 for day 19000)
	EXPECT_EQ(MoonPhase(int64_t {10976} * 86400), 3.30442476272583f);
	EXPECT_EQ(MoonPhase(int64_t {10963} * 86400 + 3600), 6.0704169273376465f);
	EXPECT_EQ(MoonPhase(int64_t {12345} * 86400), 1.0506809949874878f);
	EXPECT_EQ(MoonPhase(int64_t {19000} * 86400), 5.075367450714111f);
	EXPECT_EQ(MoonPhase(int64_t {20000} * 86400), 5.934971809387207f);
	EXPECT_EQ(MoonPhase(int64_t {20363} * 86400), 4.098221302032471f);
}

TEST(GuidanceHelpScript, RunMessagePushesBothTextsAndStartsTheScript)
{
	// RunMessage on a fake VM
	help::HelpSystem help({8, 5}, {}, {});
	std::vector<float> pushed;
	std::string started;
	uint32_t mask = 0;
	std::vector<uint32_t> stopped;
	uint32_t ownerType = 1;
	help::script_control::Vm vm;
	vm.pushFloat = [&pushed](float v) { pushed.push_back(v); };
	vm.startScript = [&started, &mask](std::string_view name, uint32_t m) {
		started = std::string(name);
		mask = m;
	};
	vm.stopTasksOfType = [&stopped](uint32_t m) { stopped.push_back(m); };
	vm.taskType = [&ownerType](uint32_t) { return ownerType; };
	EXPECT_TRUE(help::script_control::RunMessage(help, 4946, 4946, "MultiHelpJustTalkWithText", vm, 77));
	EXPECT_EQ(pushed, (std::vector<float> {4946.0f, 4946.0f}));
	EXPECT_EQ(started, "MultiHelpJustTalkWithText");
	EXPECT_EQ(mask, 0x7Fu);
	EXPECT_EQ(stopped, (std::vector<uint32_t> {0x4A}));
	EXPECT_EQ(help.GetMessageTurn(), 77u);
	// a Script task (type 1) holding the dialogue keeps it: nothing
	help.SetCurrentControl(9);
	pushed.clear();
	EXPECT_FALSE(help::script_control::RunMessage(help, 1, 1, "x", vm, 78));
	EXPECT_TRUE(pushed.empty());
	// a Help task (type 2) holding it is stopped
	ownerType = 2;
	EXPECT_TRUE(help::script_control::RunMessage(help, 1, 1, "x", vm, 79));
	// first > last: nothing
	EXPECT_FALSE(help::script_control::RunMessage(help, 2, 1, "x", vm, 80));
	// the help switch and level: on 1 and level 3 by default; HELP_SYSTEM_ON wants both
	EXPECT_TRUE(help.IsHelpSystemOn());
	EXPECT_EQ(help.GetGuidanceLevel(), 3);
	help.SetHelpOn(0);
	EXPECT_FALSE(help.IsHelpSystemOn());
	EXPECT_EQ(help.GetGuidanceLevel(), 0);
	help.Reset();
	EXPECT_EQ(help.GetHelpOn(), 1u);
}
