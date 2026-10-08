/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cctype>
#include <cstring>

#include <algorithm>
#include <array>
#include <deque>
#include <filesystem>
#include <fstream>
#include <map>
#include <optional>
#include <string>
#include <vector>

#ifdef _WIN32
#include <process.h>
#else
#include <unistd.h>
#endif

#include <PackFile.h>
#include <gtest/gtest.h>

#include "3D/MapCoords.h"
#include "Audio/Engine/MusicBank.h"
#include "Audio/Engine/MusicEngine.h"
#include "Audio/Services/GameMusic.h"
#include "Audio/Services/ScriptAudioState.h"
#include "InfoConstants.h"
#include "support/TestServices.h"

// The game's music (GameMusic) over the music engine (MusicEngine) and a fake mixer: the script music and its
// callbacks, the alignment, tribe, chant and citadel music, the music attached to objects and the script's audio
// switches. Most banks are small .sad files written here with the layout in docs/bw1-notes/audio.md; the tests that
// read the game's (Script03, MissionariesVerse1, PiperTune_M, info.dat) need OPENBLACK_TEST_BW_ROOT and skip without
// it.

using namespace openblack;
using namespace openblack::audio;

namespace
{
/// MusicBank::Register reads through the file system: a DefaultFileSystem for the call, the one before put back
/// after it (the bank keeps its own open file)
std::unique_ptr<MusicBank> RegisterMusicBank(const std::filesystem::path& path)
{
	const openblack::test::ScopedDefaultFileSystem fileSystem;
	return MusicBank::Register(path);
}

/// The running test's suite and name and the process id: parallel test processes never share a temporary file
std::string UniqueSuffix()
{
	const auto* info = ::testing::UnitTest::GetInstance()->current_test_info();
#ifdef _WIN32
	const auto pid = _getpid();
#else
	const auto pid = getpid();
#endif
	return std::string(info->test_suite_name()) + "_" + info->name() + "_" + std::to_string(pid);
}

std::string Lower(std::string s)
{
	std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	return s;
}

std::optional<std::filesystem::path> GameRoot()
{
	const char* root = std::getenv("OPENBLACK_TEST_BW_ROOT");
	if (root == nullptr || *root == '\0' || !std::filesystem::is_directory(root))
	{
		return std::nullopt;
	}
	return std::filesystem::path(root);
}

// The original runs on a case-insensitive file system: resolve each component without case
std::optional<std::filesystem::path> FindNoCase(const std::filesystem::path& root, std::string_view relative)
{
	auto current = root;
	for (const auto& part : std::filesystem::path(relative))
	{
		const auto wanted = Lower(part.string());
		std::optional<std::filesystem::path> found;
		std::error_code ec;
		for (const auto& entry : std::filesystem::directory_iterator(current, ec))
		{
			if (Lower(entry.path().filename().string()) == wanted)
			{
				found = entry.path();
				break;
			}
		}
		if (!found)
		{
			return std::nullopt;
		}
		current = *found;
	}
	return current;
}

struct BankSpec
{
	int segments {8};
	int group {0};
	uint32_t flags {0};
	int loops {0};
	int volume {127};
	float maxDistance {0.0f};              // set on the first segment
	std::vector<std::string> descriptions; // each segment's description (markers)
};

void WriteBlock(std::ofstream& out, const char* name, const std::vector<uint8_t>& data)
{
	std::array<char, 32> blockName {};
	std::strncpy(blockName.data(), name, blockName.size() - 1);
	out.write(blockName.data(), blockName.size());
	const auto size = static_cast<uint32_t>(data.size());
	out.write(reinterpret_cast<const char*>(&size), sizeof(size));
	out.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
}

void Put32(std::vector<uint8_t>& v, size_t offset, uint32_t value)
{
	std::memcpy(&v[offset], &value, sizeof(value));
}

// LiOnHeAd with LHFileSegmentBankInfo (3rd u32 = 1: music), LHAudioWaveData (4 bytes per segment: its index) and
// LHAudioBankSampleTable (n x 0x280)
std::filesystem::path WriteBank(const std::string& name, const BankSpec& spec)
{
	const auto path = std::filesystem::path(TEST_BINARY_DIR) / ("game_music_" + name + "_" + UniqueSuffix() + ".sad");
	std::ofstream out(path, std::ios::binary | std::ios::trunc);
	out.write("LiOnHeAd", 8);
	WriteBlock(out, "LHFileSegmentBankInfo", {0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0});
	std::vector<uint8_t> wave(static_cast<size_t>(spec.segments) * 4);
	for (int i = 0; i < spec.segments; ++i)
	{
		Put32(wave, static_cast<size_t>(i) * 4, static_cast<uint32_t>(i));
	}
	WriteBlock(out, "LHAudioWaveData", wave);
	std::vector<uint8_t> table(4 + static_cast<size_t>(spec.segments) * MusicBank::k_RecordSize);
	Put32(table, 0, static_cast<uint32_t>(spec.segments));
	for (int i = 0; i < spec.segments; ++i)
	{
		const size_t r = 4 + static_cast<size_t>(i) * MusicBank::k_RecordSize;
		Put32(table, r + 0x10C, 4);
		Put32(table, r + 0x110, static_cast<uint32_t>(i) * 4);
		Put32(table, r + 0x128, 22050);
		if (static_cast<size_t>(i) < spec.descriptions.size())
		{
			std::memcpy(&table[r + 0x140], spec.descriptions[static_cast<size_t>(i)].c_str(),
			            spec.descriptions[static_cast<size_t>(i)].size());
		}
		if (i == 0)
		{
			Put32(table, r + 0x118, static_cast<uint32_t>(spec.group));
			Put32(table, r + 0x244, spec.flags);
			Put32(table, r + 0x248, static_cast<uint32_t>(spec.loops));
			Put32(table, r + 0x25C, static_cast<uint32_t>(spec.volume));
			uint32_t maxDistance = 0;
			std::memcpy(&maxDistance, &spec.maxDistance, sizeof(maxDistance));
			Put32(table, r + 0x26C, maxDistance);
		}
	}
	WriteBlock(out, "LHAudioBankSampleTable", table);
	return path;
}

// A mixer without sound: the queue of each channel (the segment index is the data)
class FakeSink final: public IMusicSink
{
public:
	struct Chunk
	{
		uint32_t segment;
		bool last;
	};

