/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The full-screen videos' rules: schedules, fade, draw strength, the letterbox, pacing and the falling spell's timeline

#include <chrono>
#include <vector>

#include <gtest/gtest.h>

#include "Video/FallingSpellAudio.h"
#include "Video/PreviewPlayback.h"
#include "Video/VideoRules.h"

using namespace openblack::video;
using namespace std::chrono_literals;

TEST(VideoRules, Schedules)
{
	// INTRO.bik: 1601 frames at 24 a second
	EXPECT_EQ(DefaultSchedule(1601, 24), (Schedule {.fadeStart = 1481, .end = 1601}));
	EXPECT_EQ(IntroSchedule(24), (Schedule {.fadeStart = 1392, .end = 1440}));
	EXPECT_EQ(WithoutFade(DefaultSchedule(1200, 24)), (Schedule {.fadeStart = 1200, .end = 1200}));
	// A file that didn't open: 1 frame a second and none at all
	EXPECT_EQ(DefaultSchedule(0, 1), (Schedule {.fadeStart = -5, .end = 0}));
	EXPECT_EQ(PhaseAt(0, DefaultSchedule(0, 1)), Phase::Ended);
}

TEST(VideoRules, Phases)
{
	const Schedule intro = IntroSchedule(24);
	EXPECT_EQ(PhaseAt(0, intro), Phase::Playing);
	EXPECT_EQ(PhaseAt(1392, intro), Phase::Playing);
	EXPECT_EQ(PhaseAt(1393, intro), Phase::Fading);
	EXPECT_EQ(PhaseAt(1439, intro), Phase::Fading);
	EXPECT_EQ(PhaseAt(1440, intro), Phase::Ended);
}

TEST(VideoRules, AlphaFallsLinearlyAcrossTheFade)
{
	const Schedule intro = IntroSchedule(24);
	EXPECT_EQ(VideoAlpha(1392, intro), 1.0f);
	EXPECT_EQ(VideoAlpha(1393, intro), 1.0f - 1.0f / 49.0f);
	EXPECT_EQ(VideoAlpha(1416, intro), 1.0f - 24.0f / 49.0f);
	// The last frame drawn is still a little visible: alpha 0 is never drawn
	EXPECT_EQ(VideoAlpha(1439, intro), 1.0f - 47.0f / 49.0f);
	EXPECT_EQ(VideoAlpha(1440, intro), 0.0f);
	float previous = 1.0f;
	for (int32_t frame = 1393; frame < 1440; ++frame)
	{
		const float alpha = VideoAlpha(frame, intro);
		EXPECT_LT(alpha, previous);
		previous = alpha;
	}
}

TEST(VideoRules, DrawAlphaRoundsDown)
{
	EXPECT_EQ(DrawAlpha(1.0f, false), 0xFF);
	EXPECT_EQ(DrawAlpha(1.0f, true), 0x50);
	EXPECT_EQ(DrawAlpha(0.5f, false), 127);
	EXPECT_EQ(DrawAlpha(0.999f, false), 254);
	EXPECT_EQ(DrawAlpha(0.0f, true), 0);
}

TEST(VideoRules, Skip)
{
	const Schedule intro = IntroSchedule(24);
	// Fades over 48 frames from where it is
	EXPECT_EQ(SkipSchedule(100, intro, 1601), (Schedule {.fadeStart = 100, .end = 148}));
	// Never past the file's last frame
	EXPECT_EQ(SkipSchedule(1580, DefaultSchedule(1601, 24), 1601), std::nullopt);
	EXPECT_EQ(SkipSchedule(1470, DefaultSchedule(1601, 24), 1601), (Schedule {.fadeStart = 1470, .end = 1518}));
	EXPECT_EQ(SkipSchedule(1199, WithoutFade(DefaultSchedule(1200, 24)), 1200), (Schedule {.fadeStart = 1199, .end = 1200}));
	// Already fading: it ends at once
	EXPECT_EQ(SkipSchedule(1393, intro, 1601), std::nullopt);
	EXPECT_EQ(SkipSchedule(1392, intro, 1601), (Schedule {.fadeStart = 1392, .end = 1440}));
}

TEST(VideoRules, Letterbox)
{
	// 4:3: bars of 60 pixels at 640 x 480
	EXPECT_EQ(LetterboxRect(640, 480), (ScreenRect {.x = 0, .y = 60, .width = 641, .height = 361}));
	EXPECT_EQ(LetterboxRect(1024, 768), (ScreenRect {.x = 0, .y = 96, .width = 1025, .height = 577}));
	// 16:9 exactly: no bars
	EXPECT_EQ(LetterboxRect(1920, 1080), (ScreenRect {.x = 0, .y = 0, .width = 1921, .height = 1081}));
	// An odd loss rounds towards zero
	EXPECT_EQ(LetterboxRect(1280, 1023), (ScreenRect {.x = 0, .y = 151, .width = 1281, .height = 722}));
	// Wider than 16:9: the bars are negative and the picture runs over the screen
	EXPECT_EQ(LetterboxRect(2560, 1080), (ScreenRect {.x = 0, .y = -180, .width = 2561, .height = 1441}));
	EXPECT_EQ(LetterboxRect(2561, 1080), (ScreenRect {.x = 0, .y = -180, .width = 2562, .height = 1441}));
}

