/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <array>
#include <chrono>
#include <memory>
#include <optional>
#include <vector>

#include <Audio/MusicTimingFile.h>
#include <Audio/ScriptMusic.h>
#include <Audio/SoundDecoder.h>
#include <ECS/SyncedState.h>
#include <PackFile.h>
#include <gtest/gtest.h>

using namespace openblack::audio;
using namespace std::chrono_literals;

namespace
{
constexpr auto k_Verse = MusicType::ScriptMissionariesVerse1;
constexpr auto k_Epic = MusicType::ScriptEpic03;
constexpr uint32_t k_Rate = 22050;

/// A piece of two chunks at 22050 frames a second: a whole chunk and 1000 frames, 1142 ms in all
std::shared_ptr<MusicTiming> MakeTiming(std::vector<MusicCue> cues = {}, std::optional<int32_t> loops = std::nullopt)
{
	auto timing = std::make_shared<MusicTiming>();
	timing->cues = std::move(cues);
	timing->chunkFrames = {k_CueChunkFrames, 1000};
	timing->sampleRate = k_Rate;
	timing->loopOverride = loops;
	return timing;
}

/// Gives the same times to every piece, or none
ScriptMusic::TimingFor Always(std::shared_ptr<MusicTiming> timing)
{
	return [timing = std::move(timing)](MusicType) { return timing; };
}

const std::vector<MusicCue> k_Cues {
    {.chunk = 1, .frame = 0, .text = "L1"},
    {.chunk = 1, .frame = 3000, .text = "p"},
    {.chunk = 2, .frame = 100, .text = "L2"},
};

/// Plays one turn of 100 ms, then more until the channel's clock reaches the time or no channel plays
void TurnsUntil(ScriptMusic& music, const ScriptMusic::TimingFor& timing, std::chrono::milliseconds time)
{
	auto gone = std::chrono::microseconds(0);
	music.Turn(100ms, timing);
	while (music.GetChannelTime().has_value() && music.GetChannelTime().value() < time && gone < time + 10s)
	{
		music.Turn(100ms, timing);
		gone += 100ms;
	}
}
} // namespace

TEST(ScriptMusicClock, ThePieceStartsAtTheMusicTurnAndItsCuesComeOnTheTicksOfTheTurnClock)
{
	ScriptMusic music;
	const auto timing = Always(MakeTiming(k_Cues, -1));
	music.Start(k_Verse);
	music.Turn(100ms, timing);
	EXPECT_TRUE(music.IsStarted());
	EXPECT_EQ(music.GetChannelTime(), 0us);
	EXPECT_EQ(music.GetPlayCount(), 1U);
	// No look of the music thread has come yet
	music.Turn(100ms, timing);
	EXPECT_EQ(music.GetLyrics().line, 0);
	// The look at 120 ms passes the first line: 2646 frames
	music.Turn(100ms, timing);
	EXPECT_EQ(music.GetLyrics().line, 1);
	EXPECT_EQ(music.GetLyrics().word, 1);
	// The look at 240 ms is past frame 3000: the next word
	music.Turn(100ms, timing);
	EXPECT_EQ(music.GetLyrics().word, 2);
	EXPECT_EQ(music.GetPlace(), (MusicPiecePosition {.chunk = 1, .frame = 6615}));
	// The second chunk's line, at frame 24292, comes at the look at 1200 ms
	TurnsUntil(music, timing, 1100ms);
	EXPECT_EQ(music.GetLyrics().line, 1);
	music.Turn(100ms, timing);
	EXPECT_EQ(music.GetLyrics().line, 2);
	EXPECT_EQ(music.GetLyrics().word, 1);
}

TEST(ScriptMusicClock, TheTurnsTimeTheCuesWhateverTheirLength)
{
	// Turns at twice the game's speed take half the real time, and the cue comes at the same time of the clock
	ScriptMusic music;
	const auto timing = Always(MakeTiming(k_Cues));
	music.Start(k_Verse);
	music.Turn(50ms, timing);
	music.Turn(50ms, timing);
	music.Turn(50ms, timing);
	EXPECT_EQ(music.GetLyrics().line, 0);
	// The fourth turn reaches 150 ms, past the look at 120 ms
	music.Turn(50ms, timing);
	EXPECT_EQ(music.GetLyrics().line, 1);
}