	void SetListener(IMusicChunkListener* listener) override { _listener = listener; }
	void EnableChannel(int channel, bool is3D, const MusicDistanceMapping&, glm::vec3) override
	{
		enabled3D[static_cast<size_t>(channel)] = is3D;
	}
	void QueueChunk(int channel, const std::vector<uint8_t>& segment, bool, uint32_t, bool last) override
	{
		uint32_t index = 0;
		std::memcpy(&index, segment.data(), sizeof(index));
		queues[static_cast<size_t>(channel)].push_back({index, last});
	}
	void SetCentred(int) override {}
	void SetFrequency(int, uint32_t) override {}
	void SetVolume(int, uint32_t) override {}
	bool IsChannelDone(int channel) override { return queues[static_cast<size_t>(channel)].empty(); }
	void FlushChannel(int channel) override
	{
		auto flushed = std::move(queues[static_cast<size_t>(channel)]);
		queues[static_cast<size_t>(channel)].clear();
		for (const auto& chunk : flushed)
		{
			_listener->OnChunkDone(channel, chunk.last);
		}
	}
	uint32_t GetPlayPosition(int) override { return 0; }
	void SetSourcePosition(int channel, glm::vec3 position) override { positions[static_cast<size_t>(channel)] = position; }
	void PauseChannel(int) override {}
	void RestartChannel(int) override {}

	/// The oldest queued chunk of a channel has played; false if nothing was queued
	bool PlayOne(int channel)
	{
		auto& queue = queues[static_cast<size_t>(channel)];
		if (queue.empty())
		{
			return false;
		}
		const bool last = queue.front().last;
		queue.pop_front();
		_listener->OnChunkDone(channel, last);
		return true;
	}

	std::array<std::deque<Chunk>, k_MusicChannelCount> queues;
	std::array<bool, k_MusicChannelCount> enabled3D {};
	std::array<glm::vec3, k_MusicChannelCount> positions {};

private:
	IMusicChunkListener* _listener {nullptr};
};

class GameMusicTest: public ::testing::Test
{
protected:
	void TearDown() override
	{
		music.reset();
		banks.clear();
		for (const auto& file : files)
		{
			std::error_code ec;
			std::filesystem::remove(file, ec);
		}
	}

	MusicBank* AddBank(MusicType type, const BankSpec& spec = {})
	{
		files.push_back(WriteBank(std::to_string(static_cast<int>(type)), spec));
		return AddBank(type, files.back());
	}
	MusicBank* AddBank(MusicType type, const std::filesystem::path& path)
	{
		auto bank = RegisterMusicBank(path);
		EXPECT_NE(bank, nullptr);
		if (!bank)
		{
			return nullptr;
		}
		engine.NoteBankRegistered(*bank); // registers the bank's groups
		auto* raw = bank.get();
		banks[static_cast<int>(type)] = std::move(bank);
		return raw;
	}

	/// The GameMusic over the banks added so far
	void Make(bool withEngine = true)
	{
		GameQueries queries;
		queries.landNumber = [this]() { return land; };
		queries.turn = [this]() { return turn; };
		queries.videoPlaying = [this]() { return video; };
		queries.camera = [this]() { return camera; };
		queries.scriptWideScreen = [this]() { return wideScreen; };
		queries.cameraAlignment = [this]() { return alignment; };
		queries.insideCitadel = [this]() { return insideCitadel; };
		queries.localPlayerAlignment = [this]() { return playerAlignment; };
		queries.nearestTown = [this](float maxDistance) -> std::optional<MusicTown> {
			lastTownSearch = maxDistance;
			return nearest;
		};
		queries.town = [this](uint32_t id) -> std::optional<MusicTown> {
			const auto it = towns.find(id);
			return it != towns.end() ? std::optional<MusicTown>(it->second) : std::nullopt;
		};
		queries.thingPosition = [this](ThingId thing) -> std::optional<glm::vec3> {
			const auto it = things.find(thing);
			return it != things.end() ? std::optional<glm::vec3>(it->second) : std::nullopt;
		};
		queries.chantSite = [this]() { return chant; };
		music = std::make_unique<GameMusic>(
		    withEngine ? &engine : nullptr,
		    [this](MusicType type) -> MusicBank* {
			    const auto it = banks.find(static_cast<int>(type));
			    return it != banks.end() ? it->second.get() : nullptr;
		    },
		    script, queries, GameMusic::TownTrigger {300.0f, 400.0f});
	}

	/// One pass of the music thread, 120 ms after the previous one
	void Pass()
	{
		now += k_MusicPassSleepMs;
		engine.Process(now);
	}

	/// The channel of a bank (k_NoMusicChannel if none)
	int ChannelOf(MusicType type) const { return engine.FindChannelOfBank(banks.at(static_cast<int>(type)).get()); }

	/// Play every chunk of a channel until it has ended and been freed (the end of track callback has run)
	void PlayToTheEnd(int channel)
	{
		for (int i = 0; i < 10000 && engine.GetChannel(channel).status != MusicStatus::Free; ++i)
		{
			Pass();
			while (sink.PlayOne(channel))
			{
			}
		}
		Pass();
	}

	FakeSink sink;
	MusicEngine engine {sink};
	ScriptAudioState script;
	std::map<int, std::unique_ptr<MusicBank>> banks;
	std::vector<std::filesystem::path> files;
	std::unique_ptr<GameMusic> music;
	uint32_t now {1000};

	int land {1};
	uint32_t turn {100};
	bool video {false};
	bool wideScreen {false};
	std::optional<CameraState> camera {CameraState {}};
	float alignment {0.0f};
	bool insideCitadel {false};
	float playerAlignment {0.0f};
	std::optional<MusicTown> nearest;
	float lastTownSearch {0.0f};
	std::map<uint32_t, MusicTown> towns;
	std::map<ThingId, glm::vec3> things;
	std::optional<ChantSite> chant;
};
} // namespace

// --- The script's audio switches -------------------------------------------------------------------------------

TEST(ScriptAudioState, ResetValues)
{
	ScriptAudioState state;
	state.creatureSound = 0;
	state.gameSoundOff = 1;
	state.alignmentMusic = 0;
	state.musicLine = 7;
	state.musicBeat = 9;
	state.Reset();
	EXPECT_EQ(state.creatureSound, 1);
	EXPECT_EQ(state.gameSoundOff, 0);
	EXPECT_EQ(state.alignmentMusic, 1);
	EXPECT_EQ(state.musicLine, 0u);
	EXPECT_EQ(state.musicBeat, 0);
}

TEST(ScriptAudioState, EndDialogue)
{
	ScriptAudioState state;
	state.creatureSound = 0;
	state.musicLine = 4;
	state.musicBeat = 3;
	state.EndDialogue();
	EXPECT_EQ(state.creatureSound, 1);
	EXPECT_EQ(state.musicBeat, 0);
	EXPECT_EQ(state.musicLine, 4u); // untouched
}

// --- The script music ----------------------------------------------------------------------------------------------

