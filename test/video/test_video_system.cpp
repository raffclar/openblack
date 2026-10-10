/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The video system with fake hooks and a fake clock, over small synthetic Bink files

#include <chrono>
#include <map>
#include <memory>
#include <vector>

#include <gtest/gtest.h>

#define LOCATOR_IMPLEMENTATIONS
#include "BinkTestFiles.h"
#include "ECS/Systems/Implementations/VideoSystem.h"

using namespace openblack;
using namespace openblack::ecs::systems;
using namespace std::chrono_literals;
using Clock = std::chrono::steady_clock;

namespace
{
/// The game around the video system
struct FakeGame
{
	bool paused {false};
	bool wideScreen {false};
	int snaps {0};
	int musicReleased {0};
	bool hasCreature {true};
	bool whiteReached {false};
	std::vector<video::FallingSpellCue> cues;
	std::vector<std::filesystem::path> closed;
	std::map<std::filesystem::path, std::shared_ptr<const bink::BinkFile>> files;

	VideoSystem::Hooks Hooks()
	{
		return {
		    .isPaused = [this]() { return paused; },
		    .setPaused = [this](bool value) { paused = value; },
		    .isWideScreenOn = [this]() { return wideScreen; },
		    .setWideScreen = [this](bool on) { wideScreen = on; },
		    .snapWideScreen = [this]() { ++snaps; },
		    .releaseScriptMusic = [this]() { ++musicReleased; },
		    .openFile = [this](const std::filesystem::path& path) -> std::shared_ptr<const bink::BinkFile> {
			    const auto it = files.find(path);
			    return it != files.end() ? it->second : nullptr;
		    },
		    .closeFile = [this](const std::filesystem::path& path) { closed.push_back(path); },
		    .playerHasCreature = [this]() { return hasCreature; },
		    .fallingSpellPath = []() { return std::filesystem::path("fall.bik"); },
		    .fallingSpellCue = [this](video::FallingSpellCue cue) { cues.push_back(cue); },
		    .whiteFadeReached = [this]() { return whiteReached; },
		};
	}
};

/// A 16x16 video of `frames` frames at `fps`
std::shared_ptr<const bink::BinkFile> MakeVideo(uint32_t frames, uint32_t fps)
{
	std::vector<std::vector<uint8_t>> packets;
	for (uint32_t i = 0; i < frames; ++i)
	{
		packets.push_back(bink::test::FillFrame(static_cast<uint8_t>(16 + i % 200), 128, 128));
	}
	auto file = bink::BinkFile::Parse(bink::test::MakeBik(packets, {.fpsNumerator = fps}));
	EXPECT_TRUE(file.has_value());
	return std::make_shared<const bink::BinkFile>(std::move(*file));
}

/// Updates at `fps` frames a second from `start` for `frames` frames
Clock::time_point Play(VideoSystem& videos, Clock::time_point start, int frames, int fps)
{
	auto now = start;
	for (int i = 0; i < frames; ++i)
	{
		now = start + std::chrono::microseconds((1'000'000LL * i + fps - 1) / fps);
		videos.Update(now);
	}
	return now;
}

const Clock::time_point k_Start {};
} // namespace

TEST(VideoSystem, PlayPausesAndBringsTheBarsInAtOnce)
{
	FakeGame game;
	game.files["a.bik"] = MakeVideo(300, 24);
	VideoSystem videos(game.Hooks(), video::DecodeMode::Inline);
	EXPECT_TRUE(videos.Play("a.bik"));
	EXPECT_TRUE(videos.IsPlaying());
	EXPECT_TRUE(game.paused);
	EXPECT_TRUE(game.wideScreen);
	EXPECT_EQ(game.snaps, 1);
	EXPECT_TRUE(videos.CoversScreen());
	EXPECT_FALSE(videos.HidesWorld());
	// Before its first picture it draws black
	auto picture = videos.GetPicture();
	ASSERT_TRUE(picture.has_value());
	EXPECT_TRUE(picture->y.empty());
	EXPECT_EQ(picture->alpha, 0xFF);
	videos.Update(k_Start);
	picture = videos.GetPicture();
	ASSERT_TRUE(picture.has_value());
	EXPECT_FALSE(picture->y.empty());
	EXPECT_EQ(picture->y[0], 16);
	EXPECT_EQ(picture->serial, 1u);
	EXPECT_EQ(picture->width, 16u);
}