TEST(ScriptMusicClock, ThePieceEndsAtTheFirstLookAfterItsLastChunkHasPlayed)
{
	ScriptMusic music;
	const auto timing = Always(MakeTiming(k_Cues));
	music.Start(k_Epic);
	TurnsUntil(music, timing, 1100ms);
	EXPECT_FALSE(music.IsOver(k_Epic));
	// 25192 frames last 1142 ms: the look at 1200 ms finds the end
	music.Turn(100ms, timing);
	EXPECT_TRUE(music.IsOver(k_Epic));
	EXPECT_EQ(music.GetType(), MusicType::None);
	EXPECT_FALSE(music.GetChannelTime().has_value());
	// The look that finds the end checks no cues: the line in the last 120 ms does not come
	EXPECT_EQ(music.GetLyrics().line, 1);
	music.Turn(100ms, timing);
	EXPECT_FALSE(music.IsStarted());
}

TEST(ScriptMusicClock, ABankThatLoopsForEverNeverEndsAndItsCuesComeOnce)
{
	ScriptMusic music;
	const auto timing = Always(MakeTiming(k_Cues, -1));
	music.Start(k_Verse);
	TurnsUntil(music, timing, 5000ms);
	EXPECT_FALSE(music.IsOver(k_Verse));
	EXPECT_EQ(music.GetLyrics().line, 2);
	EXPECT_EQ(music.GetLyrics().word, 1);
	// The place of the music heard goes round the chunks again
	EXPECT_LT(music.GetPlace()->chunk, 3U);
}

TEST(ScriptMusicClock, ABankThatLoopsAFewTimesEndsAfterItsLastPass)
{
	ScriptMusic music;
	const auto timing = Always(MakeTiming({}, 1));
	music.Start(k_Epic);
	TurnsUntil(music, timing, 2200ms);
	EXPECT_FALSE(music.IsOver(k_Epic));
	TurnsUntil(music, timing, 2400ms);
	EXPECT_TRUE(music.IsOver(k_Epic));
}

TEST(ScriptMusicClock, AStoppedPieceFadesOutAndItsCuesContinueUntilItStops)
{
	ScriptMusic music;
	const auto timing = Always(MakeTiming(k_Cues, -1));
	music.Start(k_Verse);
	TurnsUntil(music, timing, 200ms);
	ASSERT_EQ(music.GetLyrics().line, 1);
	music.Start(MusicType::None);
	music.Turn(100ms, timing);
	EXPECT_FALSE(music.IsStarted());
	ASSERT_TRUE(music.GetChannelTime().has_value());
	// The fade still passes the next line
	TurnsUntil(music, timing, 1200ms);
	EXPECT_EQ(music.GetLyrics().line, 2);
	// 43 looks of the fade, then the channel stops
	TurnsUntil(music, timing, 300ms + ScriptMusic::k_Tick * ScriptMusic::k_FadeTicks);
	EXPECT_FALSE(music.GetChannelTime().has_value());
}

TEST(ScriptMusicClock, StartingThePieceAgainWhileItsChannelPlaysContinuesItsClock)
{
	ScriptMusic music;
	const auto timing = Always(MakeTiming(k_Cues, -1));
	music.Start(k_Verse);
	TurnsUntil(music, timing, 300ms);
	music.Start(MusicType::None);
	music.Turn(100ms, timing);
	music.ResetLyrics();
	music.Start(k_Verse);
	music.Turn(100ms, timing);
	EXPECT_TRUE(music.IsStarted());
	EXPECT_EQ(music.GetPlayCount(), 2U);
	EXPECT_EQ(music.GetChannelTime(), 500ms);
	// The cues that passed do not come again
	music.Turn(100ms, timing);
	EXPECT_EQ(music.GetLyrics().line, 0);
	// Nor does the fade stop it
	TurnsUntil(music, timing, 300ms + ScriptMusic::k_Tick * ScriptMusic::k_FadeTicks);
	EXPECT_TRUE(music.GetChannelTime().has_value());
}

