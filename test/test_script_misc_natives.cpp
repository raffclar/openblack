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
#include <memory>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include <LHVM.h>
#include <gtest/gtest.h>
#include <spdlog/sinks/ringbuffer_sink.h>
#include <spdlog/spdlog.h>

#include "Audio/AudioManagerNoOp.h"
#include "Audio/GameSoundRequests.h"
#include "CHLApi.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/Scaffold.h"
#include "ECS/Components/Villager.h"
#include "ECS/Registry.h"
#include "ECS/Systems/ScriptObjectsSystemInterface.h"
#include "ECS/Systems/WorkshopSystemInterface.h"
#include "Locator.h"
#include "Sound/GameMusic.h"

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

/// A workshop system that only records the scaffolds that a script starts at once, and for which player
class FakeWorkshops final: public ecs::systems::WorkshopSystemInterface
{
public:
	void ProcessWorkshop(entt::entity) override {}
	[[nodiscard]] int32_t RoomOf(entt::entity) const override { return 0; }
	entt::entity WoodPileOf(entt::entity) override { return entt::null; }
	void Made(entt::entity) override {}
	entt::entity CreateScaffold(glm::vec3, entt::entity, entt::entity, float, float) override { return entt::null; }
	entt::entity RewardScaffold(entt::entity, glm::vec3) override { return entt::null; }
	[[nodiscard]] bool CanTake(entt::entity) const override { return false; }
	void TakenByHand(entt::entity, PlayerNames) override {}
	void LetGoByHand(entt::entity, bool) override {}
	void NotPutDownByHand(entt::entity) override {}
	void CameToRest(entt::entity, bool, bool) override {}
	[[nodiscard]] bool CanMove(entt::entity) const override { return false; }
	void ProcessHeld(entt::entity) override {}
	void SiteClosed(entt::entity) override {}
	entt::entity LeaveDestroyedSite(entt::entity) override { return entt::null; }
	bool ForceBuild(entt::entity scaffold, std::optional<PlayerNames> player) override
	{
		built.emplace_back(scaffold, player);
		return true;
	}
	void PlaceGhostsForFrame(std::optional<glm::vec3>) override {}
	void UpdateFlagsForFrame(float) override {}
	[[nodiscard]] bool CanJoin(entt::entity, entt::entity) const override { return false; }
	void Join(entt::entity, entt::entity, std::optional<glm::vec3>) override {}
	[[nodiscard]] bool GoesIntoStore(entt::entity, entt::entity) const override { return false; }
	bool GiveToStore(entt::entity, entt::entity, PlayerNames) override { return false; }
	entt::entity SplitOffRest(entt::entity) override { return entt::null; }
	[[nodiscard]] bool ValidToTap(entt::entity) const override { return false; }
	bool Tap(entt::entity, glm::vec3) override { return false; }
	[[nodiscard]] std::optional<ecs::components::ScaffoldOffer> OfferAt(entt::entity, glm::vec3, uint32_t) override
	{
		return std::nullopt;
	}
	[[nodiscard]] entt::entity TownForBuilding(glm::vec3) const override { return entt::null; }

