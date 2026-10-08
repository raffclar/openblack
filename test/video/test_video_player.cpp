/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// video::VideoPlayer (src/Video/VideoPlayer.h): the game's full screen film with fake hooks and a fake wall clock,
// no GPU: starting a film, the intro's schedule, the per frame fade and draw, the skip, the ESC key, deleting a film
// and the end of a finished film.
// The films are small synthetic .bik files in a temporary folder.

#include <cstdint>

#include <atomic>
#include <filesystem>
#include <fstream>
#include <memory>
#include <span>
#include <string>
#include <vector>

#ifdef _WIN32
#include <process.h>
#else
#include <unistd.h>
#endif

#include <gtest/gtest.h>

#include "3D/ScreenFade.h"
// the tests build their own file system
#define LOCATOR_IMPLEMENTATIONS
#include "FileSystem/DefaultFileSystem.h"
#include "Graphics/Rgb16.h"
#include "Locator.h"
#include "Video/BikFile.h"
#include "Video/VideoDecoder.h"
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

/// A Bink 1 file of `frames` two-byte frames (even: bit 0 of an offset is the key frame flag), `fps` / 1
std::vector<uint8_t> MakeBik(uint32_t frames, uint32_t fps, uint32_t width = 4, uint32_t height = 2)
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
	PutU32(data, 20, width);
	PutU32(data, 24, height);
	PutU32(data, 28, fps);
	PutU32(data, 32, 1);
	return data;
}

/// Every frame one colour: R = the frame index, G = 0x80, B = 0x37
class ColourDecoder final: public IVideoDecoder
{
public:
	explicit ColourDecoder(std::shared_ptr<std::vector<uint32_t>> decoded)
	    : _decoded(std::move(decoded))
	{
	}
	bool Open(const BikFile& file) override
	{
		_frames = file.FrameCount();
		_pixels.assign(static_cast<size_t>(file.Width()) * file.Height() * 4, 0);
		return true;
	}
	std::span<const uint8_t> DecodeNext(uint32_t index) override
	{
		_decoded->push_back(index);
		if (index >= _frames)
		{
			return {};
		}
		for (size_t i = 0; i < _pixels.size(); i += 4)
		{
			_pixels[i + 0] = static_cast<uint8_t>(index);
			_pixels[i + 1] = 0x80;
			_pixels[i + 2] = 0x37;
			_pixels[i + 3] = 0x00;
		}
		return _pixels;
	}

private:
	std::shared_ptr<std::vector<uint32_t>> _decoded;
	uint32_t _frames {0};
	std::vector<uint8_t> _pixels;
};

class VideoPlayerTest: public ::testing::Test
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
		          ("openblack_test_video_" + std::to_string(::testing::UnitTest::GetInstance()->random_seed()) + "_" +
		           std::to_string(s_count++) + "_" + UniqueSuffix());
		std::filesystem::create_directories(_folder);
		_player = std::make_unique<VideoPlayer>(MakeHooks());
	}
	void TearDown() override
	{
		_player.reset();
		std::error_code ec;
		std::filesystem::remove_all(_folder, ec);
	}

	VideoPlayer::Hooks MakeHooks()
	{
		VideoPlayer::Hooks hooks;
		hooks.isPaused = [this]() { return paused; };
		hooks.pauseGame = [this](bool p) {
			pauseCalls.push_back(p);
			paused = p;
		};
		hooks.wideScreen = [this]() { return wideScreen; };
		hooks.setWideScreen = [this](int32_t on) {
			wideScreenCalls.push_back(on);
			wideScreen = on;
		};
		hooks.snapWideScreen = [this]() { ++wideScreenSnaps; };
		hooks.stopScriptMusic = [this]() { ++musicStops; };
		hooks.endFallingSpellVideo = [this]() { ++fallingEnds; };
		hooks.makeDecoder = [this]() { return std::make_unique<ColourDecoder>(decoded); };
		return hooks;
	}

	std::filesystem::path Write(const std::string& name, const std::vector<uint8_t>& data)
	{
		const auto path = _folder / name;
		std::ofstream(path, std::ios::binary)
		    .write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
		return path;
	}

	/// Process frames of `ms` until the film has decoded `frames` frames (it stops early if the film ends)
	void RunTo(int32_t frames, uint32_t ms = 1)
	{
		for (int guard = 0; guard < 1000000 && _player->IsPlaying() && _player->CurrentFrame() < frames; ++guard)
		{
			_player->Process(ms);
		}
	}

	bool paused {false};
	int32_t wideScreen {0};
	std::vector<bool> pauseCalls;
	std::vector<int32_t> wideScreenCalls;
	int musicStops {0};
	int wideScreenSnaps {0};
	int fallingEnds {0};
	std::shared_ptr<std::vector<uint32_t>> decoded = std::make_shared<std::vector<uint32_t>>();
	std::unique_ptr<VideoPlayer> _player;
	std::filesystem::path _folder;
};
} // namespace

