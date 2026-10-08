/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// video::FallingSpellVideo (src/Video/FallingSpellVideo.h): the falling spell's film over a video::VideoPlayer with
// fake hooks and a fake wall clock, no GPU: the kick-off, the end, the per-frame update, the engine frame's falling
// spell case and the temple fade. The film is a small synthetic .bik in a temporary folder.

#include <cstdint>

#include <atomic>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

#ifdef _WIN32
#include <process.h>
#else
#include <unistd.h>
#endif

#include <gtest/gtest.h>

// the tests build their own file system
#define LOCATOR_IMPLEMENTATIONS
#include "FileSystem/DefaultFileSystem.h"
#include "Locator.h"
#include "Video/BikFile.h"
#include "Video/FallingSpellVideo.h"
#include "Video/VideoPlayer.h"
#include "support/RestoreService.h"

using namespace openblack;
using namespace openblack::video;

namespace
{
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

void PutU32(std::vector<uint8_t>& data, size_t at, uint32_t v)
{
	for (size_t i = 0; i < 4; ++i)
	{
		data[at + i] = static_cast<uint8_t>(v >> (8 * i));
	}
}

/// A Bink 1 file of `frames` two-byte frames, `fps` / 1 (as test_video_player's)
std::vector<uint8_t> MakeBik(uint32_t frames, uint32_t fps)
{
	const size_t table = BikFile::k_HeaderSize;
	std::vector<uint8_t> data(table + 4 * (static_cast<size_t>(frames) + 1), 0);
	data[0] = 'B';
	data[1] = 'I';
	data[2] = 'K';
	data[3] = 'i';
	for (uint32_t i = 0; i < frames; ++i)
	{
		PutU32(data, table + 4 * i, static_cast<uint32_t>(data.size()) | (i == 0 ? 1u : 0u));
		data.push_back(static_cast<uint8_t>(i));
		data.push_back(0);
	}
	PutU32(data, table + 4 * static_cast<size_t>(frames), static_cast<uint32_t>(data.size()));
	PutU32(data, 4, static_cast<uint32_t>(data.size() - 8));
	PutU32(data, 8, frames);
	PutU32(data, 12, 2);
	PutU32(data, 16, frames);
	PutU32(data, 20, 4); // width
	PutU32(data, 24, 2); // height
	PutU32(data, 28, fps);
	PutU32(data, 32, 1);
	return data;
}

using Kind = FallingSpellSound::Kind;
using Bank = FallingSpellSound::Bank;
using Cue = FallingSpellSound::Cue;

class FallingSpellVideoTest: public ::testing::Test
{
protected:
	// first, so that it goes last: it puts back the file system from before the test once everything else has gone
	const test::RestoreService<Locator::filesystem> _restoreFilesystem;

	void SetUp() override
	{
		// the films are opened through the file system: one over the test's folder, restored when the test ends
		Locator::filesystem::emplace<filesystem::DefaultFileSystem>();
		static std::atomic<int> s_count {0};
		_folder = std::filesystem::temp_directory_path() /
		          ("openblack_test_fall_" + std::to_string(::testing::UnitTest::GetInstance()->random_seed()) + "_" +
		           std::to_string(s_count++) + "_" + UniqueSuffix());
		std::filesystem::create_directories(_folder);
		_film = Write("fall.bik", MakeBik(1200, 24)); // fall.bik: 640x360, 24 fps, 1200 frames

		VideoPlayer::Hooks playerHooks;
		playerHooks.isPaused = [this]() { return paused; };
		playerHooks.pauseGame = [this](bool p) { paused = p; };
		playerHooks.wideScreen = [this]() { return wideScreen; };
		playerHooks.setWideScreen = [this](int32_t on) { wideScreen = on; };
		playerHooks.endFallingSpellVideo = [this]() { _falling->End(); };
		_player = std::make_unique<VideoPlayer>(playerHooks);

		FallingSpellVideo::Hooks hooks;
		hooks.hasCreature = [this]() { return hasCreature; };
		hooks.filmPath = [this]() { return _film; };
		hooks.sound = [this](const FallingSpellSound& sound) { sounds.push_back(sound); };
		hooks.musicStop = [this](int32_t fade) { musicStops.push_back(fade); };
		hooks.setScreenFadeColour = [this](uint32_t argb) { fadeColours.push_back(argb); };
		_falling = std::make_unique<FallingSpellVideo>(*_player, hooks);
	}
	void TearDown() override
	{
		_falling.reset();
		_player.reset();
		std::error_code ec;
		std::filesystem::remove_all(_folder, ec);
	}