TEST_F(GameMusicTest, ScriptMusicPlaysOnce2DFromChunk1)
{
	AddBank(MusicType::ScriptGeneric03, BankSpec {.segments = 4, .flags = 0x20, .volume = 65});
	Make();
	script.musicLine = 5;
	script.musicBeat = 2;
	music->ScriptStartMusic(static_cast<int>(MusicType::ScriptGeneric03));
	EXPECT_EQ(script.musicLine, 0u);
	EXPECT_EQ(script.musicBeat, 0);
	EXPECT_EQ(music->GetScriptType(), 59);
	EXPECT_EQ(music->ScriptMusicPlayed(59), std::optional<bool>(false));

	music->ProcessMusic();
	const int channel = ChannelOf(MusicType::ScriptGeneric03);
	ASSERT_NE(channel, k_NoMusicChannel);
	const auto& ch = engine.GetChannel(channel);
	EXPECT_EQ(ch.target, 127);   // full volume
	EXPECT_EQ(ch.sadVolume, 65); // the .sad's (flag 0x20)
	EXPECT_EQ(ch.current, 127);  // no fade: at the volume at once
	EXPECT_EQ(ch.is3D, 0);
	EXPECT_EQ(ch.startChunk, 1);
	EXPECT_EQ(ch.sync, 0);
	EXPECT_EQ(music->GetScriptStarted(), 1);
	EXPECT_EQ(music->GetAlignmentType(), -1);
	EXPECT_EQ(music->GetPlayingMessage(), "Music Playing=MUSIC_TYPE_SCRIPT_GENERIC_03");

	// every turn after: already started, nothing is played again
	music->ProcessMusic();
	EXPECT_EQ(ChannelOf(MusicType::ScriptGeneric03), channel);

	// the end of the track: MUSIC_PLAYED becomes true, and the next turn the script music stops
	PlayToTheEnd(channel);
	EXPECT_EQ(music->GetScriptType(), 0);
	EXPECT_EQ(music->ScriptMusicPlayed(59), std::optional<bool>(true));
	music->ProcessMusic();
	EXPECT_EQ(music->GetScriptStarted(), 0);
}

TEST_F(GameMusicTest, StartMusicRestartsTheSameType)
{
	AddBank(MusicType::ScriptEpic01);
	Make();
	music->StartScriptMusic(61);
	music->ProcessMusic();
	EXPECT_EQ(music->GetScriptStarted(), 1);
	music->StartScriptMusic(61); // clears the started flag even for the same type
	EXPECT_EQ(music->GetScriptStarted(), 0);
	music->ProcessMusic();
	EXPECT_EQ(music->GetScriptStarted(), 1);
}

TEST_F(GameMusicTest, StopMusicFadesOut)
{
	AddBank(MusicType::ScriptEpic01);
	Make();
	music->ScriptStartMusic(61);
	music->ProcessMusic();
	const int channel = ChannelOf(MusicType::ScriptEpic01);
	ASSERT_NE(channel, k_NoMusicChannel);
	music->ScriptStopMusic(); // StartScriptMusic(0)
	EXPECT_EQ(music->GetScriptType(), 0);
	EXPECT_EQ(music->ScriptMusicPlayed(61), std::optional<bool>(true));
	EXPECT_EQ(music->GetScriptStarted(), 1); // StartScriptMusic(0) does not clear the started flag
	turn = 5;                                // no alignment music
	music->ProcessMusic();
	// the script music fades out, then ProcessMusic finds nothing and fades it out again
	EXPECT_EQ(music->GetScriptStarted(), 0);
	EXPECT_EQ(engine.GetChannel(channel).target, 0);
	EXPECT_EQ(music->GetPlayingMessage(), "Music Playing=NONE");
}

TEST_F(GameMusicTest, MissingBankYields)
{
	Make();
	music->ScriptStartMusic(56); // WELCOME_DANCE: no file, no bank
	turn = 5;
	music->ProcessMusic();
	EXPECT_EQ(music->GetScriptStarted(), 0);
	EXPECT_EQ(music->GetScriptType(), 56);
	EXPECT_EQ(music->ScriptMusicPlayed(56), std::optional<bool>(false));
}

TEST_F(GameMusicTest, NoMusicOnLand6OrWithoutEngine)
{
	AddBank(MusicType::ScriptEpic01);
	land = 6;
	Make();
	music->ScriptStartMusic(61);
	music->ProcessMusic(); // no music on land 6
	EXPECT_EQ(music->GetScriptStarted(), 0);

	music.reset();
	Make(false);
	music->ScriptStartMusic(61);
	EXPECT_EQ(music->ScriptMusicPlayed(61), std::nullopt); // TEXT_READ instead
	EXPECT_EQ(music->ScriptLastMusicLine(1.0f), std::nullopt);
	music->UpdateTurn(true);
	EXPECT_EQ(music->GetScriptStarted(), 0);
}

TEST_F(GameMusicTest, VideoKeepsWhatPlays)
{
	AddBank(MusicType::ScriptEpic01);
	Make();
	music->ScriptStartMusic(61);
	music->ProcessMusic();
	const int channel = ChannelOf(MusicType::ScriptEpic01);
	video = true;
	music->ProcessMusic(); // started flag cleared, no alignment type, nothing stopped
	EXPECT_EQ(music->GetScriptStarted(), 0);
	EXPECT_EQ(engine.GetChannel(channel).target, 127);
}

TEST_F(GameMusicTest, MarkersSetTheLineAndTheBeats)
{
	// chunk 1 sample 100 "L3", chunk 2 sample 0 "P" and "w"; LAST_MUSIC_LINE compares >=
	AddBank(MusicType::ScriptMissionariesVerse1, BankSpec {.segments = 4, .descriptions = {"!100=L3", "!0=P!10=w"}});
	Make();
	music->ScriptStartMusic(51);
	music->ProcessMusic();
	EXPECT_EQ(music->ScriptLastMusicLine(1.0f), std::optional<bool>(false));
	for (int i = 0; i < 20; ++i) // 2.4 s: past both chunks' markers
	{
		Pass();
	}
	EXPECT_EQ(script.musicLine, 3u);
	EXPECT_EQ(script.musicBeat, 3); // 1 at "L3", +1 at "P", +1 at "w"
	EXPECT_EQ(music->ScriptLastMusicLine(3.0f), std::optional<bool>(true));
	EXPECT_EQ(music->ScriptLastMusicLine(3.9f), std::optional<bool>(true)); // truncated
	EXPECT_EQ(music->ScriptLastMusicLine(4.0f), std::optional<bool>(false));
}

// Integration test: needs the original game data (OPENBLACK_TEST_BW_ROOT); skipped without it
TEST_F(GameMusicTest, ScriptMusicOfTheGame)
{
	const auto root = GameRoot();
	if (!root)
	{
		GTEST_SKIP() << "OPENBLACK_TEST_BW_ROOT not set";
	}
	// START_MUSIC(59): Script03.sad at its own volume 65 (docs/bw1-notes/audio.md)
	const auto script03 = FindNoCase(*root, MusicBankFor(MusicType::ScriptGeneric03).path);
	ASSERT_TRUE(script03);
	AddBank(MusicType::ScriptGeneric03, *script03);
	const auto verse = FindNoCase(*root, MusicBankFor(MusicType::ScriptMissionariesVerse1).path);
	ASSERT_TRUE(verse);
	AddBank(MusicType::ScriptMissionariesVerse1, *verse);
	Make();
	music->ScriptStartMusic(59);
	music->ProcessMusic();
	const int channel = ChannelOf(MusicType::ScriptGeneric03);
	ASSERT_NE(channel, k_NoMusicChannel);
	EXPECT_EQ(engine.GetChannel(channel).sadVolume, 65);
	EXPECT_EQ(engine.GetChannel(channel).target, 127);
	EXPECT_EQ(engine.GetChannel(channel).loops, 0);

	// the missionaries' first verse: its markers bring LAST_MUSIC_LINE to 1 and on, in order
	music->ScriptStartMusic(51);
	music->ProcessMusic();
	ASSERT_NE(ChannelOf(MusicType::ScriptMissionariesVerse1), k_NoMusicChannel);
	uint32_t previous = 0;
	bool reachedOne = false;
	for (int i = 0; i < 400; ++i) // 48 s, longer than the verse (~40 s)
	{
		Pass();
		EXPECT_GE(script.musicLine, previous);
		previous = script.musicLine;
		reachedOne = reachedOne || music->ScriptLastMusicLine(1.0f) == std::optional<bool>(true);
	}
	EXPECT_TRUE(reachedOne);
	EXPECT_GT(script.musicLine, 1u);
}