TEST(VideoPlayerMaths, FadeAlpha)
{
	// 1 - (f - start) / (|end - start| + 1)
	EXPECT_FLOAT_EQ(FadeAlpha(1393, 1392, 1440), 1.0f - 1.0f / 49.0f);
	EXPECT_FLOAT_EQ(FadeAlpha(1440, 1392, 1440), 1.0f - 48.0f / 49.0f);
	EXPECT_EQ(FadeAlpha(1392, 1392, 1440), 1.0f);
	EXPECT_EQ(FadeAlpha(11, 10, 20), 1.0f - 1.0f / 11.0f); // the float arithmetic, bit for bit
	// a schedule the wrong way round: |end - start|
	EXPECT_FLOAT_EQ(FadeAlpha(12, 10, 5), 1.0f - 2.0f / 6.0f);
}

TEST(VideoPlayerMaths, VertexColour)
{
	// 0x00FFFFFF | ftol(base * alpha) << 24
	EXPECT_EQ(VertexColour(1.0f, false), 0xFFFFFFFFu);
	EXPECT_EQ(VertexColour(0.0f, false), 0x00FFFFFFu);
	EXPECT_EQ(VertexColour(0.5f, false), 0x7FFFFFFFu); // 127.5 truncated
	EXPECT_EQ(VertexColour(1.0f, true), 0x50FFFFFFu);
	EXPECT_EQ(VertexColour(0.5f, true), 0x28FFFFFFu);
	EXPECT_EQ(VertexColour(0.999f, false), 0xFEFFFFFFu);
}

TEST(VideoPlayerMaths, FullScreenRect)
{
	// bars = ftol(H - W * 0.5625) / 2, (0, bars, W + 1, H - 2 bars + 1)
	auto r = FullScreenRect(1024, 768);
	EXPECT_EQ(r.x, 0);
	EXPECT_EQ(r.y, 96);
	EXPECT_EQ(r.width, 1025);
	EXPECT_EQ(r.height, 577);
	r = FullScreenRect(1920, 1080);
	EXPECT_EQ(r.y, 0);
	EXPECT_EQ(r.height, 1081);
	r = FullScreenRect(1280, 1024); // 1024 - 720 = 304
	EXPECT_EQ(r.y, 152);
	EXPECT_EQ(r.height, 721);
	r = FullScreenRect(801, 600); // 600 - 450.5625 = 149.4375 -> 149 -> 74 (signed division)
	EXPECT_EQ(r.y, 74);
	EXPECT_EQ(r.height, 453);
	// wider than 16:9: no clamp, the picture overflows (unlike ScreenFade::LetterboxHeight)
	r = FullScreenRect(2560, 1080); // 1080 - 1440 = -360
	EXPECT_EQ(r.y, -180);
	EXPECT_EQ(r.height, 1441);
	r = FullScreenRect(1921, 1080); // -0.5625 -> 0
	EXPECT_EQ(r.y, 0);
	r = FullScreenRect(1924, 1080); // -2.25 -> -2 -> -1
	EXPECT_EQ(r.y, -1);
}