	std::vector<std::pair<entt::entity, std::optional<PlayerNames>>> built;
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
		Locator::workshopSystem::reset();
		Locator::sounds::reset();
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

constexpr uint32_t k_GameThingClicked = 16;
constexpr uint32_t k_GetProperty = 21;
constexpr uint32_t k_RestartMusic = 190;
constexpr uint32_t k_SetActive = 255;
constexpr uint32_t k_SetAnimationModify = 202;
constexpr uint32_t k_MusicPlayedOnObject = 191;
constexpr uint32_t k_ImmersionExists = 304;
constexpr uint32_t k_LastMusicLine = 335;
constexpr uint32_t k_MusicPlayed = 350;
constexpr uint32_t k_SexIsMale = 358;
constexpr uint32_t k_StopSoundEffect = 424;
constexpr uint32_t k_StartImmersion = 429;
constexpr uint32_t k_StopImmersion = 430;
constexpr uint32_t k_StopAllImmersion = 431;
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
	    {k_StopAllImmersion, "STOP_ALL_IMMERSION", 0, 0},
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
	EXPECT_TRUE(Call(k_StopAllImmersion, {}).empty());
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
	Locator::sounds::emplace<audio::GameSoundRequests>();
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
	audio::MusicLyrics lyrics;
	audio::ApplyMusicCue(lyrics, "L1");
	EXPECT_EQ(lyrics.line, 1);
	EXPECT_EQ(lyrics.word, 1);
	audio::ApplyMusicCue(lyrics, "W");
	audio::ApplyMusicCue(lyrics, "p");
	EXPECT_EQ(lyrics.word, 3);
	audio::ApplyMusicCue(lyrics, "l12");
	EXPECT_EQ(lyrics.line, 12);
	EXPECT_EQ(lyrics.word, 1);
	// Other cues change nothing
	audio::ApplyMusicCue(lyrics, "X3");
	audio::ApplyMusicCue(lyrics, "");
	EXPECT_EQ(lyrics.line, 12);
	EXPECT_EQ(lyrics.word, 1);
	// A line cue without a number is line 0
	audio::ApplyMusicCue(lyrics, "L");
	EXPECT_EQ(lyrics.line, 0);
}

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST(ScriptMusic, ALineIsReachedOnceItsCueOrALaterOneIsHeard)
{
	EXPECT_TRUE(audio::ReachedMusicLine(0, 0));
	EXPECT_FALSE(audio::ReachedMusicLine(0, 1));
	EXPECT_TRUE(audio::ReachedMusicLine(3, 3));
	EXPECT_TRUE(audio::ReachedMusicLine(4, 3));
	// Compared unsigned: a negative line is never reached
	EXPECT_FALSE(audio::ReachedMusicLine(12, -1));
}

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST(ScriptMusic, StartingMusicStartsItsLyricsAgain)
{
	audio::ScriptMusic music;
	music.Restore(MusicType::None, false, {.line = 4, .word = 2});
	music.ResetLyrics();
	EXPECT_EQ(music.GetLyrics().line, 0);
	EXPECT_EQ(music.GetLyrics().word, 0);
}

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST(ScriptMusic, TheEndOfADialogueStartsTheWordAgainAndKeepsTheLine)
{
	audio::ScriptMusic music;
	music.Restore(MusicType::None, false, {.line = 4, .word = 2});
	music.ResetWord();
	EXPECT_EQ(music.GetLyrics().line, 4);
	EXPECT_EQ(music.GetLyrics().word, 0);
}

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST(ScriptMusic, ScriptMusicIsOverOnceOtherMusicOrNoneIsAskedFor)
{
	audio::ScriptMusic music;
	EXPECT_TRUE(music.IsOver(MusicType::ScriptMissionariesVerse1));
	music.Start(MusicType::ScriptMissionariesVerse1);
	EXPECT_FALSE(music.IsOver(MusicType::ScriptMissionariesVerse1));
	EXPECT_TRUE(music.IsOver(MusicType::ScriptMissionariesVerse2));
	music.Start(MusicType::None);
	EXPECT_TRUE(music.IsOver(MusicType::ScriptMissionariesVerse1));
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

// SET_ANIMATION_MODIFY sets real time on a creature's body only for the value 1. Other values clear it, and a thing that
// is not a creature does not change.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(ScriptMiscNatives, OnlyOneSetsRealTimeOnACreature)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto creature = registry.Create();
	registry.Assign<ecs::components::Creature>(creature);
	registry.Assign<ecs::components::CreatureAnimation>(creature);
	const auto& animation = registry.Get<const ecs::components::CreatureAnimation>(creature);
	EXPECT_TRUE(Call(k_SetAnimationModify, {Int(1), Object(creature)}).empty());
	EXPECT_TRUE(animation.realTime);
	EXPECT_TRUE(Call(k_SetAnimationModify, {Int(2), Object(creature)}).empty());
	EXPECT_FALSE(animation.realTime);
	EXPECT_TRUE(Call(k_SetAnimationModify, {Int(1), Object(creature)}).empty());
	EXPECT_TRUE(Call(k_SetAnimationModify, {Int(0), Object(creature)}).empty());
	EXPECT_FALSE(animation.realTime);
	// A villager is not a creature
	const auto villager = CreateVillager(ecs::components::Villager::Sex::MALE);
	registry.Assign<ecs::components::CreatureAnimation>(villager);
	EXPECT_TRUE(Call(k_SetAnimationModify, {Int(1), Object(villager)}).empty());
	EXPECT_FALSE(registry.Get<const ecs::components::CreatureAnimation>(villager).realTime);
	EXPECT_TRUE(Call(k_SetAnimationModify, {Int(1), Object(entt::null)}).empty());
	EXPECT_EQ(_api.GetStubCalls().Total(), 0u);
}

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(ScriptMiscNatives, AThingThatIsGoneIsNotClickedAndTheLogStaysQuiet)
{
	// A script can ask on each turn whether a thing that is gone is clicked. The game answers no and says nothing.
	const auto sink = std::make_shared<spdlog::sinks::ringbuffer_sink_mt>(8);
	const auto logger = std::make_shared<spdlog::logger>("scripting", sink);
	logger->set_level(spdlog::level::info);
	// Another test in the same program may have left a logger of this name
	spdlog::drop("scripting");
	spdlog::register_logger(logger);
	auto& registry = Locator::entitiesRegistry::value();
	const auto gone = registry.Create();
	registry.Destroy(gone);
	for (int turn = 0; turn < 3; ++turn)
	{
		EXPECT_FALSE(AskBool(k_GameThingClicked, {Object(gone)}));
	}
	const auto property = Call(k_GetProperty, {Int(0), Object(gone)});
	const auto lines = sink->last_raw();
	spdlog::drop("scripting");
	EXPECT_TRUE(lines.empty());
	ASSERT_EQ(property.size(), 1u);
	EXPECT_EQ(property.front().first.floatVal, 0.0f);
}

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(ScriptMiscNatives, AScaffoldSetActiveStartsItsBuildingAtOnceForNoPlayer)
{
	auto& workshops = static_cast<FakeWorkshops&>(Locator::workshopSystem::emplace<FakeWorkshops>());
	auto& registry = Locator::entitiesRegistry::value();
	const auto scaffold = registry.Create();
	registry.Assign<ecs::components::Scaffold>(scaffold);

	// Set inactive, the scaffold does nothing
	Call(k_SetActive, {Int(0), Object(scaffold)});
	EXPECT_TRUE(workshops.built.empty());

	Call(k_SetActive, {Int(1), Object(scaffold)});
	ASSERT_EQ(workshops.built.size(), 1u);
	EXPECT_EQ(workshops.built.front().first, scaffold);
	EXPECT_FALSE(workshops.built.front().second.has_value());
}