TEST_F(GameMusicTest, ScriptMusicOfTheGameSynthetic)
{
	// a script bank at its own volume 65 without the loops flag, and a verse whose two chunks carry the lines 1 and 2
	AddBank(MusicType::ScriptGeneric03, BankSpec {.segments = 4, .flags = 0x20, .volume = 65});
	AddBank(MusicType::ScriptMissionariesVerse1, BankSpec {.segments = 4, .descriptions = {"!0=L1", "!0=L2"}});
	Make();
	music->ScriptStartMusic(59);
	music->ProcessMusic();
	const int channel = ChannelOf(MusicType::ScriptGeneric03);
	ASSERT_NE(channel, k_NoMusicChannel);
	EXPECT_EQ(engine.GetChannel(channel).sadVolume, 65);
	EXPECT_EQ(engine.GetChannel(channel).target, 127);
	EXPECT_EQ(engine.GetChannel(channel).loops, 0);

	// the verse's markers bring LAST_MUSIC_LINE to 1 and then 2, never back
	music->ScriptStartMusic(51);
	music->ProcessMusic();
	ASSERT_NE(ChannelOf(MusicType::ScriptMissionariesVerse1), k_NoMusicChannel);
	uint32_t previous = 0;
	bool reachedOne = false;
	for (int i = 0; i < 40; ++i) // 4.8 s, past both chunks' markers
	{
		Pass();
		EXPECT_GE(script.musicLine, previous);
		previous = script.musicLine;
		reachedOne = reachedOne || music->ScriptLastMusicLine(1.0f) == std::optional<bool>(true);
	}
	EXPECT_TRUE(reachedOne);
	EXPECT_EQ(script.musicLine, 2u);
}

// --- The music attached to objects ---------------------------------------------------------------------------------

TEST_F(GameMusicTest, PlayDistance)
{
	AddBank(MusicType::ScriptPiperTune, BankSpec {.flags = 0x3C0, .maxDistance = 100.0f});
	AddBank(MusicType::ScriptGregorian3D, BankSpec {.flags = 0x3E0, .maxDistance = 120.0f});
	AddBank(MusicType::ScriptHermit, BankSpec {.maxDistance = -5.0f});
	AddBank(MusicType::ScriptKhazar, BankSpec {});
	Make();
	EXPECT_EQ(music->GetPlayDistance(47), 100.0f);
	EXPECT_EQ(music->GetPlayDistance(81), 120.0f);
	EXPECT_EQ(music->GetPlayDistance(49), 100.0f); // negative -> 100
	EXPECT_EQ(music->GetPlayDistance(68), 0.0f);   // 0 stays 0
	EXPECT_EQ(music->GetPlayDistance(82), 100.0f); // no bank
	EXPECT_EQ(music->GetPlayDistance(0), 100.0f);

	// GET_MUSIC_ENUM_DISTANCE: an invalid type pushes 0 and then the distance
	EXPECT_EQ(music->ScriptMusicTypeDistances(47), (std::vector<float> {100.0f}));
	EXPECT_EQ(music->ScriptMusicTypeDistances(0), (std::vector<float> {0.0f, 100.0f}));
	EXPECT_EQ(music->ScriptMusicTypeDistances(85), (std::vector<float> {0.0f, 100.0f}));

	// GET_MUSIC_OBJ_DISTANCE: 0 without an info
	things[7] = glm::vec3(0.0f);
	EXPECT_EQ(music->ThingMusicDistance(7), 0.0f);
	music->AddThingMusic(81, 7);
	EXPECT_EQ(music->ThingMusicDistance(7), 120.0f);
}

// Integration test: needs the original game data (OPENBLACK_TEST_BW_ROOT); skipped without it
TEST_F(GameMusicTest, PiperOfTheGame)
{
	const auto root = GameRoot();
	if (!root)
	{
		GTEST_SKIP() << "OPENBLACK_TEST_BW_ROOT not set";
	}
	// PiperTune_M.sad: 15 / 100 / 4 (docs/bw1-notes/audio.md): it plays only nearer than 100
	const auto piper = FindNoCase(*root, MusicBankFor(MusicType::ScriptPiperTune).path);
	ASSERT_TRUE(piper);
	AddBank(MusicType::ScriptPiperTune, *piper);
	Make();
	EXPECT_EQ(music->GetPlayDistance(47), 100.0f);
	things[3] = glm::vec3(100.0f, 0.0f, 0.0f);
	music->ScriptAttachMusic(47, 3);
	music->ProcessMusic();
	EXPECT_EQ(ChannelOf(MusicType::ScriptPiperTune), k_NoMusicChannel); // at 100: not < 100
	things[3] = glm::vec3(99.0f, 0.0f, 0.0f);
	music->ProcessMusic();
	const int channel = ChannelOf(MusicType::ScriptPiperTune);
	ASSERT_NE(channel, k_NoMusicChannel);
	EXPECT_EQ(engine.GetChannel(channel).is3D, 1);
	EXPECT_EQ(engine.GetChannel(channel).loops, -1); // flag 0x40
}

TEST_F(GameMusicTest, PiperOfTheGameSynthetic)
{
	// a bank with the maximum distance 100 and loops -1, both with their flags set
	AddBank(MusicType::ScriptPiperTune, BankSpec {.flags = 0x3C0, .loops = -1, .maxDistance = 100.0f});
	Make();
	EXPECT_EQ(music->GetPlayDistance(47), 100.0f);
	things[3] = glm::vec3(100.0f, 0.0f, 0.0f);
	music->ScriptAttachMusic(47, 3);
	music->ProcessMusic();
	EXPECT_EQ(ChannelOf(MusicType::ScriptPiperTune), k_NoMusicChannel); // at 100: not < 100
	things[3] = glm::vec3(99.0f, 0.0f, 0.0f);
	music->ProcessMusic();
	const int channel = ChannelOf(MusicType::ScriptPiperTune);
	ASSERT_NE(channel, k_NoMusicChannel);
	EXPECT_EQ(engine.GetChannel(channel).is3D, 1);
	EXPECT_EQ(engine.GetChannel(channel).loops, -1); // the bank's loops, flag 0x40
}