TEST(VideoPlayerMaths, BarsFullAtOnceDuringTheFilm)
{
	// starting a film: SetWideScreen(1, 0) then the bars snapped full: 100 % at once,
	// and still 100 % with the game paused (0 ms)
	openblack::ScreenFade fade;
	fade.SetWideScreen(true, 2.0f);
	fade.SnapWideScreen();
	fade.UpdateWideScreen(0.0f);
	EXPECT_EQ(fade.GetWideScreenFraction(), 1.0f);
	// the bar height at 100 % is where the film starts: the film fits between the bars
	EXPECT_EQ(openblack::ScreenFade::LetterboxHeight(1024, 768, fade.GetWideScreenFraction()), FullScreenRect(1024, 768).y);
	fade.UpdateWideScreen(0.0f);
	EXPECT_EQ(fade.GetWideScreenFraction(), 1.0f);
	// the end of the film: SetWideScreen(0, 0), the bars leave in 2 s of game time
	fade.SetWideScreen(false, 2.0f);
	fade.UpdateWideScreen(0.0f);
	EXPECT_EQ(fade.GetWideScreenFraction(), 1.0f);
	fade.UpdateWideScreen(500.0f);
	EXPECT_FLOAT_EQ(fade.GetWideScreenFraction(), 0.75f);
	fade.UpdateWideScreen(1500.0f);
	EXPECT_EQ(fade.GetWideScreenFraction(), 0.0f);
	EXPECT_TRUE(fade.IsWideScreenTransitionFinished());
}

TEST(VideoPlayerMaths, FramesDue)
{
	EXPECT_EQ(FramesDue(0, 24, 1), 1u);
	EXPECT_EQ(FramesDue(41, 24, 1), 1u);
	EXPECT_EQ(FramesDue(42, 24, 1), 2u);
	EXPECT_EQ(FramesDue(1000, 24, 1), 25u);
	EXPECT_EQ(FramesDue(60000, 24, 1), 1441u);
	EXPECT_EQ(FramesDue(1001, 30000, 1001), 31u);
	EXPECT_EQ(FramesDue(5, 0, 1), 0u);
}

TEST_F(VideoPlayerTest, PlayKeepsThePauseAndTheWideScreen)
{
	const auto path = Write("film.bik", MakeBik(240, 24));
	EXPECT_FALSE(_player->IsPlaying());
	EXPECT_FALSE(_player->CoversScreen());
	EXPECT_FALSE(_player->GetFrame().has_value());
	ASSERT_TRUE(_player->Play(path));
	EXPECT_TRUE(_player->IsPlaying());
	EXPECT_FALSE(_player->PreviousPause());
	EXPECT_EQ(pauseCalls, (std::vector<bool> {true})); // PauseGame(1)
	EXPECT_EQ(_player->PreviousWideScreen(), 0);
	EXPECT_EQ(wideScreenCalls, (std::vector<int32_t> {1}));
	EXPECT_EQ(_player->Fps(), 24);
	EXPECT_EQ(_player->TotalFrames(), 240);
	EXPECT_EQ(_player->CurrentFrame(), 0);
	EXPECT_EQ(_player->EndFrame(), 240);
	EXPECT_EQ(_player->FadeStartFrame(), 240 - 24 * 5);
	EXPECT_EQ(_player->Alpha(), 1.0f);
	EXPECT_FALSE(_player->IsIntro());
	EXPECT_TRUE(_player->CoversScreen());
}

TEST_F(VideoPlayerTest, PlayWhilePausedAndWide)
{
	paused = true;
	wideScreen = 1;
	ASSERT_TRUE(_player->Play(Write("film.bik", MakeBik(48, 24))));
	EXPECT_TRUE(_player->PreviousPause());
	EXPECT_EQ(_player->PreviousWideScreen(), 1);
	EXPECT_TRUE(wideScreenCalls.empty()); // already on
	EXPECT_EQ(wideScreenSnaps, 1);        // the bars snapped even then: the script's bars at 100 % at once
	RunTo(1000);
	EXPECT_FALSE(_player->IsPlaying());
	_player->Process(1); // the end of the film
	EXPECT_TRUE(paused);
	EXPECT_EQ(wideScreen, 1);
	EXPECT_TRUE(wideScreenCalls.empty());
}