TEST(ScriptMusicClock, AnotherPieceStartsOnItsOwnClock)
{
	ScriptMusic music;
	const auto timing = Always(MakeTiming(k_Cues, -1));
	music.Start(k_Verse);
	TurnsUntil(music, timing, 300ms);
	music.Start(k_Epic);
	music.Turn(100ms, timing);
	EXPECT_EQ(music.GetType(), k_Epic);
	EXPECT_EQ(music.GetChannelTime(), 0us);
}

TEST(ScriptMusicClock, APieceWithoutABankNeverStartsAndIsNeverOver)
{
	ScriptMusic music;
	const auto none = [](MusicType) { return std::shared_ptr<const MusicTiming> {}; };
	music.Start(k_Epic);
	for (int i = 0; i < 100; ++i)
	{
		music.Turn(100ms, none);
	}
	EXPECT_FALSE(music.IsStarted());
	EXPECT_FALSE(music.IsOver(k_Epic));
	// A bank that does not read has no chunks, and counts as no bank
	music.Turn(100ms, Always(std::make_shared<MusicTiming>()));
	EXPECT_FALSE(music.IsStarted());
}

TEST(ScriptMusicClock, APieceWithoutABankLetsTheEarlierPieceFadeWithoutItsCues)
{
	ScriptMusic music;
	const auto timing = MakeTiming(k_Cues, -1);
	const auto onlyTheVerse = [timing](MusicType type) {
		return type == k_Verse ? timing : std::shared_ptr<const MusicTiming> {};
	};
	music.Start(k_Verse);
	TurnsUntil(music, onlyTheVerse, 200ms);
	music.Start(k_Epic);
	TurnsUntil(music, onlyTheVerse, 1200ms);
	EXPECT_EQ(music.GetLyrics().line, 1);
}

TEST(ScriptMusicClock, APieceThatPlayedBeforeALoadGivesNoSoundAndNeverEnds)
{
	ScriptMusic music;
	const auto timing = Always(MakeTiming());
	music.Restore(k_Epic, true, {.line = 3, .word = 2});
	TurnsUntil(music, timing, 0ms);
	for (int i = 0; i < 100; ++i)
	{
		music.Turn(100ms, timing);
	}
	EXPECT_FALSE(music.IsOver(k_Epic));
	EXPECT_FALSE(music.GetPlace().has_value());
	EXPECT_EQ(music.GetLyrics().line, 3);
	EXPECT_EQ(music.GetLyrics().word, 2);
	// A script that starts it again plays it from its start
	music.Start(k_Epic);
	music.Turn(100ms, timing);
	EXPECT_EQ(music.GetPlace(), (MusicPiecePosition {.chunk = 1, .frame = 0}));
}

TEST(ScriptMusicClock, AChannelThatComesBackFromALoadGivesTheSameCuesAndEnd)
{
	// A piece that plays without a stop, and the same piece saved and loaded at 500 ms
	ScriptMusic whole;
	const auto timing = Always(MakeTiming(k_Cues));
	whole.Start(k_Verse);
	TurnsUntil(whole, timing, 500ms);
	ASSERT_TRUE(whole.GetChannelState().has_value());
	ScriptMusic loaded;
	loaded.Restore(whole.GetType(), whole.IsStarted(), whole.GetLyrics());
	loaded.RestoreChannel(*whole.GetChannelState(), timing);
	for (int i = 0; i < 12; ++i)
	{
		whole.Turn(100ms, timing);
		loaded.Turn(100ms, timing);
		EXPECT_EQ(loaded.SyncedValues(), whole.SyncedValues());
	}
	EXPECT_EQ(loaded.GetLyrics().line, whole.GetLyrics().line);
	EXPECT_TRUE(loaded.IsOver(k_Verse));
}

TEST(ScriptMusicClock, AChannelWhoseBankCannotPlayDoesNotComeBack)
{
	ScriptMusic music;
	music.Restore(k_Verse, true, {.line = 1, .word = 1});
	music.RestoreChannel({.type = k_Verse, .time = 500ms, .ticks = 4}, [](MusicType) { return nullptr; });
	EXPECT_FALSE(music.GetChannelState().has_value());
	EXPECT_FALSE(music.IsOver(k_Verse));
}

