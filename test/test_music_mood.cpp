/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <algorithm>
#include <optional>
#include <string>
#include <vector>

#include <MindFile.h>
#include <gtest/gtest.h>

#include "Audio/MusicMood.h"

using namespace openblack;
using namespace openblack::music_mood;

namespace
{
/// A player that plays a fixed track, or none, or that cannot work
Listen Plays(std::optional<uint32_t> track)
{
	return [track]() { return track; };
}

/// Turns until the controller looks at the music, and gives what it told the creature
std::optional<Packet> Look(Controller& controller, const Listen& listen, std::optional<Emotion> feelings, int64_t now = 100)
{
	return controller.Turn(k_TurnSeconds, true, listen, feelings, now);
}
} // namespace

TEST(MusicMood, TrackIdHashesTheSongNameWithSignedCharacters)
{
	EXPECT_EQ(TrackId("ab"), 0x356du);
	EXPECT_EQ(TrackId("\xe9"), 0xfffffe79u);
	EXPECT_EQ(TrackId("Tone A"), 0x2a75d97u);
	// The empty name still gives a number that is not 0
	EXPECT_EQ(TrackId(""), 1u);
}

TEST(MusicMood, TrackIdReadsOnlyTheFirst1023Characters)
{
	const std::string longName(1100, 'x');
	EXPECT_EQ(TrackId(longName), TrackId(longName.substr(0, 1023)));
}

TEST(MusicMood, FeelingsAreLimitedBeforeTheyAreAdded)
{
	const auto added = AddFeelings({.mood = 0.5f, .fear = -0.25f}, {.mood = 3.0f, .fear = -2.0f});
	EXPECT_FLOAT_EQ(added.mood, 1.5f);
	EXPECT_FLOAT_EQ(added.fear, -1.25f);
}

TEST(MusicMood, ButtonBlendsGreenForLikedAndRedForDisliked)
{
	EXPECT_FALSE(ButtonTint(std::nullopt).has_value());
	const auto liked = ButtonTint(Emotion {.mood = 0.5f});
	ASSERT_TRUE(liked.has_value());
	EXPECT_EQ(liked->colour, k_GoodMoodColour);
	EXPECT_EQ(liked->amount, 127u);
	const auto disliked = ButtonTint(Emotion {.mood = -2.0f});
	ASSERT_TRUE(disliked.has_value());
	EXPECT_EQ(disliked->colour, k_BadMoodColour);
	EXPECT_EQ(disliked->amount, 255u);
	EXPECT_EQ(ButtonTint(Emotion {})->colour, k_GoodMoodColour);
}

TEST(MusicMood, ANewTrackMakesARecordAndTellsTheCreatureMoodZero)
{
	Controller controller;
	const auto packet = Look(controller, Plays(77u), Emotion {.mood = 0.6f});
	ASSERT_TRUE(packet.has_value());
	EXPECT_EQ(packet->id, 77u);
	EXPECT_EQ(packet->index, 1u);
	EXPECT_EQ(packet->count, 1u);
	EXPECT_FLOAT_EQ(packet->emotion.mood, 0.0f);
	ASSERT_EQ(controller.GetRecords().size(), 1u);
	EXPECT_EQ(controller.GetRecords().front().time, 100);
	EXPECT_FLOAT_EQ(controller.GetCountdown(), k_PlayingSeconds);
}

TEST(MusicMood, TheSameTrackPlayingOnTellsNothing)
{
	Controller controller;
	ASSERT_TRUE(Look(controller, Plays(77u), std::nullopt).has_value());
	EXPECT_FALSE(Look(controller, Plays(77u), std::nullopt).has_value());
}