TEST_F(VideoPlayerTest, MissingFileEndsOnTheNextFrame)
{
	EXPECT_FALSE(_player->Play(_folder / "missing.bik"));
	// the player exists anyway: 0 frames, fps 1
	EXPECT_TRUE(_player->IsPlaying());
	EXPECT_TRUE(paused);
	EXPECT_EQ(wideScreen, 1);
	EXPECT_EQ(_player->Fps(), 1);
	EXPECT_EQ(_player->EndFrame(), 0);
	EXPECT_EQ(_player->FadeStartFrame(), -5);
	const auto frame = _player->GetFrame();
	ASSERT_TRUE(frame.has_value());
	EXPECT_TRUE(frame->rgba.empty());
	_player->Process(16);
	// frame 0 >= end 0 -> alpha 0, the film deleted
	EXPECT_FALSE(_player->IsPlaying());
	EXPECT_EQ(_player->Alpha(), 0.0f);
	EXPECT_EQ(_player->FinishedCount(), 1);
	EXPECT_TRUE(paused); // the end of the film is the next frame's
	_player->Process(16);
	EXPECT_FALSE(paused);
	EXPECT_EQ(wideScreen, 0);
	EXPECT_EQ(wideScreenCalls, (std::vector<int32_t> {1, 0}));
	EXPECT_EQ(_player->FinishedCount(), 0);
	EXPECT_TRUE(decoded->empty());
}

TEST_F(VideoPlayerTest, FramesFollowTheWallClock)
{
	ASSERT_TRUE(_player->Play(Write("film.bik", MakeBik(240, 24))));
	_player->Process(500); // the first Process starts the clock: frame 0 only
	EXPECT_EQ(_player->CurrentFrame(), 1);
	_player->Process(41); // 41 ms: frame 1 is due at 41.67 ms
	EXPECT_EQ(_player->CurrentFrame(), 1);
	_player->Process(1);
	EXPECT_EQ(_player->CurrentFrame(), 2);
	_player->Process(1000); // a long frame: every frame due is decoded (BINKNOSKIP)
	EXPECT_EQ(_player->CurrentFrame(), 26);
	EXPECT_EQ(*decoded, ([] {
		std::vector<uint32_t> v;
		for (uint32_t i = 0; i < 26; ++i)
		{
			v.push_back(i);
		}
		return v;
	})());
}

TEST_F(VideoPlayerTest, FadeAndEnd)
{
	ASSERT_TRUE(_player->Play(Write("film.bik", MakeBik(240, 24))));
	_player->SetSchedule(10, 20);
	RunTo(10);
	EXPECT_EQ(_player->CurrentFrame(), 10);
	EXPECT_EQ(_player->Alpha(), 1.0f);
	EXPECT_TRUE(paused);
	EXPECT_TRUE(_player->CoversScreen());
	RunTo(11);
	// frame 11 > 10: the pause given back, alpha 1 - 1 / 11
	EXPECT_FALSE(paused);
	EXPECT_EQ(_player->Alpha(), 1.0f - 1.0f / 11.0f);
	EXPECT_FALSE(_player->CoversScreen());
	EXPECT_EQ(_player->GetFrame()->colour, VertexColour(1.0f - 1.0f / 11.0f, false));
	// a pause asked in the fade zone is undone each frame
	paused = true;
	_player->Process(0);
	EXPECT_FALSE(paused);
	RunTo(19);
	EXPECT_EQ(_player->Alpha(), 1.0f - 9.0f / 11.0f);
	EXPECT_TRUE(_player->IsPlaying());
	RunTo(20);
	// frame 20 >= end 20: alpha 0 and the film gone; frame 19 (index) was decoded but never drawn
	EXPECT_FALSE(_player->IsPlaying());
	EXPECT_EQ(_player->Alpha(), 0.0f);
	EXPECT_FALSE(_player->CoversScreen());
	EXPECT_EQ(wideScreen, 1);
	_player->Process(16);
	EXPECT_EQ(wideScreen, 0); // the end of the film
	EXPECT_FALSE(paused);
}