	std::filesystem::path Write(const std::string& name, const std::vector<uint8_t>& data)
	{
		const auto path = _folder / name;
		std::ofstream(path, std::ios::binary)
		    .write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
		return path;
	}

	/// One engine frame: the film's service, then the falling spell case and the temple fade
	void Frame(uint32_t ms)
	{
		_player->Process(ms);
		_falling->ProcessFrame(ms);
	}
	/// Frames of `ms` until the film has decoded `frames` frames or the falling spell is over
	void RunTo(int32_t frames, uint32_t ms = 1)
	{
		for (int guard = 0; guard < 2000000 && _falling->IsActive() && _player->CurrentFrame() < frames; ++guard)
		{
			Frame(ms);
		}
	}

	bool hasCreature {true};
	bool paused {false};
	int32_t wideScreen {0};
	std::vector<FallingSpellSound> sounds;
	std::vector<int32_t> musicStops;
	std::vector<uint32_t> fadeColours;
	std::filesystem::path _folder;
	std::filesystem::path _film;
	std::unique_ptr<VideoPlayer> _player;
	std::unique_ptr<FallingSpellVideo> _falling;
};
} // namespace

TEST(FallingSpellFilmMs, Formula)
{
	// frame * 1000 / fps, integer
	EXPECT_EQ(FallingSpellFilmMs(0, 24), 0);
	EXPECT_EQ(FallingSpellFilmMs(322, 24), 13416);
	EXPECT_EQ(FallingSpellFilmMs(323, 24), 13458);
	EXPECT_EQ(FallingSpellFilmMs(1053, 24), 43875);
	EXPECT_EQ(FallingSpellFilmMs(1054, 24), 43916);
	EXPECT_EQ(FallingSpellFilmMs(1200, 24), 50000);
}

TEST(TempleFade, Runs)
{
	TempleFade fade;
	EXPECT_FALSE(fade.Runs(k_SequenceModeNone));   // target == current
	EXPECT_TRUE(fade.Runs(k_SequenceModeCitadel)); // the citadel always runs it
	fade.target = 1.0f;
	EXPECT_TRUE(fade.Runs(k_SequenceModeNone));
	EXPECT_TRUE(fade.Runs(k_SequenceModeFallingSpell));
	EXPECT_FALSE(fade.Runs(3)); // neither
}

TEST(TempleFade, UpAndDown)
{
	TempleFade fade {.target = 1.0f, .current = 0.0f, .rgb = 0xFFFFFF, .done = 0};
	EXPECT_EQ(fade.Update(500), 0x7FFFFFFFu); // 500 * 0.001f rounds to 0.5f; 0.5 * 255 truncated = 127
	EXPECT_EQ(fade.done, 0);
	// 0.5 + 0.6: past the target, + 1, and the target becomes 0
	EXPECT_EQ(fade.Update(600), 0xFFFFFFFFu);
	EXPECT_EQ(fade.done, 1);
	EXPECT_EQ(fade.current, 1.0f);
	EXPECT_EQ(fade.target, 0.0f);
	// now it goes down by itself
	EXPECT_EQ(fade.Update(250), 0xBFFFFFFFu); // 0.75 * 255 = 191
	fade.Update(250);
	fade.Update(250);
	EXPECT_EQ(fade.done, 1);
	// below 0: the target, + 1, and with a target <= 0 the colour goes to 0
	EXPECT_EQ(fade.Update(300), 0x00000000u);
	EXPECT_EQ(fade.done, 2);
	EXPECT_EQ(fade.current, 0.0f);
	EXPECT_EQ(fade.rgb, 0u);
	EXPECT_FALSE(fade.Runs(k_SequenceModeNone));
}