TEST_F(GameMusicTest, ThingMusicPlays3DEveryTurnAndBlocksTheAlignment)
{
	AddBank(MusicType::GenericNeutral, BankSpec {.segments = 20, .group = 1});
	AddBank(MusicType::ScriptPiperTune, BankSpec {.flags = 0x3C0, .loops = -1, .maxDistance = 100.0f});
	Make();
	things[11] = glm::vec3(30.0f, 5.0f, 40.0f); // 50 from the camera at 0
	music->ScriptAttachMusic(47, 11);
	ASSERT_EQ(music->GetThingMusic().GetCount(), 1u);

	music->ProcessMusic();
	const int channel = ChannelOf(MusicType::ScriptPiperTune);
	ASSERT_NE(channel, k_NoMusicChannel);
	EXPECT_EQ(engine.GetChannel(channel).is3D, 1);
	EXPECT_EQ(engine.GetChannel(channel).sync, 0); // group 0: no sync, no fade
	EXPECT_EQ(engine.GetChannel(channel).fade, 0);
	EXPECT_EQ(sink.positions[static_cast<size_t>(channel)], glm::vec3(30.0f, 5.0f, 40.0f)); // placed in 3D
	EXPECT_EQ(music->GetPlayingMessage(), "Music Playing=NONE");
	EXPECT_EQ(ChannelOf(MusicType::GenericNeutral), k_NoMusicChannel); // no alignment music
	EXPECT_EQ(music->GetThingMusic().GetInfos()[0].started, 1);

	// it moves: re-triggered, the new position
	things[11] = glm::vec3(10.0f, 0.0f, 0.0f);
	music->ProcessMusic();
	EXPECT_EQ(ChannelOf(MusicType::ScriptPiperTune), channel);
	EXPECT_EQ(sink.positions[static_cast<size_t>(channel)], glm::vec3(10.0f, 0.0f, 0.0f));

	// SET_MUSIC_PLAY_POSITION: the music sounds there, the range is still the thing's
	music->SetPlayPosition(11, glm::vec3(1.25f, 7.0f, -2.5f));
	music->ProcessMusic();
	EXPECT_EQ(sink.positions[static_cast<size_t>(channel)],
	          glm::vec3(map_coords::Quantise(1.25f), 7.0f, map_coords::Quantise(-2.5f)));

	// disabled but in range: nothing plays, and the alignment music does not come either
	music->EnableThingMusic(11, 0);
	music->ProcessMusic();
	EXPECT_EQ(ChannelOf(MusicType::GenericNeutral), k_NoMusicChannel);
	EXPECT_EQ(music->GetThingMusic().GetInfos()[0].started, 0);

	// out of range: the alignment music
	things[11] = glm::vec3(500.0f, 0.0f, 0.0f);
	music->ProcessMusic();
	EXPECT_NE(ChannelOf(MusicType::GenericNeutral), k_NoMusicChannel);

	// the thing is gone: its info goes
	things.erase(11);
	music->UpdateTurn(true);
	EXPECT_EQ(music->GetThingMusic().GetCount(), 0u);
}

TEST_F(GameMusicTest, ThingMusicList)
{
	AddBank(MusicType::ScriptPiperTune, BankSpec {.flags = 0x3C0, .maxDistance = 100.0f});
	AddBank(MusicType::ScriptPiperCaveTune, BankSpec {.flags = 0x3C0, .maxDistance = 80.0f});
	Make();
	things[1] = glm::vec3(0.0f);
	things[2] = glm::vec3(0.0f);

	music->AddThingMusic(47, 99); // not available: nothing
	EXPECT_EQ(music->GetThingMusic().GetCount(), 0u);

	music->AddThingMusic(47, 1);
	music->AddThingMusic(48, 2);
	ASSERT_EQ(music->GetThingMusic().GetCount(), 2u);
	EXPECT_EQ(music->GetThingMusic().GetInfos()[0].thing, 2u); // the head is the newest
	music->AddThingMusic(48, 1);                               // same thing: only the type changes
	EXPECT_EQ(music->GetThingMusic().GetCount(), 2u);
	EXPECT_EQ(music->GetThingMusic().GetInfos()[1].type, 48);

	EXPECT_EQ(music->IsThingMusicFinished(1), 0);
	EXPECT_EQ(music->IsThingMusicFinished(5), 1); // no info: 1

	music->MoveThingMusic(1, 5);
	EXPECT_EQ(music->GetThingMusic().GetInfos()[1].thing, 5u);
	music->RemoveThingMusic(5);
	EXPECT_EQ(music->GetThingMusic().GetCount(), 1u);

	// ATTACH_MUSIC with a type out of 1..84 reports it and attaches it anyway; without a thing, nothing
	music->ScriptAttachMusic(85, 1);
	EXPECT_EQ(music->GetThingMusic().GetCount(), 2u);
	music->ScriptAttachMusic(47, std::nullopt);
	EXPECT_EQ(music->GetThingMusic().GetCount(), 2u);

	// RESTART_MUSIC: started back to 0 and the channel cut
	music->ProcessMusic();
	const int channel = ChannelOf(MusicType::ScriptPiperCaveTune);
	ASSERT_NE(channel, k_NoMusicChannel);
	music->RestartThingMusic(2);
	EXPECT_EQ(engine.GetChannel(channel).status, MusicStatus::Free);

	// Reset: every info goes
	music->Reset();
	EXPECT_EQ(music->GetThingMusic().GetCount(), 0u);
}

TEST(ThingMusic, PlayPositionQuantised)
{
	// SetPlayPosition keeps map coordinates: trunc(v * 6553.6f) * 10 / 65536 (map_coords::Quantise), truncated
	// to the 16.16 grid of 10-unit cells
	EXPECT_EQ(map_coords::Quantise(0.0f), 0.0f);
	EXPECT_EQ(map_coords::Quantise(1.0f), static_cast<float>(6553.0 * 10.0 / 65536.0));
	EXPECT_EQ(map_coords::Quantise(-1.0f), static_cast<float>(-6553.0 * 10.0 / 65536.0));
	EXPECT_EQ(map_coords::Quantise(2560.0f), 2560.0f);
	ThingMusicList list;
	list.AddFront(static_cast<int>(MusicType::ScriptPiperTune), 7);
	list.SetPlayPosition(7, glm::vec3(1.0f, 3.0f, -1.0f));
	const auto* info = list.Get(7);
	ASSERT_NE(info, nullptr);
	EXPECT_EQ(info->playPosition, glm::vec3(map_coords::Quantise(1.0f), 3.0f, map_coords::Quantise(-1.0f)));
}

// --- The alignment and tribe music -------------------------------------------------------------------------------