TEST_F(VideoPlayerTest, IntroSchedule)
{
	// the intro: fps * 58 / fps * 60 and the intro flag set
	ASSERT_TRUE(_player->Play(Write("intro.bik", MakeBik(1601, 24))));
	_player->ScheduleIntro();
	EXPECT_EQ(_player->FadeStartFrame(), 1392);
	EXPECT_EQ(_player->EndFrame(), 1440);
	EXPECT_TRUE(_player->IsIntro());
	// at the wall clock: frame 1392 at 58 s, the end at 60 s
	_player->Process(0);
	for (int i = 0; i < 57999; ++i)
	{
		_player->Process(1);
	}
	EXPECT_EQ(_player->CurrentFrame(), 1392); // frames 0..1391 decoded
	EXPECT_TRUE(paused);
	EXPECT_EQ(_player->Alpha(), 1.0f);
	_player->Process(1); // 58 s: frame 1392 decoded, 1393 > 1392: the fade zone, the pause given back
	EXPECT_EQ(_player->CurrentFrame(), 1393);
	EXPECT_FALSE(paused);
	EXPECT_EQ(_player->Alpha(), 1.0f - 1.0f / 49.0f);
	for (int i = 0; i < 2000 && _player->IsPlaying(); ++i)
	{
		_player->Process(1);
	}
	EXPECT_FALSE(_player->IsPlaying());
	EXPECT_EQ(decoded->back(), 1439u); // frames 1440..1600 are never shown
	// a new film clears the intro flag
	ASSERT_TRUE(_player->Play(Write("film.bik", MakeBik(24, 24))));
	EXPECT_FALSE(_player->IsIntro());
	// without a film, nothing
	_player->Stop();
	_player->ScheduleIntro();
	EXPECT_FALSE(_player->IsIntro());
}

TEST_F(VideoPlayerTest, SkipFadesOver48Frames)
{
	ASSERT_TRUE(_player->Play(Write("film.bik", MakeBik(240, 24))));
	RunTo(30);
	pauseCalls.clear();
	_player->Skip();
	// the skip: start = f, end = min(f + 48, total), the pause from before the film given back
	EXPECT_EQ(_player->FadeStartFrame(), 30);
	EXPECT_EQ(_player->EndFrame(), 78);
	EXPECT_EQ(pauseCalls, (std::vector<bool> {false}));
	EXPECT_FALSE(paused);
	EXPECT_TRUE(_player->IsPlaying());
	EXPECT_EQ(_player->Alpha(), 1.0f);
	RunTo(31);
	EXPECT_EQ(_player->Alpha(), 1.0f - 1.0f / 49.0f);
	// skipping again in the fade zone ends the film at once
	_player->Skip();
	EXPECT_FALSE(_player->IsPlaying());
	EXPECT_EQ(_player->FinishedCount(), 1);
}

TEST_F(VideoPlayerTest, SkipNearTheEnd)
{
	ASSERT_TRUE(_player->Play(Write("film.bik", MakeBik(100, 24))));
	_player->SetSchedule(99, 100);
	RunTo(80);
	_player->Skip();
	EXPECT_EQ(_player->FadeStartFrame(), 80);
	EXPECT_EQ(_player->EndFrame(), 100); // never past the last frame
}

TEST_F(VideoPlayerTest, SkipTheFallingSpellsFilm)
{
	ASSERT_TRUE(_player->Play(Write("fall.bik", MakeBik(100, 24))));
	_player->SetFallingSpellVideo(true);
	EXPECT_FALSE(_player->CoversScreen()); // drawn over the world
	EXPECT_EQ(_player->GetFrame()->colour, 0x50FFFFFFu);
	_player->Skip();
	EXPECT_EQ(fallingEnds, 1);
	EXPECT_EQ(_player->EndFrame(), 100);
}