TEST(MusicMood, TheControllerLooksOnlyWhenItsCountdownRunsOut)
{
	Controller controller;
	int asked = 0;
	const Listen listen = [&asked]() -> std::optional<uint32_t> {
		++asked;
		return 5u;
	};
	// The first turn looks at once
	static_cast<void>(controller.Turn(k_TurnSeconds, false, listen, std::nullopt, 0));
	EXPECT_EQ(asked, 1);
	// Then five seconds, about 50 turns, pass before it looks again
	for (int turn = 0; turn < 49; ++turn)
	{
		static_cast<void>(controller.Turn(k_TurnSeconds, false, listen, std::nullopt, 0));
	}
	EXPECT_EQ(asked, 1);
	static_cast<void>(controller.Turn(k_TurnSeconds, false, listen, std::nullopt, 0));
	static_cast<void>(controller.Turn(k_TurnSeconds, false, listen, std::nullopt, 0));
	EXPECT_EQ(asked, 2);
}

TEST(MusicMood, ATrackChangeGivesTheOldRecordTheFeelingsNow)
{
	Controller controller;
	ASSERT_TRUE(Look(controller, Plays(1u), Emotion {}).has_value());
	const auto second = Look(controller, Plays(2u), Emotion {.mood = 0.75f, .fear = 0.25f});
	ASSERT_TRUE(second.has_value());
	EXPECT_EQ(second->id, 2u);
	// The newest record is first
	const auto& records = controller.GetRecords();
	ASSERT_EQ(records.size(), 2u);
	EXPECT_EQ(records.front().id, 2u);
	EXPECT_EQ(records.back().id, 1u);
	EXPECT_FLOAT_EQ(records.back().emotions.at(0).mood, 0.75f);
	EXPECT_FLOAT_EQ(records.back().emotions.at(0).fear, 0.25f);
	// The first track again: the creature is told the mood it felt
	const auto again = Look(controller, Plays(1u), Emotion {});
	ASSERT_TRUE(again.has_value());
	EXPECT_FLOAT_EQ(again->emotion.mood, 0.75f);
}

TEST(MusicMood, WithoutACreatureNoRecordGetsFeelings)
{
	Controller controller;
	ASSERT_TRUE(Look(controller, Plays(1u), std::nullopt).has_value());
	ASSERT_TRUE(Look(controller, Plays(2u), std::nullopt).has_value());
	EXPECT_FLOAT_EQ(controller.GetRecords().back().emotions.at(0).mood, 0.0f);
}

TEST(MusicMood, SilenceEndsTheCurrentTrack)
{
	Controller controller;
	ASSERT_TRUE(Look(controller, Plays(9u), Emotion {}).has_value());
	const auto silence = Look(controller, Plays(0u), Emotion {.mood = -0.5f});
	ASSERT_TRUE(silence.has_value());
	EXPECT_EQ(silence->id, 0u);
	EXPECT_EQ(silence->count, 0u);
	EXPECT_FALSE(controller.Current().has_value());
	EXPECT_FLOAT_EQ(controller.GetCountdown(), k_SilentSeconds);
	EXPECT_FLOAT_EQ(controller.GetRecords().front().emotions.at(0).mood, -0.5f);
	// Silence again tells nothing
	EXPECT_FALSE(Look(controller, Plays(0u), Emotion {}).has_value());
}

TEST(MusicMood, NoPlayerEndsTheCurrentTrackForAnHour)
{
	Controller controller;
	ASSERT_TRUE(Look(controller, Plays(9u), Emotion {}).has_value());
	const auto off = Look(controller, Plays(std::nullopt), Emotion {});
	ASSERT_TRUE(off.has_value());
	EXPECT_EQ(off->id, 0u);
	EXPECT_FLOAT_EQ(controller.GetCountdown(), k_InactiveSeconds);
}

TEST(MusicMood, PausingAndPlayingAgainTellsTheCreatureAgain)
{
	Controller controller;
	ASSERT_TRUE(Look(controller, Plays(4u), Emotion {}).has_value());
	ASSERT_TRUE(Look(controller, Plays(0u), Emotion {.mood = 0.25f}).has_value());
	const auto resumed = Look(controller, Plays(4u), Emotion {});
	ASSERT_TRUE(resumed.has_value());
	EXPECT_FLOAT_EQ(resumed->emotion.mood, 0.25f);
}