TEST(VideoRules, FramesDueByTheRealClock)
{
	EXPECT_EQ(FramesDue(0us, 24, 1), 1u);
	EXPECT_EQ(FramesDue(41'666us, 24, 1), 1u);
	EXPECT_EQ(FramesDue(41'667us, 24, 1), 2u);
	EXPECT_EQ(FramesDue(1s, 24, 1), 25u);
	EXPECT_EQ(FramesDue(60s, 24, 1), 1441u);
	// The exact fraction, not the whole frames a second: 29.97
	EXPECT_EQ(FramesDue(100s, 30000, 1001), 2998u);
	EXPECT_EQ(FramesDue(-1ms, 24, 1), 1u);
}

TEST(VideoRules, FilmMilliseconds)
{
	EXPECT_EQ(FilmMilliseconds(0, 24), 0);
	EXPECT_EQ(FilmMilliseconds(24, 24), 1000);
	EXPECT_EQ(FilmMilliseconds(419, 24), 17458);
	EXPECT_EQ(FilmMilliseconds(1199, 24), 49958);
	// frame * 1000 wraps in 32 bits
	EXPECT_EQ(FilmMilliseconds(2'147'484, 1), -2'147'483'296);
	EXPECT_EQ(FilmMilliseconds(10, 0), 0);
}

TEST(VideoRules, FallingSpellTimeline)
{
	using enum FallingSpellCue;
	FallingSpellTimeline timeline;
	std::vector<FallingSpellCue> cues;
	timeline.Advance(13450, false, cues);
	EXPECT_TRUE(cues.empty());
	timeline.Advance(13451, false, cues);
	EXPECT_EQ(cues, (std::vector {CitadelExplode, Volcano}));
	EXPECT_EQ(timeline.State(), 1);
	cues.clear();
	// One frame can pass several sound steps
	timeline.Advance(32000, false, cues);
	EXPECT_EQ(cues, (std::vector {Rumble, LaserExplode, VolcanoStop, Creed}));
	EXPECT_EQ(timeline.SoundState(), 3);
	cues.clear();
	timeline.Advance(37751, false, cues);
	EXPECT_EQ(cues, (std::vector {HealChakra, CreedHigh}));
	cues.clear();
	timeline.Advance(43901, true, cues);
	EXPECT_EQ(cues, (std::vector {WhiteFadeStart, CreedStop, CreedHighStop, MusicFadeOut, CitadelExplodeEnd}));
	EXPECT_EQ(timeline.State(), 3);
	cues.clear();
	// It waits for the white fade to reach white
	timeline.Advance(44000, false, cues);
	EXPECT_TRUE(cues.empty());
	EXPECT_FALSE(timeline.HasEnded());
	timeline.Advance(44900, true, cues);
	EXPECT_EQ(cues, (std::vector {WhiteFadeBack}));
	EXPECT_TRUE(timeline.HasEnded());

	FallingSpellTimeline withoutFilm;
	for (int i = 0; i < 4; ++i)
	{
		withoutFilm.AdvanceWithoutFilm();
	}
	EXPECT_TRUE(withoutFilm.HasEnded());
}

TEST(VideoRules, FallingSpellSounds)
{
	using Action = FallingSpellSound::Action;
	EXPECT_EQ(SoundOf(FallingSpellCue::Rumble), (FallingSpellSound {.bank = "Scriptsfx.sad", .sample = 151}));
	EXPECT_EQ(SoundOf(FallingSpellCue::CreedHigh),
	          (FallingSpellSound {.bank = "InGame.sad", .sample = 166, .owner = 2, .pitchPercent = 133}));
	EXPECT_EQ(SoundOf(FallingSpellCue::VolcanoStop),
	          (FallingSpellSound {.action = Action::Stop, .bank = "InGame.sad", .sample = 172, .owner = 1}));
	EXPECT_EQ(SoundOf(FallingSpellCue::MusicFadeOut), std::nullopt);
	EXPECT_EQ(SoundOf(FallingSpellCue::WhiteFadeStart), std::nullopt);
}

TEST(PreviewPlayback, PlaysPausesStepsAndRestarts)
{
	PreviewPlayback playback(48, 24, 1);
	EXPECT_EQ(playback.Advance(1s), 0u); // paused at first
	playback.Play();
	EXPECT_EQ(playback.Advance(41ms), 0u);
	EXPECT_EQ(playback.Advance(1ms), 1u);
	EXPECT_EQ(playback.Advance(958ms), 24u);
	playback.Pause();
	EXPECT_EQ(playback.Advance(10s), 24u);
	playback.Step();
	EXPECT_EQ(playback.Frame(), 25u);
	EXPECT_FALSE(playback.IsPlaying());
	// Playing on from a step keeps the video's pace from that frame
	playback.Play();
	EXPECT_EQ(playback.Advance(42ms), 26u);
	// It stops on its last frame, and playing again starts over
	EXPECT_EQ(playback.Advance(10s), 47u);
	EXPECT_FALSE(playback.IsPlaying());
	playback.Play();
	EXPECT_EQ(playback.Frame(), 0u);
	playback.Restart();
	EXPECT_EQ(playback.Advance(0ms), 0u);
}