TEST_F(VideoPlayerTest, EscapeKey)
{
	EXPECT_FALSE(_player->EscapeKey(false, false)); // no film: the key's other use
	ASSERT_TRUE(_player->Play(Write("film.bik", MakeBik(240, 24))));
	RunTo(5);
	EXPECT_TRUE(_player->EscapeKey(true, false)); // Shift: nothing
	EXPECT_TRUE(_player->EscapeKey(false, true)); // Ctrl: nothing
	EXPECT_EQ(_player->EndFrame(), 240);
	EXPECT_EQ(musicStops, 0);
	_player->SetNoSkip(true);
	EXPECT_TRUE(_player->EscapeKey(false, false)); // no skip: taken, nothing
	EXPECT_EQ(_player->EndFrame(), 240);
	_player->SetNoSkip(false);
	EXPECT_TRUE(_player->EscapeKey(false, false));
	EXPECT_EQ(_player->FadeStartFrame(), 5);
	EXPECT_EQ(_player->EndFrame(), 53);
	EXPECT_EQ(musicStops, 1); // StartScriptMusic(0)
}

TEST_F(VideoPlayerTest, NoSkipClearedByTheEnd)
{
	ASSERT_TRUE(_player->Play(Write("film.bik", MakeBik(24, 24))));
	_player->SetNoSkip(true);
	_player->Stop();
	EXPECT_FALSE(_player->NoSkip());
	_player->SetNoSkip(true);
	_player->Process(1); // the end of the film
	EXPECT_FALSE(_player->NoSkip());
}

TEST_F(VideoPlayerTest, ANewFilmReplacesTheOld)
{
	ASSERT_TRUE(_player->Play(Write("a.bik", MakeBik(240, 24))));
	RunTo(3);
	ASSERT_TRUE(_player->Play(Write("b.bik", MakeBik(100, 24))));
	// the old film deleted (+1), then the finished count cleared: the first film's end never runs, and the
	// pause kept is the first film's (paused)
	EXPECT_EQ(_player->FinishedCount(), 0);
	EXPECT_TRUE(_player->PreviousPause());
	EXPECT_EQ(_player->PreviousWideScreen(), 1);
	EXPECT_EQ(_player->TotalFrames(), 100);
	EXPECT_EQ(_player->CurrentFrame(), 0);
	RunTo(1000);
	_player->Process(1);
	EXPECT_TRUE(paused); // given back to the first film's "paused"
	EXPECT_EQ(wideScreen, 1);
}

TEST_F(VideoPlayerTest, FrameGoesThrough16Bits)
{
	ASSERT_TRUE(_player->Play(Write("film.bik", MakeBik(240, 24, 4, 2))));
	auto frame = _player->GetFrame();
	ASSERT_TRUE(frame.has_value());
	// before the first frame: the zeroed framebuffer, opaque black
	EXPECT_EQ(frame->width, 4u);
	EXPECT_EQ(frame->height, 2u);
	ASSERT_EQ(frame->rgba.size(), 32u);
	ASSERT_EQ(frame->rgb16.size(), 8u);
	EXPECT_EQ(frame->rgba[0], 0);
	EXPECT_EQ(frame->rgba[3], 0xFF);
	EXPECT_EQ(frame->serial, 0u);
	EXPECT_EQ(frame->format, graphics::rgb16::Format::Rgb555);
	EXPECT_EQ(frame->colour, 0xFFFFFFFFu);
	RunTo(0x2C);
	frame = _player->GetFrame();
	ASSERT_TRUE(frame.has_value());
	EXPECT_EQ(frame->serial, 0x2Cu);
	EXPECT_EQ(frame->frame, 0x2C);
	// the last frame decoded, index 0x2B: (0x2B, 0x80, 0x37) -> 555 -> sampled
	EXPECT_EQ(frame->rgb16[7], graphics::rgb16::Pack555(0x2B, 0x80, 0x37));
	EXPECT_EQ(frame->rgba[28], graphics::rgb16::Cut5(0x2B));
	EXPECT_EQ(frame->rgba[29], graphics::rgb16::Cut5(0x80));
	EXPECT_EQ(frame->rgba[30], graphics::rgb16::Cut5(0x37));
	EXPECT_EQ(frame->rgba[31], 0xFF); // the decoder's alpha is not kept

	_player->SetFormat(graphics::rgb16::Format::Rgb565);
	RunTo(0x2D);
	frame = _player->GetFrame();
	EXPECT_EQ(frame->rgb16[0], graphics::rgb16::Pack565(0x2C, 0x80, 0x37));
	EXPECT_EQ(frame->rgba[1], graphics::rgb16::Cut6(0x80));
}