TEST(TempleFade, LandingOnTheTarget)
{
	// in single precision 0.5f + 0.5f is exactly 1, not past it: no + 1, the target stays, and the engine frame stops
	// running the fade, so it is held at white (a double sum, 0.5000000237 x 2, would have gone past)
	TempleFade fade {.target = 1.0f, .current = 0.0f, .rgb = 0xFFFFFF, .done = 0};
	fade.Update(500);
	EXPECT_EQ(fade.Update(500), 0xFFFFFFFFu);
	EXPECT_EQ(fade.current, 1.0f);
	EXPECT_EQ(fade.target, 1.0f);
	EXPECT_EQ(fade.done, 0);
	EXPECT_FALSE(fade.Runs(k_SequenceModeFallingSpell));
	// the citadel runs it anyway: current >= target goes down, below the target -> back to it, + 1
	EXPECT_EQ(fade.Update(40), 0xFFFFFFFFu);
	EXPECT_EQ(fade.done, 1);
	EXPECT_EQ(fade.current, 1.0f);
}

TEST(TempleFade, AboveOne)
{
	TempleFade fade {.target = 0.0f, .current = 2.0f, .rgb = 0x123456, .done = 0};
	EXPECT_EQ(fade.Update(100), 0xFF123456u); // 1.9 > 1: 0xFF
}

TEST_F(FallingSpellVideoTest, NoCreatureNoFilm)
{
	hasCreature = false;
	_falling->KickOff();
	EXPECT_FALSE(_falling->IsActive());
	EXPECT_EQ(_falling->Mode(), k_SequenceModeNone);
	EXPECT_FALSE(_player->IsPlaying());
	EXPECT_FALSE(paused);
}

TEST_F(FallingSpellVideoTest, KickOff)
{
	_falling->KickOff();
	EXPECT_TRUE(_falling->IsActive());
	EXPECT_EQ(_falling->Mode(), k_SequenceModeFallingSpell);
	EXPECT_TRUE(_falling->HidesWorld());
	EXPECT_EQ(_falling->State(), 0);
	EXPECT_EQ(_falling->LastMs(), 100);
	ASSERT_TRUE(_player->IsPlaying());
	EXPECT_TRUE(paused); // the full-screen film pauses the game
	EXPECT_EQ(wideScreen, 1);
	EXPECT_FALSE(_player->CoversScreen()); // not opaque: the falling spell video
	EXPECT_EQ(_player->GetFrame()->colour, 0x50FFFFFFu);
	// no fade of its own
	EXPECT_EQ(_player->EndFrame(), 1200);
	EXPECT_EQ(_player->FadeStartFrame(), 1200);
}

TEST_F(FallingSpellVideoTest, TheFilmNeverFadesByItself)
{
	_falling->KickOff();
	// the film's service alone (the white fade would end the spell at ~1078 frames): past where a normal film's 5 s
	// fade would start (frame 1080), the alpha stays 1 and the game paused
	for (int guard = 0; guard < 2000000 && _player->CurrentFrame() < 1150; ++guard)
	{
		_player->Process(1);
	}
	EXPECT_EQ(_player->CurrentFrame(), 1150);
	EXPECT_EQ(_player->Alpha(), 1.0f);
	EXPECT_TRUE(paused);
}