TEST(AlignmentMusic, DiscreteAlignmentAndTables)
{
	// trunc(min((a + 1) / 2 * 7, 6)); evil below -3/7, good from 3/7
	EXPECT_EQ(DiscreteAlignment(-1.0f), 0);
	EXPECT_EQ(DiscreteAlignment(0.0f), 3);
	EXPECT_EQ(DiscreteAlignment(1.0f), 6);
	EXPECT_EQ(AlignmentIndex(DiscreteAlignment(-0.43f)), 0);
	EXPECT_EQ(AlignmentIndex(DiscreteAlignment(-0.42f)), 1);
	EXPECT_EQ(AlignmentIndex(DiscreteAlignment(0.42f)), 1);
	EXPECT_EQ(AlignmentIndex(DiscreteAlignment(0.43f)), 2);
	EXPECT_EQ(AlignmentIndex(7), 1);
	// the town music of each tribe and alignment
	EXPECT_EQ(TribeMusicType(0, 0), 4);  // CELTIC_TOWN_EVIL
	EXPECT_EQ(TribeMusicType(1, 1), 5);  // CELTIC_TOWN_NEUTRAL
	EXPECT_EQ(TribeMusicType(2, 2), 9);  // AZTEC_TOWN_GOOD
	EXPECT_EQ(TribeMusicType(1, 7), 23); // NORSE_TOWN_NEUTRAL (celt_neutral.sad)
	EXPECT_EQ(TribeMusicType(2, 8), 27); // TIBETAN_TOWN_GOOD
	EXPECT_EQ(TribeMusicType(0, 9), 5);  // from 9 on: CELTIC_TOWN_NEUTRAL
}

// --- The worship chants ------------------------------------------------------------------------------------------

TEST(ChantMusic, Table)
{
	// each tribe's chant; + 1 (the _vox version) above 8 dancers
	EXPECT_EQ(ChantMusicType(0, 1), static_cast<int>(MusicType::CelticChant));
	EXPECT_EQ(ChantMusicType(1, 8), static_cast<int>(MusicType::CelticChant)); // African: the Celtic chant
	EXPECT_EQ(ChantMusicType(1, 9), static_cast<int>(MusicType::CelticChantVox));
	EXPECT_EQ(ChantMusicType(2, 20), static_cast<int>(MusicType::AztecChantVox));
	EXPECT_EQ(ChantMusicType(7, 3), static_cast<int>(MusicType::NorseChant));
	EXPECT_EQ(ChantMusicType(8, 9), static_cast<int>(MusicType::TibetanChantVox));
	EXPECT_EQ(ChantMusicType(9, 20), 5); // from 9 on: CELTIC_TOWN_NEUTRAL
}

TEST_F(GameMusicTest, ChantMusicAtTheDance)
{
	// ProcessChantMusic runs after the script music and before the thing music
	AddBank(MusicType::GenericNeutral, BankSpec {.segments = 20, .group = 1});
	AddBank(MusicType::NorseChant, BankSpec {.segments = 20, .group = 2});
	AddBank(MusicType::NorseChantVox, BankSpec {.segments = 20, .group = 2});
	Make();
	camera = CameraState {glm::vec3(0.0f, 60.0f, 0.0f), 40.0f};
	chant = ChantSite {
	    .tribe = 7, .dancers = 5, .position = glm::vec3(30.0f, 25.0f, 40.0f), .ground = 20.0f, .cameraGround = 20.0f};
	music->ProcessMusic();
	const int channel = ChannelOf(MusicType::NorseChant);
	ASSERT_NE(channel, k_NoMusicChannel);
	const auto& ch = engine.GetChannel(channel);
	EXPECT_EQ(ch.target, 127);
	EXPECT_EQ(ch.sync, 1);
	EXPECT_EQ(ch.fade, 0);
	EXPECT_EQ(ch.is3D, 1);
	EXPECT_EQ(sink.positions[static_cast<size_t>(channel)], glm::vec3(30.0f, 25.0f, 40.0f)); // placed in 3D
	EXPECT_EQ(music->GetPlayingMessage(), "Music Playing=NONE");                             // ProcessMusic's message after it
	EXPECT_EQ(ChannelOf(MusicType::GenericNeutral), k_NoMusicChannel);

	// more than 8 dancers: the _vox chant alone
	chant->dancers = 9;
	music->ProcessMusic();
	EXPECT_NE(ChannelOf(MusicType::NorseChantVox), k_NoMusicChannel);

	// the camera's height against the dance's ground: 20 + 80 - 20 = 80 < 100 plays; 20 + 100 - 20 does not
	camera->heightAboveGround = 80.0f;
	music->ProcessMusic();
	EXPECT_EQ(music->GetAlignmentType(), -1);
	camera->heightAboveGround = 100.0f;
	music->ProcessMusic();
	EXPECT_EQ(music->GetAlignmentType(), 2); // the alignment music again

	// no bank for the tribe's chant: nothing
	camera->heightAboveGround = 40.0f;
	chant->tribe = 3;
	music->ProcessMusic();
	EXPECT_EQ(music->GetAlignmentType(), 2);
}

TEST_F(GameMusicTest, AlignmentMusicGenericAt80WithFade)
{
	for (int type = 1; type <= 27; ++type)
	{
		AddBank(static_cast<MusicType>(type), BankSpec {.segments = 20, .group = 1});
	}
	Make();
	ASSERT_EQ(music->GetGroupPositions().size(), 1u); // one music group
	music->ProcessMusic();
	const int channel = ChannelOf(MusicType::GenericNeutral);
	ASSERT_NE(channel, k_NoMusicChannel);
	const auto& ch = engine.GetChannel(channel);
	EXPECT_EQ(ch.target, 80);
	EXPECT_EQ(ch.fade, 1);
	EXPECT_EQ(ch.current, 0);
	EXPECT_EQ(ch.startChunk, 1); // the group's saved position, 1
	EXPECT_EQ(ch.sync, 1);       // synced to the group
	EXPECT_EQ(ch.loops, 0);      // loops = (start 1 >= 20 / 2)
	EXPECT_EQ(music->GetAlignmentType(), 2);
	EXPECT_EQ(music->GetPlayingMessage(), "Music Playing=MUSIC_TYPE_GENERIC_NEUTRAL");
	EXPECT_EQ(lastTownSearch, 400.0f); // the nearest town within townTriggerOffDistance

	// good alignment: GENERIC_GOOD; the position of the group is saved first (+2)
	for (int i = 0; i < 5; ++i)
	{
		Pass();
		sink.PlayOne(channel);
	}
	const auto playing = engine.GetChannel(channel).playingChunk;
	alignment = 0.5f;
	music->ProcessMusic();
	EXPECT_EQ(music->GetGroupPositions()[0], static_cast<int>(playing) + 2);
	const int good = ChannelOf(MusicType::GenericGood);
	ASSERT_NE(good, k_NoMusicChannel);
	EXPECT_EQ(engine.GetChannel(good).startChunk, static_cast<int>(playing) + 2);
	EXPECT_EQ(engine.GetChannel(good).sync, 1);
	EXPECT_EQ(engine.GetChannel(good).loops, static_cast<int>(playing) + 2 >= 10 ? 1 : 0);
}