TEST(VideoSystem, TheBarsSnapEvenWhenAlreadyOn)
{
	FakeGame game;
	game.wideScreen = true;
	game.files["a.bik"] = MakeVideo(10, 24);
	VideoSystem videos(game.Hooks(), video::DecodeMode::Inline);
	videos.Play("a.bik");
	EXPECT_EQ(game.snaps, 1);
	// They were on: they stay on afterwards
	Play(videos, k_Start, 40, 24);
	EXPECT_FALSE(videos.IsPlaying());
	EXPECT_TRUE(game.wideScreen);
	EXPECT_FALSE(game.paused);
}

TEST(VideoSystem, AFileThatDoesntOpenEndsAtOnce)
{
	FakeGame game;
	VideoSystem videos(game.Hooks(), video::DecodeMode::Inline);
	EXPECT_FALSE(videos.Play("missing.bik"));
	EXPECT_TRUE(videos.IsPlaying());
	EXPECT_TRUE(game.paused);
	videos.Update(k_Start);
	EXPECT_FALSE(videos.IsPlaying());
	EXPECT_FALSE(videos.GetPicture().has_value());
	// The pause and the bars go back on the update after
	EXPECT_TRUE(game.paused);
	EXPECT_TRUE(game.wideScreen);
	videos.Update(k_Start + 16ms);
	EXPECT_FALSE(game.paused);
	EXPECT_FALSE(game.wideScreen);
	EXPECT_TRUE(game.closed.empty());
}

TEST(VideoSystem, FramesFollowTheRealClockAndNoneIsSkipped)
{
	FakeGame game;
	game.files["a.bik"] = MakeVideo(300, 24);
	VideoSystem videos(game.Hooks(), video::DecodeMode::Inline);
	videos.Play("a.bik");
	videos.Update(k_Start);
	EXPECT_EQ(videos.GetStatus()->frame, 1);
	videos.Update(k_Start + 41ms);
	EXPECT_EQ(videos.GetStatus()->frame, 1);
	videos.Update(k_Start + 42ms);
	EXPECT_EQ(videos.GetStatus()->frame, 2);
	// A long stall: every frame due is decoded, the last one shown
	videos.Update(k_Start + 2s);
	EXPECT_EQ(videos.GetStatus()->frame, 49);
	EXPECT_EQ(videos.GetPicture()->serial, 49u);
	EXPECT_EQ(videos.GetPicture()->y[0], 16 + 48);
}

TEST(VideoSystem, FadesOverItsLastFiveSecondsAndGivesThePauseBack)
{
	FakeGame game;
	game.files["a.bik"] = MakeVideo(240, 24); // 10 s: the fade from frame 120
	VideoSystem videos(game.Hooks(), video::DecodeMode::Inline);
	videos.Play("a.bik");
	EXPECT_EQ(videos.GetStatus()->schedule, (video::Schedule {.fadeStart = 120, .end = 240}));
	auto now = Play(videos, k_Start, 120, 24);
	EXPECT_EQ(videos.GetStatus()->frame, 120);
	EXPECT_TRUE(game.paused);
	EXPECT_TRUE(videos.CoversScreen());
	videos.Update(now + 42ms);
	// Fading: the game runs again under it, the world is drawn
	EXPECT_FALSE(game.paused);
	EXPECT_FALSE(videos.CoversScreen());
	EXPECT_EQ(videos.GetStatus()->alpha, 1.0f - 1.0f / 121.0f);
	EXPECT_EQ(videos.GetPicture()->alpha, 252);
	now = Play(videos, now, 200, 24);
	EXPECT_FALSE(videos.IsPlaying());
	EXPECT_EQ(game.closed, std::vector<std::filesystem::path> {"a.bik"});
	EXPECT_FALSE(game.wideScreen);
}