TEST(MusicMood, The257thTrackReusesTheOldestRecord)
{
	Controller controller;
	for (uint32_t track = 1; track <= k_MostRecords; ++track)
	{
		// Track 3 started first of all
		const int64_t now = track == 3 ? 1 : 1000 + track;
		ASSERT_TRUE(Look(controller, Plays(track), Emotion {.mood = 0.5f}, now).has_value());
	}
	ASSERT_EQ(controller.GetRecords().size(), k_MostRecords);
	const auto packet = Look(controller, Plays(999u), Emotion {}, 5000);
	ASSERT_TRUE(packet.has_value());
	// The reused record starts with no feelings
	EXPECT_FLOAT_EQ(packet->emotion.mood, 0.0f);
	EXPECT_EQ(controller.GetRecords().size(), k_MostRecords);
	EXPECT_EQ(std::ranges::count(controller.GetRecords(), 3u, &Record::id), 0);
	EXPECT_EQ(std::ranges::count(controller.GetRecords(), 999u, &Record::id), 1);
}

TEST(MusicMood, WritingAddsTheFeelingsNowToTheCurrentRecord)
{
	Controller controller;
	ASSERT_TRUE(Look(controller, Plays(4u), Emotion {}).has_value());
	auto written = controller.WriteRecords(Emotion {.mood = 0.5f});
	ASSERT_EQ(written.size(), 1u);
	EXPECT_FLOAT_EQ(written.front().emotions.at(0).mood, 0.5f);
	// Each write adds them again
	written = controller.WriteRecords(Emotion {.mood = 0.5f});
	EXPECT_FLOAT_EQ(written.front().emotions.at(0).mood, 1.0f);
}

TEST(MusicMood, OnlyTheOwnCreatureKeepsTheRecordsItReads)
{
	const std::vector<Record> file {
	    {.id = 10, .time = 5, .emotions = {{.mood = 0.5f}}},
	    {.id = 20, .time = 6, .emotions = {{.mood = -0.5f}}},
	};
	Controller other;
	ASSERT_TRUE(Look(other, Plays(4u), Emotion {}).has_value());
	const auto told = other.ReadRecords(file, false, Emotion {});
	EXPECT_EQ(told.id, 4u);
	EXPECT_EQ(other.GetRecords().size(), 1u);

	Controller own;
	ASSERT_TRUE(Look(own, Plays(4u), Emotion {}).has_value());
	const auto ownTold = own.ReadRecords(file, true, Emotion {});
	EXPECT_EQ(ownTold.id, 0u);
	// The records come back in the reverse order of the file
	ASSERT_EQ(own.GetRecords().size(), 2u);
	EXPECT_EQ(own.GetRecords().front().id, 20u);
	EXPECT_FLOAT_EQ(own.GetCountdown(), 0.0f);
}

TEST(MusicMood, MindFileKeepsOnlyFeelingsThatAreNotZero)
{
	const std::vector<Record> records {
	    {.id = 7, .time = 42, .emotions = {{.mood = 0.0f, .fear = 0.0f}, {.mood = 0.5f, .fear = 0.0f}}},
	};
	const auto entries = ToMindFile(records);
	ASSERT_EQ(entries.size(), 1u);
	EXPECT_EQ(entries.front().id, 7u);
	EXPECT_EQ(entries.front().time, 42u);
	EXPECT_EQ(entries.front().count, 2);
	ASSERT_EQ(entries.front().pairs.size(), 1u);
	EXPECT_EQ(entries.front().pairs.front().number, 2);
	const auto back = FromMindFile(entries);
	ASSERT_EQ(back.size(), 1u);
	ASSERT_EQ(back.front().emotions.size(), 2u);
	EXPECT_FLOAT_EQ(back.front().emotions.at(1).mood, 0.5f);
}

TEST(MusicMood, MindFileFeelingsOutsideTheirRecordAreLeftOut)
{
	creaturemind::MindDatabaseEntry entry {.id = 3, .time = 1, .count = 1};
	entry.pairs.push_back({.number = 5, .mood = 1.0f, .fear = 0.0f});
	const auto records = FromMindFile(std::vector {entry});
	ASSERT_EQ(records.size(), 1u);
	ASSERT_EQ(records.front().emotions.size(), 1u);
	EXPECT_FLOAT_EQ(records.front().emotions.front().mood, 0.0f);
}