TEST_F(GameMusicTest, CitadelMusicInsideTheCitadel)
{
	// ProcessCitadelMusic, first in ProcessMusic
	AddBank(MusicType::GenericNeutral, BankSpec {.segments = 20, .group = 1});
	AddBank(MusicType::CitadelEvil, BankSpec {.segments = 20, .group = 1});
	AddBank(MusicType::CitadelNeutral, BankSpec {.segments = 20, .group = 1});
	AddBank(MusicType::CitadelGood, BankSpec {.segments = 20, .group = 1});
	Make();
	music->ProcessMusic();
	const int outside = ChannelOf(MusicType::GenericNeutral);
	ASSERT_NE(outside, k_NoMusicChannel);
	EXPECT_EQ(music->GetCitadelSamplesStopped(), 0);
	for (int i = 0; i < 4; ++i)
	{
		Pass();
		sink.PlayOne(outside);
	}
	const auto playing = engine.GetChannel(outside).playingChunk;

	// inside: the samples stop once; the type is 44 + the alignment index of the local player's discrete alignment:
	// 0 -> 3 -> 1, CITADEL_NEUTRAL, volume 127, fade 1, sync 1
	insideCitadel = true;
	alignment = 1.0f; // the camera's alignment is not the one read
	music->ProcessMusic();
	const int citadel = ChannelOf(MusicType::CitadelNeutral);
	ASSERT_NE(citadel, k_NoMusicChannel);
	const auto& ch = engine.GetChannel(citadel);
	EXPECT_EQ(ch.target, 127);
	EXPECT_EQ(ch.fade, 1);
	EXPECT_EQ(ch.sync, 1);
	EXPECT_FALSE(sink.enabled3D[static_cast<size_t>(citadel)]);
	// the start is read before the group's position is saved: the old one, 1 (the sync puts the new channel on the
	// playing one's chunk anyway)
	EXPECT_EQ(ch.startChunk, 1);
	EXPECT_EQ(music->GetGroupPositions()[0], static_cast<int>(playing) + 2);
	EXPECT_EQ(music->GetCitadelSamplesStopped(), 1);
	// ProcessMusic's "Music Playing=NONE" after it, and no alignment type
	EXPECT_EQ(music->GetPlayingMessage(), "Music Playing=NONE");
	EXPECT_EQ(music->GetAlignmentType(), -1);

	// the local player's alignment picks the track: -1 -> 0 -> CITADEL_EVIL, 1 -> 6 -> CITADEL_GOOD
	playerAlignment = -1.0f;
	music->ProcessMusic();
	EXPECT_NE(ChannelOf(MusicType::CitadelEvil), k_NoMusicChannel);
	playerAlignment = 1.0f;
	music->ProcessMusic();
	EXPECT_NE(ChannelOf(MusicType::CitadelGood), k_NoMusicChannel);

	// out again: the latch goes back to 0 and the alignment music comes back
	insideCitadel = false;
	alignment = 0.0f;
	music->ProcessMusic();
	EXPECT_EQ(music->GetCitadelSamplesStopped(), 0);
	EXPECT_EQ(music->GetAlignmentType(), 2);
}

TEST_F(GameMusicTest, CitadelMusicWithoutItsBank)
{
	// no bank for the type: ProcessMusic goes on (the latch is already set)
	AddBank(MusicType::GenericNeutral, BankSpec {.segments = 20, .group = 1});
	Make();
	insideCitadel = true;
	music->ProcessMusic();
	EXPECT_EQ(music->GetCitadelSamplesStopped(), 1);
	EXPECT_NE(ChannelOf(MusicType::GenericNeutral), k_NoMusicChannel);
}

TEST_F(GameMusicTest, AlignmentMusicFromTheSecondHalfLoopsOnce)
{
	// a start at or past half of the bank's segments (20 / 2 = 10) loops once
	AddBank(MusicType::GenericNeutral, BankSpec {.segments = 20, .group = 1});
	AddBank(MusicType::GenericGood, BankSpec {.segments = 20, .group = 1});
	Make();
	music->ProcessMusic();
	const int neutral = ChannelOf(MusicType::GenericNeutral);
	ASSERT_NE(neutral, k_NoMusicChannel);
	EXPECT_EQ(engine.GetChannel(neutral).loops, 0); // start 1 < 10
	for (int i = 0; i < 100 && engine.GetChannel(neutral).playingChunk < 8; ++i)
	{
		Pass();
		sink.PlayOne(neutral);
	}
	ASSERT_EQ(engine.GetChannel(neutral).status, MusicStatus::Playing);
	alignment = 1.0f;
	music->ProcessMusic(); // the group's position = audible chunk + 2
	const int start = music->GetGroupPositions()[0];
	ASSERT_GE(start, 10);
	const int good = ChannelOf(MusicType::GenericGood);
	ASSERT_NE(good, k_NoMusicChannel);
	EXPECT_EQ(engine.GetChannel(good).startChunk, start);
	EXPECT_EQ(engine.GetChannel(good).loops, 1);
	EXPECT_EQ(engine.GetChannel(good).sync, 1);
}

TEST_F(GameMusicTest, AlignmentMusicConditions)
{
	AddBank(MusicType::GenericNeutral, BankSpec {.segments = 20, .group = 1});
	Make();
	turn = 20; // needs > 20
	music->ProcessMusic();
	EXPECT_EQ(ChannelOf(MusicType::GenericNeutral), k_NoMusicChannel);
	turn = 21;
	script.alignmentMusic = 0; // ENABLE_DISABLE_ALIGNMENT_MUSIC
	music->ProcessMusic();
	EXPECT_EQ(ChannelOf(MusicType::GenericNeutral), k_NoMusicChannel);
	script.alignmentMusic = 1;
	wideScreen = true;
	music->ProcessMusic();
	EXPECT_EQ(ChannelOf(MusicType::GenericNeutral), k_NoMusicChannel);
	wideScreen = false;
	camera.reset();
	music->ProcessMusic();
	EXPECT_EQ(ChannelOf(MusicType::GenericNeutral), k_NoMusicChannel);
	camera = CameraState {};
	music->ProcessMusic();
	EXPECT_NE(ChannelOf(MusicType::GenericNeutral), k_NoMusicChannel);
}