TEST(VideoSystem, TheIntroEndsAtSixtySeconds)
{
	FakeGame game;
	game.files["intro.bik"] = MakeVideo(1601, 24);
	VideoSystem videos(game.Hooks(), video::DecodeMode::Inline);
	videos.Play("intro.bik");
	videos.ScheduleIntro();
	EXPECT_EQ(videos.GetStatus()->schedule, (video::Schedule {.fadeStart = 1392, .end = 1440}));
	Play(videos, k_Start, 1439, 24);
	ASSERT_TRUE(videos.IsPlaying());
	EXPECT_EQ(videos.GetStatus()->frame, 1439);
	videos.Update(k_Start + 60s);
	EXPECT_FALSE(videos.IsPlaying());
}

TEST(VideoSystem, AMissingIntroWaitsForEscape)
{
	FakeGame game;
	VideoSystem videos(game.Hooks(), video::DecodeMode::Inline);
	EXPECT_FALSE(videos.Play("intro.bik"));
	videos.ScheduleIntro();
	// Timed at one frame a second, but with no file no frame is counted: it stays, paused, however long
	EXPECT_EQ(videos.GetStatus()->schedule, (video::Schedule {.fadeStart = 58, .end = 60}));
	videos.Update(k_Start);
	videos.Update(k_Start + 120s);
	EXPECT_TRUE(videos.IsPlaying());
	EXPECT_TRUE(videos.CoversScreen());
	EXPECT_EQ(videos.GetStatus()->frame, 0);
	EXPECT_TRUE(game.paused);
	// Escape: the fade would end at the file's last frame, which is none, so it ends at once
	EXPECT_TRUE(videos.Escape(false, false));
	videos.Update(k_Start + 121s);
	EXPECT_FALSE(videos.IsPlaying());
	videos.Update(k_Start + 122s);
	EXPECT_FALSE(game.paused);
}

TEST(VideoSystem, WithFilmsOffTheIntroEndsAtOnce)
{
	FakeGame game;
	game.files["intro.bik"] = MakeVideo(1601, 24);
	VideoSystem videos(game.Hooks(), video::DecodeMode::Inline);
	videos.SetFilmsEnabled(false);
	videos.Play("intro.bik");
	videos.ScheduleIntro();
	videos.Update(k_Start);
	EXPECT_FALSE(videos.IsPlaying());
}

TEST(VideoSystem, EscapeFadesOutOverFortyEightFrames)
{
	FakeGame game;
	game.files["a.bik"] = MakeVideo(1000, 24);
	VideoSystem videos(game.Hooks(), video::DecodeMode::Inline);
	EXPECT_FALSE(videos.Escape(false, false)); // nothing to skip
	videos.Play("a.bik");
	auto now = Play(videos, k_Start, 100, 24);
	// With Shift or Ctrl held it takes the key and does nothing
	EXPECT_TRUE(videos.Escape(true, false));
	EXPECT_TRUE(videos.Escape(false, true));
	EXPECT_EQ(videos.GetStatus()->schedule, video::DefaultSchedule(1000, 24));
	EXPECT_EQ(game.musicReleased, 0);
	// While it can't be skipped, likewise
	videos.SetNoSkip(true);
	EXPECT_TRUE(videos.Escape(false, false));
	EXPECT_EQ(game.musicReleased, 0);
	videos.SetNoSkip(false);

	EXPECT_TRUE(videos.Escape(false, false));
	EXPECT_EQ(game.musicReleased, 1);
	EXPECT_EQ(videos.GetStatus()->schedule, (video::Schedule {.fadeStart = 100, .end = 148}));
	EXPECT_FALSE(game.paused);
	// A second Escape while fading ends it at once
	videos.Update(now + 42ms);
	EXPECT_TRUE(videos.Escape(false, false));
	EXPECT_FALSE(videos.IsPlaying());
}