TEST_F(VideoPlayerTest, NullDecoderIsBlack)
{
	auto hooks = MakeHooks();
	hooks.makeDecoder = nullptr; // NullVideoDecoder
	VideoPlayer player(std::move(hooks));
	ASSERT_TRUE(player.Play(Write("film.bik", MakeBik(10, 24, 2, 2))));
	player.Process(0);
	const auto frame = player.GetFrame();
	ASSERT_TRUE(frame.has_value());
	EXPECT_EQ(frame->serial, 1u);
	EXPECT_EQ(std::vector<uint8_t>(frame->rgba.begin(), frame->rgba.end()),
	          (std::vector<uint8_t> {0, 0, 0, 0xFF, 0, 0, 0, 0xFF, 0, 0, 0, 0xFF, 0, 0, 0, 0xFF}));
}

TEST_F(VideoPlayerTest, AFailedFrameKeepsThePicture)
{
	// frames past the file (a schedule beyond the last frame): DecodeNext fails, the frame count still goes on
	ASSERT_TRUE(_player->Play(Write("film.bik", MakeBik(3, 24))));
	_player->SetSchedule(100, 200);
	RunTo(6);
	EXPECT_TRUE(_player->IsPlaying());
	EXPECT_EQ(_player->CurrentFrame(), 6);
	EXPECT_EQ(_player->GetFrame()->serial, 3u);
	// past total + 1 the decoder is not even asked
	EXPECT_EQ(*decoded, (std::vector<uint32_t> {0, 1, 2, 3}));
}

TEST_F(VideoPlayerTest, ACatchUpShowsTheLastFrameDecoded)
{
	// a long frame (a hitch): every frame due is decoded in order, each one counted, and the picture is the last one's
	ASSERT_TRUE(_player->Play(Write("film.bik", MakeBik(240, 24, 4, 2))));
	_player->Process(0);
	ASSERT_EQ(_player->GetFrame()->serial, 1u);
	_player->Process(500); // 12 more frames due at 24 fps
	const auto frame = _player->GetFrame();
	ASSERT_TRUE(frame.has_value());
	const auto last = static_cast<uint8_t>(frame->frame - 1);
	EXPECT_GT(frame->frame, 2);
	EXPECT_EQ(frame->serial, static_cast<uint32_t>(frame->frame));
	EXPECT_EQ(decoded->size(), static_cast<size_t>(frame->frame));
	for (size_t i = 0; i < 8; ++i)
	{
		EXPECT_EQ(frame->rgb16[i], graphics::rgb16::Pack555(last, 0x80, 0x37)) << i;
		EXPECT_EQ(frame->rgba[i * 4 + 0], graphics::rgb16::Cut5(last)) << i;
	}
}

TEST_F(VideoPlayerTest, ACatchUpEndingOnAFailedFrameShowsTheLastGoodOne)
{
	// frames 0..2 exist; a catch-up over 0..5 shows frame 2, as when each was converted in turn
	ASSERT_TRUE(_player->Play(Write("film.bik", MakeBik(3, 24, 4, 2))));
	_player->SetSchedule(100, 200);
	_player->Process(0);
	_player->Process(250);
	const auto frame = _player->GetFrame();
	ASSERT_TRUE(frame.has_value());
	EXPECT_EQ(frame->serial, 3u);
	EXPECT_EQ(frame->rgb16[0], graphics::rgb16::Pack555(2, 0x80, 0x37));
	EXPECT_EQ(frame->rgba[0], graphics::rgb16::Cut5(2));
}