TEST_F(GameMusicTest, TribeMusicNearTowns)
{
	for (int type = 1; type <= 27; ++type)
	{
		AddBank(static_cast<MusicType>(type), BankSpec {.segments = 20, .group = 1});
	}
	Make();
	// an Aztec town (tribe 2) at 250 with the camera low: AZTEC_TOWN_NEUTRAL
	towns[1] = MusicTown {1, 2, 250.0f};
	nearest = towns[1];
	camera->heightAboveGround = 100.0f;
	music->ProcessMusic();
	EXPECT_EQ(music->GetAlignmentType(), 8);
	EXPECT_EQ(music->GetCurrentTown(), std::optional<uint32_t>(1));

	// the camera high: generic, and the town forgotten
	camera->heightAboveGround = 400.0f; // needs < 400
	music->ProcessMusic();
	EXPECT_EQ(music->GetAlignmentType(), 2);
	EXPECT_EQ(music->GetCurrentTown(), std::nullopt);

	// the same town between 300 and 400: generic (the kept town is the nearest one)
	camera->heightAboveGround = 100.0f;
	music->ProcessMusic();
	towns[1].distance = 350.0f;
	nearest = towns[1];
	music->ProcessMusic();
	EXPECT_EQ(music->GetAlignmentType(), 2);
	EXPECT_EQ(music->GetCurrentTown(), std::nullopt);

	// another town nearer but beyond 300 while the kept one is within 400: the kept one's music (Japanese, 3)
	towns[1] = MusicTown {1, 3, 280.0f};
	nearest = towns[1];
	music->ProcessMusic();
	EXPECT_EQ(music->GetAlignmentType(), 11);
	towns[1].distance = 390.0f;
	towns[2] = MusicTown {2, 6, 320.0f};
	nearest = towns[2];
	music->ProcessMusic();
	EXPECT_EQ(music->GetAlignmentType(), 11);
	EXPECT_EQ(music->GetCurrentTown(), std::optional<uint32_t>(1));
	// the kept one at 400: not < 400, generic
	towns[1].distance = 400.0f;
	music->ProcessMusic();
	EXPECT_EQ(music->GetAlignmentType(), 2);
	// a kept town that is no longer available is forgotten
	towns[2].distance = 300.0f;
	nearest = towns[2];
	music->ProcessMusic();
	EXPECT_EQ(music->GetAlignmentType(), 20); // Greek (6), neutral
	towns.erase(2);
	nearest.reset();
	music->ProcessMusic();
	EXPECT_EQ(music->GetCurrentTown(), std::nullopt);
	EXPECT_EQ(music->GetAlignmentType(), 2);
}

TEST_F(GameMusicTest, SilenceAfterTheAlignmentTrackEnds)
{
	AddBank(MusicType::GenericNeutral, BankSpec {.segments = 2, .group = 1});
	Make();
	music->ProcessMusic();
	const int channel = ChannelOf(MusicType::GenericNeutral);
	ASSERT_NE(channel, k_NoMusicChannel);
	PlayToTheEnd(channel);
	EXPECT_EQ(music->GetFinishedType(), 2);
	EXPECT_EQ(music->GetAlignmentType(), -1);
	EXPECT_EQ(music->GetGroupPositions()[0], 1);

	// 3500 turns: the turns 1..3499 play nothing, the 3500th plays again
	for (uint32_t i = 1; i < 0xDAC; ++i)
	{
		music->ProcessMusic();
		ASSERT_EQ(ChannelOf(MusicType::GenericNeutral), k_NoMusicChannel) << "turn " << i;
	}
	EXPECT_EQ(music->GetSilenceTurns(), 0xDABu);
	music->ProcessMusic();
	EXPECT_NE(ChannelOf(MusicType::GenericNeutral), k_NoMusicChannel);
	EXPECT_EQ(music->GetFinishedType(), 0);
}

TEST_F(GameMusicTest, SilenceEndsWhenTheTypeChanges)
{
	AddBank(MusicType::GenericNeutral, BankSpec {.segments = 2, .group = 1});
	AddBank(MusicType::GenericGood, BankSpec {.segments = 2, .group = 1});
	Make();
	music->ProcessMusic();
	PlayToTheEnd(ChannelOf(MusicType::GenericNeutral));
	music->ProcessMusic();
	EXPECT_EQ(ChannelOf(MusicType::GenericNeutral), k_NoMusicChannel);
	alignment = 1.0f;
	music->ProcessMusic();
	EXPECT_NE(ChannelOf(MusicType::GenericGood), k_NoMusicChannel);
}

TEST_F(GameMusicTest, ScriptMusicBeforeAlignment)
{
	AddBank(MusicType::GenericNeutral, BankSpec {.segments = 20, .group = 1});
	AddBank(MusicType::ScriptKhazar, BankSpec {.flags = 0x60, .loops = -1, .volume = 65});
	Make();
	music->ProcessMusic();
	const int align = ChannelOf(MusicType::GenericNeutral);
	ASSERT_NE(align, k_NoMusicChannel);
	music->ScriptStartMusic(68);
	music->ProcessMusic();
	EXPECT_NE(ChannelOf(MusicType::ScriptKhazar), k_NoMusicChannel);
	EXPECT_EQ(music->GetAlignmentType(), -1);
	// the script music stops: the alignment music comes back at once
	music->ScriptStopMusic();
	music->ProcessMusic();
	EXPECT_EQ(music->GetAlignmentType(), 2);
}

TEST_F(GameMusicTest, ResetValues)
{
	AddBank(MusicType::GenericNeutral, BankSpec {.segments = 20, .group = 1});
	AddBank(MusicType::ScriptEpic01);
	Make();
	towns[1] = MusicTown {1, 0, 10.0f};
	nearest = towns[1];
	music->ProcessMusic();
	music->ScriptStartMusic(61);
	music->ProcessMusic();
	music->Reset();
	EXPECT_EQ(music->GetScriptType(), 0);
	EXPECT_EQ(music->GetScriptStarted(), 0);
	EXPECT_EQ(music->GetAlignmentType(), -1);
	EXPECT_EQ(music->GetFinishedType(), 0);
	EXPECT_EQ(music->GetCurrentTown(), std::nullopt);
	EXPECT_EQ(music->GetGroupPositions()[0], 1);
	for (int i = 0; i < k_MusicChannelCount; ++i)
	{
		EXPECT_EQ(engine.GetChannel(i).status, MusicStatus::Free); // stopped without a fade: cut
	}
}

// Integration test: needs the original game data (OPENBLACK_TEST_BW_ROOT); skipped without it
TEST(AlignmentMusic, TownTriggerOfTheGame)
{
	const auto root = GameRoot();
	if (!root)
	{
		GTEST_SKIP() << "OPENBLACK_TEST_BW_ROOT not set";
	}
	const auto info = FindNoCase(*root, "Scripts/info.dat");
	ASSERT_TRUE(info);
	// InfoFile::LoadFromFile without the Locator: the "Info" block of the pack holds the info constants as they are
	std::ifstream stream(*info, std::ios::binary);
	pack::PackFile file;
	ASSERT_EQ(file.ReadFile(stream), pack::PackResult::Success);
	const auto& block = file.GetBlock("Info");
	ASSERT_EQ(block.size(), sizeof(InfoConstants));
	auto constants = std::make_unique<InfoConstants>();
	std::memcpy(constants.get(), block.data(), sizeof(InfoConstants));
	// the sound info's town trigger distances (docs/bw1-notes/audio.md)
	EXPECT_EQ(constants->sound.townTriggerDistance, 300.0f);
	EXPECT_EQ(constants->sound.townTriggerOffDistance, 400.0f);
}