TEST(VideoSystem, EndingClearsTheNoSkip)
{
	FakeGame game;
	game.files["a.bik"] = MakeVideo(4, 24);
	VideoSystem videos(game.Hooks(), video::DecodeMode::Inline);
	videos.Play("a.bik");
	videos.SetNoSkip(true);
	Play(videos, k_Start, 10, 24);
	game.files["b.bik"] = MakeVideo(100, 24);
	videos.Play("b.bik");
	videos.Update(k_Start + 1s);
	EXPECT_TRUE(videos.Escape(false, false));
	EXPECT_EQ(game.musicReleased, 1);
}

TEST(VideoSystem, AReplacedVideoLeavesTheGamePaused)
{
	FakeGame game;
	game.files["a.bik"] = MakeVideo(1000, 24);
	game.files["b.bik"] = MakeVideo(10, 24);
	VideoSystem videos(game.Hooks(), video::DecodeMode::Inline);
	videos.Play("a.bik");
	videos.Update(k_Start);
	// The second keeps the first's pause and bars as what to go back to
	videos.Play("b.bik");
	EXPECT_EQ(game.closed, std::vector<std::filesystem::path> {"a.bik"});
	Play(videos, k_Start + 1s, 40, 24);
	EXPECT_FALSE(videos.IsPlaying());
	EXPECT_TRUE(game.paused);
	EXPECT_TRUE(game.wideScreen);
}

TEST(VideoSystem, FilmsTurnedOffPlayAsMissing)
{
	FakeGame game;
	game.files["a.bik"] = MakeVideo(100, 24);
	VideoSystem videos(game.Hooks(), video::DecodeMode::Inline);
	videos.SetFilmsEnabled(false);
	EXPECT_FALSE(videos.Play("a.bik"));
	videos.Update(k_Start);
	EXPECT_FALSE(videos.IsPlaying());
}

TEST(VideoSystem, TheFallingSpell)
{
	FakeGame game;
	game.files["fall.bik"] = MakeVideo(1200, 24);
	VideoSystem videos(game.Hooks(), video::DecodeMode::Inline);
	game.hasCreature = false;
	videos.StartFallingSpell();
	EXPECT_FALSE(videos.IsPlaying());

	game.hasCreature = true;
	videos.StartFallingSpell();
	ASSERT_TRUE(videos.IsPlaying());
	EXPECT_TRUE(videos.HidesWorld());
	EXPECT_FALSE(videos.CoversScreen());
	EXPECT_TRUE(game.paused);
	// No fade of its own
	EXPECT_EQ(videos.GetStatus()->schedule, (video::Schedule {.fadeStart = 1200, .end = 1200}));
	auto now = Play(videos, k_Start, 324, 24);
	EXPECT_EQ(videos.GetPicture()->alpha, 0x50);
	EXPECT_EQ(game.cues, (std::vector {video::FallingSpellCue::CitadelExplode, video::FallingSpellCue::Volcano}));
	EXPECT_EQ(videos.GetStatus()->fallingSpellState, 1);

	// At 43.9 s the white fade starts; once it is white the spell ends and the film fades as Escape fades it
	now = Play(videos, now, 740, 24);
	EXPECT_EQ(videos.GetStatus()->fallingSpellState, 3);
	game.whiteReached = true;
	videos.Update(now + 42ms);
	EXPECT_FALSE(videos.HidesWorld());
	ASSERT_TRUE(videos.IsPlaying());
	const auto status = videos.GetStatus();
	EXPECT_EQ(status->schedule.end - status->schedule.fadeStart, 48);
	EXPECT_EQ(videos.GetPicture()->alpha, 0xFF);
	EXPECT_FALSE(game.paused);
}

TEST(VideoSystem, EscapeEndsTheFallingSpell)
{
	FakeGame game;
	game.files["fall.bik"] = MakeVideo(1200, 24);
	VideoSystem videos(game.Hooks(), video::DecodeMode::Inline);
	videos.StartFallingSpell();
	Play(videos, k_Start, 100, 24);
	EXPECT_TRUE(videos.Escape(false, false));
	EXPECT_FALSE(videos.HidesWorld());
	EXPECT_TRUE(videos.IsPlaying());
	EXPECT_EQ(videos.GetStatus()->schedule, (video::Schedule {.fadeStart = 100, .end = 148}));
}
