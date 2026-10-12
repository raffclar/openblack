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

#include <algorithm>
#include <string_view>
#include <utility>
#include <vector>

#include <LHVM.h>
#include <gtest/gtest.h>

#include "Audio/AudioManagerNoOp.h"
#include "Audio/GameMusic.h"
#include "CHLApi.h"
#include "ECS/Components/Villager.h"
#include "ECS/Registry.h"
#include "ECS/Systems/ScriptObjectsSystemInterface.h"
#include "Locator.h"

using namespace openblack;
using audio::GameMusic;
using audio::MusicType;

namespace
{
/// Every object a script names is itself
class MiscPassThroughScriptObjects final: public ecs::systems::ScriptObjectsSystemInterface
{
public:
	bool Register(entt::entity, bool) override { return true; }
	void EnterNative(uint32_t) override {}
	entt::entity Fetch(entt::entity object) override { return object; }
	void AddReference(entt::entity) override {}
	void RemoveReference(entt::entity) override {}
	void ReleaseFromScript(entt::entity) override {}
	void Replace(entt::entity, entt::entity) override {}
	void Reset() override {}
	void ReleaseUnreferenced() override {}
	bool Disband(entt::entity) override { return false; }
	[[nodiscard]] std::vector<ecs::systems::ScriptObjectPlace> Places() const override { return {}; }
	[[nodiscard]] uint32_t TimesFull() const override { return 0; }
	void ClearForNewLand() override {}
};

using Results = std::vector<std::pair<lhvm::VMValue, lhvm::DataType>>;

class ScriptMiscNatives: public ::testing::Test
{
protected:
	void SetUp() override
	{
		Locator::entitiesRegistry::emplace();
		Locator::scriptObjects::emplace<MiscPassThroughScriptObjects>();
		Locator::vm::emplace();
		Locator::vm::value().Initialise(&_api.GetFunctionsTable(), nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
		// A script's object 0 is none, so nothing the scripts are given is the registry's first entity
		[[maybe_unused]] const auto none = Locator::entitiesRegistry::value().Create();
	}

	void TearDown() override
	{
		Locator::audio::reset();
		Locator::vm::reset();
		Locator::scriptObjects::reset();
		Locator::entitiesRegistry::reset();
	}

	static Results Call(uint32_t native, const std::vector<lhvm::NativeArgument>& arguments)
	{
		Results results;
		EXPECT_TRUE(Locator::vm::value().CallNative(native, arguments, results));
		return results;
	}

	static bool AskBool(uint32_t native, const std::vector<lhvm::NativeArgument>& arguments)
	{
		const auto results = Call(native, arguments);
		EXPECT_EQ(results.size(), 1u);
		return !results.empty() && results.front().first.floatVal != 0.0f;
	}

	static lhvm::NativeArgument Object(entt::entity entity)
	{
		return std::pair(lhvm::VMValue(static_cast<uint32_t>(entity)), lhvm::DataType::Object);
	}

	static lhvm::NativeArgument Float(float value) { return std::pair(lhvm::VMValue(value), lhvm::DataType::Float); }

	static lhvm::NativeArgument Int(int32_t value) { return std::pair(lhvm::VMValue(value), lhvm::DataType::Int); }

	static entt::entity CreateVillager(ecs::components::Villager::Sex sex)
	{
		auto& registry = Locator::entitiesRegistry::value();
		const auto entity = registry.Create();
		ecs::components::Villager villager {};
		villager.sex = sex;
		registry.Assign<ecs::components::Villager>(entity, villager);
		return entity;
	}

	chlapi::CHLApi _api;
};

constexpr uint32_t k_RestartMusic = 190;
constexpr uint32_t k_MusicPlayedOnObject = 191;
constexpr uint32_t k_ImmersionExists = 304;
constexpr uint32_t k_LastMusicLine = 335;
constexpr uint32_t k_MusicPlayed = 350;
constexpr uint32_t k_SexIsMale = 358;
constexpr uint32_t k_StopSoundEffect = 424;
constexpr uint32_t k_StartImmersion = 429;
constexpr uint32_t k_StopImmersion = 430;
constexpr uint32_t k_AttachSoundTag = 447;
constexpr uint32_t k_DetachSoundTag = 448;
constexpr uint32_t k_GameSoundPlaying = 450;
} // namespace

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(ScriptMiscNatives, TakeAndGiveWhatTheGameDoes)
{
	struct Expected
	{
		uint32_t native;
		std::string_view name;
		int stackIn;
		uint32_t stackOut;
	};
	constexpr auto k_Natives = std::to_array<Expected>({
	    {k_RestartMusic, "RESTART_MUSIC", 1, 0},
	    {k_MusicPlayedOnObject, "MUSIC_PLAYED", 1, 1},
	    {k_ImmersionExists, "IMMERSION_EXISTS", 0, 1},
	    {k_LastMusicLine, "LAST_MUSIC_LINE", 1, 1},
	    {k_MusicPlayed, "MUSIC_PLAYED", 1, 1},
	    {k_SexIsMale, "SEX_IS_MALE", 1, 1},
	    {k_StopSoundEffect, "STOP_SOUND_EFFECT", 3, 0},
	    {k_StartImmersion, "START_IMMERSION", 1, 0},
	    {k_StopImmersion, "STOP_IMMERSION", 1, 0},
	    {k_AttachSoundTag, "ATTACH_SOUND_TAG", 4, 0},
	    {k_DetachSoundTag, "DETACH_SOUND_TAG", 3, 0},
	    {k_GameSoundPlaying, "GAME_SOUND_PLAYING", 2, 1},
	});
	const auto& table = _api.GetFunctionsTable();
	for (const auto& expected : k_Natives)
	{
		ASSERT_LT(expected.native, table.size());
		EXPECT_EQ(table[expected.native].name, expected.name);
		EXPECT_EQ(table[expected.native].stackIn, expected.stackIn) << expected.name;
		EXPECT_EQ(table[expected.native].stackOut, expected.stackOut) << expected.name;
	}
}

// The Immersion Mushrooms and the opening's force-feedback teasing ask for a device openblack never has
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(ScriptMiscNatives, ThereIsNoForceFeedbackDevice)
{
	EXPECT_FALSE(AskBool(k_ImmersionExists, {}));
	EXPECT_TRUE(Call(k_StartImmersion, {Int(16)}).empty());
	EXPECT_TRUE(Call(k_StopImmersion, {Int(16)}).empty());
	EXPECT_EQ(_api.GetStubCalls().Total(), 0u);
}

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(ScriptMiscNatives, OnlyAVillagerCanBeFemale)
{
	EXPECT_TRUE(AskBool(k_SexIsMale, {Object(CreateVillager(ecs::components::Villager::Sex::MALE))}));
	EXPECT_FALSE(AskBool(k_SexIsMale, {Object(CreateVillager(ecs::components::Villager::Sex::FEMALE))}));
	// Anything else answers male, a thing that has gone too
	EXPECT_TRUE(AskBool(k_SexIsMale, {Object(Locator::entitiesRegistry::value().Create())}));
	EXPECT_TRUE(AskBool(k_SexIsMale, {Object(entt::null)}));
}

// Without a sound device, the scripts waiting on music wait on the help text instead; with no text shown there is
// nothing to wait for
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(ScriptMiscNatives, WithoutASoundDeviceMusicWaitsOnTheText)
{
	EXPECT_TRUE(AskBool(k_LastMusicLine, {Float(5.0f)}));
	EXPECT_TRUE(AskBool(k_MusicPlayed, {Int(static_cast<int32_t>(MusicType::ScriptMissionariesVerse1))}));
	Locator::audio::emplace<audio::AudioManagerNoOp>();
	EXPECT_TRUE(AskBool(k_LastMusicLine, {Float(5.0f)}));
	EXPECT_TRUE(AskBool(k_MusicPlayed, {Int(static_cast<int32_t>(MusicType::ScriptMissionariesVerse1))}));
	// Nothing sounds without one
	EXPECT_FALSE(AskBool(k_GameSoundPlaying, {Int(127), Int(5)}));
	EXPECT_TRUE(Call(k_StopSoundEffect, {Int(0), Int(91), Int(5)}).empty());
}

// An object without music counts as finished, before the game's music has started too
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(ScriptMiscNatives, AnObjectWithoutMusicHasFinished)
{
	const auto object = Locator::entitiesRegistry::value().Create();
	EXPECT_TRUE(AskBool(k_MusicPlayedOnObject, {Object(object)}));
	EXPECT_TRUE(Call(k_RestartMusic, {Object(object)}).empty());
}

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST(ScriptMusic, CuesInTheMusicMarkItsLinesAndWords)
{
	GameMusic::MusicLyrics lyrics;
	GameMusic::ApplyMusicCue(lyrics, "L1");
	EXPECT_EQ(lyrics.line, 1);
	EXPECT_EQ(lyrics.word, 1);
	GameMusic::ApplyMusicCue(lyrics, "W");
	GameMusic::ApplyMusicCue(lyrics, "p");
	EXPECT_EQ(lyrics.word, 3);
	GameMusic::ApplyMusicCue(lyrics, "l12");
	EXPECT_EQ(lyrics.line, 12);
	EXPECT_EQ(lyrics.word, 1);
	// Other cues change nothing
	GameMusic::ApplyMusicCue(lyrics, "X3");
	GameMusic::ApplyMusicCue(lyrics, "");
	EXPECT_EQ(lyrics.line, 12);
	EXPECT_EQ(lyrics.word, 1);
	// A line cue without a number is line 0
	GameMusic::ApplyMusicCue(lyrics, "L");
	EXPECT_EQ(lyrics.line, 0);
}

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST(ScriptMusic, ALineIsReachedOnceItsCueOrALaterOneIsHeard)
{
	EXPECT_TRUE(GameMusic::ReachedMusicLine(0, 0));
	EXPECT_FALSE(GameMusic::ReachedMusicLine(0, 1));
	EXPECT_TRUE(GameMusic::ReachedMusicLine(3, 3));
	EXPECT_TRUE(GameMusic::ReachedMusicLine(4, 3));
	// Compared unsigned: a negative line is never reached
	EXPECT_FALSE(GameMusic::ReachedMusicLine(12, -1));
}

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST(ScriptMusic, StartingMusicStartsItsLyricsAgain)
{
	GameMusic music;
	music.OnScriptMusicCue("L4");
	music.OnScriptMusicCue("W");
	EXPECT_EQ(music.GetMusicLyrics().line, 4);
	EXPECT_EQ(music.GetMusicLyrics().word, 2);
	music.ResetMusicLyrics();
	EXPECT_EQ(music.GetMusicLyrics().line, 0);
	EXPECT_EQ(music.GetMusicLyrics().word, 0);
}

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST(ScriptMusic, ScriptMusicIsOverOnceOtherMusicOrNoneIsAskedFor)
{
	GameMusic music;
	EXPECT_TRUE(music.IsScriptMusicOver(MusicType::ScriptMissionariesVerse1));
	music.StartScriptMusic(MusicType::ScriptMissionariesVerse1);
	EXPECT_FALSE(music.IsScriptMusicOver(MusicType::ScriptMissionariesVerse1));
	EXPECT_TRUE(music.IsScriptMusicOver(MusicType::ScriptMissionariesVerse2));
	music.StartScriptMusic(MusicType::None);
	EXPECT_TRUE(music.IsScriptMusicOver(MusicType::ScriptMissionariesVerse1));
}

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST(ScriptMusic, AttachedMusicFinishesOnlyWhenTakenOff)
{
	GameMusic music;
	EXPECT_TRUE(music.IsAttachedMusicFinished(7));
	music.AttachMusic(7, MusicType::ScriptMissionariesBackground);
	EXPECT_FALSE(music.IsAttachedMusicFinished(7));
	// Starting it again leaves it attached
	music.RestartAttachedMusic(7);
	EXPECT_FALSE(music.IsAttachedMusicFinished(7));
	music.DetachMusic(7);
	EXPECT_TRUE(music.IsAttachedMusicFinished(7));
}