TEST(ScriptMusicClock, ThePlaceInTheChunksCountsTheRealChunkLengths)
{
	const std::vector<uint32_t> chunks {100, 50};
	EXPECT_EQ(PiecePositionAfter(chunks, 0), (MusicPiecePosition {.chunk = 1, .frame = 0}));
	EXPECT_EQ(PiecePositionAfter(chunks, 99), (MusicPiecePosition {.chunk = 1, .frame = 99}));
	EXPECT_EQ(PiecePositionAfter(chunks, 100), (MusicPiecePosition {.chunk = 2, .frame = 0}));
	EXPECT_EQ(PiecePositionAfter(chunks, 160), (MusicPiecePosition {.chunk = 1, .frame = 10}));
	EXPECT_EQ(PiecePositionAfter({}, 160), (MusicPiecePosition {}));
}

TEST(ScriptMusicClock, TheWorldHasTheMusicWithASoundDeviceInANetworkGameAndOnTheServer)
{
	EXPECT_TRUE(WorldHasMusic(true, false, false));
	EXPECT_FALSE(WorldHasMusic(false, false, false));
	EXPECT_TRUE(WorldHasMusic(false, true, false));
	EXPECT_TRUE(WorldHasMusic(false, false, true));
}

namespace
{
/// Raw MPEG-2 layer III audio: frames of 104 bytes at 32 kbit/s, 22050 Hz, one channel, with no sound in them
std::vector<uint8_t> SilentMpeg(size_t frames)
{
	constexpr size_t k_FrameBytes = 104;
	std::vector<uint8_t> data(frames * k_FrameBytes, 0);
	for (size_t i = 0; i < frames; ++i)
	{
		data[i * k_FrameBytes + 0] = 0xFF;
		data[i * k_FrameBytes + 1] = 0xF3;
		data[i * k_FrameBytes + 2] = 0x40;
		data[i * k_FrameBytes + 3] = 0xC0;
	}
	return data;
}
} // namespace

TEST(ScriptMusicClock, TheTimesOfABankComeFromItsHeadersAndTheFramesOfItsChunks)
{
	EXPECT_EQ(CountMpegFrames(SilentMpeg(10)), 10U * 576U);
	EXPECT_FALSE(CountMpegFrames({}).has_value());

	std::vector<openblack::pack::AudioBankSampleHeader> headers(2);
	headers[0].sampleRate = k_Rate;
	headers[0].overrideFlags = static_cast<uint32_t>(openblack::pack::AudioBankOverride::Loop);
	headers[0].loop = -1;
	headers[0].description = {};
	headers[1].description = {};
	const std::string text = "!2303=p!18815=L2";
	std::copy(text.begin(), text.end(), headers[1].description.begin());
	const std::vector<std::vector<uint8_t>> chunks {SilentMpeg(42), SilentMpeg(14)};
	const auto timing = MakeMusicTiming(headers, chunks);
	ASSERT_TRUE(timing.has_value());
	EXPECT_EQ(timing->sampleRate, k_Rate);
	EXPECT_EQ(timing->loopOverride, -1);
	EXPECT_EQ(timing->chunkFrames, (std::vector<uint32_t> {k_CueChunkFrames, 8064}));
	ASSERT_EQ(timing->cues.size(), 2U);
	EXPECT_EQ(timing->cues[0].text, "L2");
	EXPECT_EQ(timing->cues[0].chunk, 2U);
	// A bank whose chunks do not match its headers gives no times
	EXPECT_FALSE(MakeMusicTiming(headers, std::span(chunks).first(1)).has_value());
}

TEST(ScriptMusicClock, TheSyncedChecksumTakesInTheValuesOfTheScriptsMusic)
{
	const std::array<int64_t, 2> lineOne {1, 1};
	const std::array<int64_t, 2> lineTwo {2, 1};
	EXPECT_EQ(openblack::synced_state::MixValues(7, lineOne), openblack::synced_state::MixValues(7, lineOne));
	EXPECT_NE(openblack::synced_state::MixValues(7, lineOne), openblack::synced_state::MixValues(7, lineTwo));
	EXPECT_EQ(openblack::synced_state::MixValues(7, {}), 7U);
}