TEST_F(FallingSpellVideoTest, StatesAndSounds)
{
	_falling->KickOff();
	RunTo(322);
	EXPECT_EQ(_falling->State(), 0);
	EXPECT_TRUE(sounds.empty());
	RunTo(323); // 13458 ms > 13450
	EXPECT_EQ(_falling->State(), 1);
	EXPECT_TRUE(_falling->SparklesOn());
	ASSERT_EQ(sounds.size(), 2u);
	EXPECT_EQ(sounds[0], (FallingSpellSound {Kind::Play, Bank::InGame, 168, 0, 100, Cue::CitadelExplode}));
	EXPECT_EQ(sounds[1], (FallingSpellSound {Kind::Play, Bank::InGame, 172, 1, 100, Cue::Volcano}));

	RunTo(420); // 17500 ms > 17450
	EXPECT_EQ(_falling->SoundState(), 1);
	ASSERT_EQ(sounds.size(), 3u);
	EXPECT_EQ(sounds[2], (FallingSpellSound {Kind::Play, Bank::ScriptSfx, 151, 0, 100, Cue::Rumble}));
	RunTo(468); // 19500 ms > 19450
	EXPECT_EQ(_falling->SoundState(), 2);
	ASSERT_EQ(sounds.size(), 5u);
	EXPECT_EQ(sounds[3], (FallingSpellSound {Kind::Play, Bank::Spells, 56, 0, 100, Cue::LaserExplode}));
	EXPECT_EQ(sounds[4], (FallingSpellSound {Kind::Stop, Bank::InGame, 172, 1, 100, Cue::VolcanoStop}));
	RunTo(760); // 31666 ms > 31650
	EXPECT_EQ(_falling->SoundState(), 3);
	ASSERT_EQ(sounds.size(), 6u);
	EXPECT_EQ(sounds[5], (FallingSpellSound {Kind::Play, Bank::InGame, 166, 0, 100, Cue::Creed}));
	RunTo(907); // 37791 ms > 37750
	EXPECT_EQ(_falling->State(), 2);
	ASSERT_EQ(sounds.size(), 8u);
	EXPECT_EQ(sounds[6], (FallingSpellSound {Kind::Play, Bank::Spells, 30, 0, 100, Cue::HealChakra}));
	EXPECT_EQ(sounds[7], (FallingSpellSound {Kind::Play, Bank::InGame, 166, 2, 0x85, Cue::CreedHigh}));
	EXPECT_TRUE(fadeColours.empty());
	EXPECT_TRUE(musicStops.empty());

	RunTo(1054); // 43916 ms > 43900
	EXPECT_EQ(_falling->State(), 3);
	ASSERT_EQ(sounds.size(), 11u);
	EXPECT_EQ(sounds[8], (FallingSpellSound {Kind::Stop, Bank::InGame, 166, 0, 100, Cue::CreedStop}));
	EXPECT_EQ(sounds[9], (FallingSpellSound {Kind::Stop, Bank::InGame, 166, 2, 100, Cue::CreedHighStop}));
	EXPECT_EQ(sounds[10], (FallingSpellSound {Kind::Play, Bank::InGame, 168, 0, 100, Cue::CitadelExplodeEnd}));
	EXPECT_EQ(musicStops, std::vector<int32_t> {1}); // the music fades out
	EXPECT_EQ(_falling->GetTempleFade().rgb, 0xFFFFFFu);
	EXPECT_EQ(_falling->GetTempleFade().target, 1.0f);
	ASSERT_FALSE(fadeColours.empty()); // the same frame's temple fade
	EXPECT_EQ(fadeColours.back() & 0x00FFFFFFu, 0xFFFFFFu);
}

TEST_F(FallingSpellVideoTest, TheWhiteFadeEndsIt)
{
	_falling->KickOff();
	RunTo(1054);
	ASSERT_EQ(_falling->State(), 3);
	// 1 s of frame time up to white: 25 frames of 40 ms (the film goes on, ~1 frame each)
	int frames = 0;
	while (_falling->IsActive() && frames < 100)
	{
		Frame(40);
		++frames;
	}
	EXPECT_FALSE(_falling->IsActive()); // state 4 ends the falling spell video
	EXPECT_EQ(_falling->Mode(), k_SequenceModeNone);
	EXPECT_FALSE(_falling->HidesWorld());
	EXPECT_EQ(frames, 26);                      // 25 frames to 1.0, the 26th sees done != 0
	EXPECT_EQ(fadeColours.back(), 0xF4FFFFFFu); // the fade back started: 1 - 0.04 = 0.96 -> 244
	// the player's skip without the falling spell video: the normal skip, base alpha 0xFF, 48 frames from now, the
	// pause back
	ASSERT_TRUE(_player->IsPlaying());
	const int32_t frame = _player->CurrentFrame();
	EXPECT_EQ(_player->FadeStartFrame(), frame);
	EXPECT_EQ(_player->EndFrame(), frame + 48);
	EXPECT_FALSE(paused);
	EXPECT_EQ(_player->GetFrame()->colour, 0xFFFFFFFFu);
	EXPECT_TRUE(_player->CoversScreen()); // alpha 1.0 and no FallingSpellVideo: the world is not drawn this frame
	// the fade back from white keeps going in mode 0 (target != current)
	for (int i = 0; i < 30; ++i)
	{
		Frame(40);
	}
	EXPECT_EQ(fadeColours.back(), 0x00000000u);
	EXPECT_EQ(_falling->GetTempleFade().current, 0.0f);
	const auto count = fadeColours.size();
	Frame(40);
	EXPECT_EQ(fadeColours.size(), count); // target == current: no more Temple fade
}

TEST_F(FallingSpellVideoTest, EscapeSkips)
{
	_falling->KickOff();
	RunTo(100);
	EXPECT_TRUE(_player->EscapeKey(false, false)); // the skip ends the falling spell video, which skips the film
	EXPECT_FALSE(_falling->IsActive());
	EXPECT_EQ(_falling->Mode(), k_SequenceModeNone);
	EXPECT_EQ(_player->FadeStartFrame(), 100);
	EXPECT_EQ(_player->EndFrame(), 148);
	EXPECT_FALSE(paused);
	EXPECT_TRUE(musicStops.empty()); // only 43.9 s stops the music (ESC's StartScriptMusic(0) is VideoPlayer's hook)
}

TEST_F(FallingSpellVideoTest, StopAviSequence)
{
	_falling->End(); // stopping AVI sequence 2 without the falling spell video: nothing
	EXPECT_FALSE(_player->IsPlaying());
	_falling->KickOff();
	RunTo(10);
	_falling->End(); // stopping AVI sequence 2
	EXPECT_FALSE(_falling->IsActive());
	EXPECT_EQ(_player->FadeStartFrame(), 10);
	EXPECT_EQ(_player->EndFrame(), 58);
	_falling->End(); // a second time: nothing
	EXPECT_EQ(_player->EndFrame(), 58);
}

TEST_F(FallingSpellVideoTest, AShortFilmEndsIt)
{
	_film = Write("short.bik", MakeBik(48, 24)); // 2 s: the film ends before any state
	_falling->KickOff();
	RunTo(1000);
	EXPECT_FALSE(_falling->IsActive()); // no film: the falling spell video ends
	EXPECT_FALSE(_player->IsPlaying());
	EXPECT_EQ(_falling->Mode(), k_SequenceModeNone);
	EXPECT_EQ(_falling->State(), 1); // + 1 the frame the film was gone
}

TEST_F(FallingSpellVideoTest, AMissingFilmEndsItAtOnce)
{
	_film = _folder / "missing.bik";
	_falling->KickOff();
	EXPECT_TRUE(_falling->IsActive());
	EXPECT_TRUE(_player->IsPlaying()); // the player exists even without the file
	Frame(1);                          // the film's service deletes it (0 >= 0), then case 2 ends the spell
	EXPECT_FALSE(_falling->IsActive());
	EXPECT_EQ(_falling->Mode(), k_SequenceModeNone);
}

TEST_F(FallingSpellVideoTest, KickOffTwice)
{
	_falling->KickOff();
	RunTo(500);
	ASSERT_EQ(_falling->State(), 1);
	_falling->KickOff(); // the previous one ended, then a new one from frame 0
	EXPECT_TRUE(_falling->IsActive());
	EXPECT_EQ(_falling->State(), 0);
	EXPECT_EQ(_falling->SoundState(), 0);
	EXPECT_EQ(_player->CurrentFrame(), 0);
	EXPECT_EQ(_player->FadeStartFrame(), 1200);
	EXPECT_TRUE(paused);
}
