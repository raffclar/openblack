/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "VideoPlayer.h"

#include <algorithm>
#include <limits>
#include <utility>

#include <spdlog/spdlog.h>

#include "3D/ScreenFade.h"
#include "Audio/Services/GameMusic.h"
#include "ECS/Systems/ScreenFadeSystemInterface.h"
#include "ECS/Systems/VideoSystemInterface.h"
#include "FallingSpellVideo.h"
#ifdef OPENBLACK_USE_BINK
#include "BinkDecoder.h"
#endif
#include "Game.h"
#include "GameClock.h"
#include "Help/HelpSystem.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::video;

float video::FadeAlpha(int32_t frame, int32_t fadeStart, int32_t end) noexcept
{
	// f - start, end - start and |end - start| + 1 in 32-bit wrapping arithmetic, as unsigned here
	const auto done = static_cast<int32_t>(static_cast<uint32_t>(frame) - static_cast<uint32_t>(fadeStart));
	const auto length = static_cast<int32_t>(static_cast<uint32_t>(end) - static_cast<uint32_t>(fadeStart));
	const uint32_t sign = length < 0 ? 0xFFFFFFFFu : 0u;
	const auto span = static_cast<int32_t>(((static_cast<uint32_t>(length) ^ sign) - sign) + 1u);
	// int to float, divide, subtract from 1.0: each rounded to 24 bits
	return 1.0f - static_cast<float>(done) / static_cast<float>(span);
}

uint32_t video::VertexColour(float alpha, bool fallingSpell) noexcept
{
	const uint32_t base = fallingSpell ? k_FallingSpellAlpha : k_OpaqueAlpha;
	// base * alpha, truncated, the low byte kept
	const auto byte = static_cast<uint32_t>(static_cast<int32_t>(static_cast<float>(base) * alpha)) & 0xFFu;
	return k_VertexRgb | byte << 24;
}

ScreenRect video::FullScreenRect(int32_t screenWidth, int32_t screenHeight, float letterboxScale) noexcept
{
	// (H - W * 0.5625) * letterboxScale, truncated
	const float bars =
	    (static_cast<float>(screenHeight) - static_cast<float>(screenWidth) * k_LetterboxAspect) * letterboxScale;
	const int32_t letterbox = static_cast<int32_t>(bars) / 2; // signed halving
	// (0, bars, W + 1, H - 2 bars + 1)
	return {0, letterbox, screenWidth + 1, screenHeight - 2 * letterbox + 1};
}

uint32_t video::FramesDue(uint64_t elapsedMs, uint32_t fpsNumerator, uint32_t fpsDenominator) noexcept
{
	if (fpsNumerator == 0 || fpsDenominator == 0)
	{
		return 0;
	}
	const uint64_t due = elapsedMs * fpsNumerator / (static_cast<uint64_t>(fpsDenominator) * 1000u) + 1u;
	return static_cast<uint32_t>(std::min<uint64_t>(due, std::numeric_limits<uint32_t>::max()));
}

VideoPlayer::Hooks VideoPlayer::GameHooks()
{
	Hooks hooks;
	hooks.isPaused = []() { return game_clock::IsPaused(); };
	hooks.pauseGame = [](bool paused) { game_clock::Pause(paused); };
	// the original always has a help system; without a HelpSystem (openblack only, tools) there is no wide screen
	hooks.wideScreen = []() {
		const auto* helpSystem = help::Get();
		return helpSystem != nullptr ? helpSystem->GetWideScreen() : 0;
	};
	// HelpSystem's hook moves the bars (ScreenFade::SetWideScreen) and tells audio (Game.cpp)
	hooks.setWideScreen = [](int32_t on) {
		if (auto* helpSystem = help::Get(); helpSystem != nullptr)
		{
			helpSystem->SetWideScreen(on, 0);
		}
	};
	// the bars at 100 % at once (ScreenFade::SnapWideScreen)
	hooks.snapWideScreen = []() {
		if (Locator::screenFade::has_value())
		{
			Locator::screenFade::value().Fade().SnapWideScreen();
		}
	};
	hooks.stopScriptMusic = []() {
		const auto lock = audio::game_music::Lock();
		if (auto* gameMusic = audio::game_music::Get(); gameMusic != nullptr)
		{
			gameMusic->ScriptStopMusic();
		}
	};
	// the skip of the falling spell's film (FallingSpellVideo.h), which skips again without
	// a falling spell's film
	hooks.endFallingSpellVideo = []() { GetFallingSpell().End(); };
	// openblack's own decoder, unless the films are turned off (or the build has no decoder): then there is no
	// decoder, Open fails and the film is skipped
	hooks.makeDecoder = []() -> std::unique_ptr<IVideoDecoder> {
#ifdef OPENBLACK_USE_BINK
		if (!Locator::videoSystem::has_value() || Locator::videoSystem::value().FilmsEnabled())
		{
			return std::make_unique<BinkDecoder>();
		}
#endif
		return nullptr;
	};
	return hooks;
}

VideoPlayer::VideoPlayer(Hooks hooks)
    : _hooks(std::move(hooks))
{
}

VideoPlayer::~VideoPlayer() = default;

bool VideoPlayer::Play(const std::filesystem::path& path)
{
	// openblack has no loading screen, so no tip video to clear
	_isIntro = false;
	Stop();
	_alpha = 1.0f;
	// the original forgets the music playing (GameMusic::_alignmentType): not ported, audio has no
	// setter (pending; the music service does it again in the fade zone)
	_previousPause = _hooks.isPaused && _hooks.isPaused();
	if (_hooks.pauseGame)
	{
		_hooks.pauseGame(true);
	}
	// the film's sound bank: both callers pass none, not ported
	const bool opened = Open(path, k_FadeSeconds);
	// the original starts a 16 ms poll timer here: Process polls instead (approximate)
	_previousWideScreen = _hooks.wideScreen ? _hooks.wideScreen() : 0;
	if (_previousWideScreen == 0 && _hooks.setWideScreen)
	{
		_hooks.setWideScreen(1);
	}
	// the bars at 100 % at once: reached whether the bars were turned on here or were already on
	if (_hooks.snapWideScreen)
	{
		_hooks.snapWideScreen();
	}
	return opened;
}

bool VideoPlayer::Open(const std::filesystem::path& path, int32_t fadeSeconds)
{
	_finished = 0; // the film's music started flag, also cleared here, is not ported
	_framesReady = 0;
	if (_movie)
	{
		Stop();
	}
	// the player is made whether the file opens or not
	_movie = std::make_unique<Movie>();
	_playing.store(true, std::memory_order_release);
	auto& movie = *_movie;

	// opened for 16 bits, several textures
	bool opened = movie.file.Open(path);
	if (opened)
	{
		movie.decoder = _hooks.makeDecoder ? _hooks.makeDecoder() : std::make_unique<NullVideoDecoder>();
		opened = movie.decoder != nullptr && movie.decoder->Open(movie.file);
		if (movie.decoder == nullptr)
		{
			// a build without a video decoder: the film is skipped as a file that does not open
			if (auto logger = spdlog::get("game"); logger != nullptr)
			{
				logger->info("Video: no video decoder in this build, skipping {}", path.string());
			}
		}
		else if (!opened)
		{
			// openblack: Bink decodes whatever it opened; here a decoder can still refuse a valid container
			// (one it cannot decode): the film then plays black, with its pause, fade and skip
			if (auto logger = spdlog::get("game"); logger != nullptr)
			{
				logger->warn("Video: the decoder refused the film {}: playing it black", path.string());
			}
			movie.decoder = std::make_unique<NullVideoDecoder>();
			opened = movie.decoder->Open(movie.file);
		}
	}
	movie.fps = 1;
	if (opened)
	{
		// the film's summary: width, height, fps, frames, current frame 0
		movie.open = true;
		movie.width = movie.file.Width();
		movie.height = movie.file.Height();
		movie.fps = static_cast<int32_t>(movie.file.Fps());
		movie.totalFrames = static_cast<int32_t>(movie.file.FrameCount());
		movie.frame = 0;
		// the framebuffer: width * height * 2 bytes, zeroed; the textures show what it holds
		movie.framebuffer.assign(static_cast<size_t>(movie.width) * movie.height, 0);
		movie.texture.assign(movie.framebuffer.size() * 4, 0);
		graphics::rgb16::Expand(_format, movie.framebuffer, movie.texture);
	}
	else if (auto logger = spdlog::get("game"); logger != nullptr)
	{
		logger->warn("Video: cannot play {}: {}", path.string(),
		             movie.file.GetError().empty() ? std::string("the decoder refused it") : movie.file.GetError());
	}
	_endFrame = movie.totalFrames;
	_fadeStartFrame = movie.totalFrames - movie.fps * fadeSeconds;
	// the original's pre-roll (up to 10 waits): no frame decoded
	return opened;
}

void VideoPlayer::SetSchedule(int32_t fadeStartFrame, int32_t endFrame)
{
	if (!_movie)
	{
		return;
	}
	_fadeStartFrame = fadeStartFrame;
	_endFrame = endFrame;
}

void VideoPlayer::ScheduleIntro()
{
	if (!_movie)
	{
		return;
	}
	SetSchedule(_movie->fps * k_IntroFadeStartSeconds, _movie->fps * k_IntroEndSeconds);
	_isIntro = true;
}

void VideoPlayer::Process(uint32_t realMs)
{
	// no Bink service to run
	if (_finished != 0)
	{
		FinishedVideo();
	}
	if (!_movie)
	{
		return;
	}
	// (approximate) the original's timer poll: the frames due by the wall clock since the first Process of the film
	if (_movie->clockStarted)
	{
		_movie->elapsedMs += realMs;
	}
	_movie->clockStarted = true;
	Poll();
	if (!_movie)
	{
		return; // the poll ended it: no draw, the 3D world drawn
	}

	_alpha = 1.0f; // every frame
	const int32_t frame = _movie->frame;
	if (frame >= _endFrame)
	{
		_alpha = 0.0f;
		Stop();
		return;
	}
	if (frame > _fadeStartFrame)
	{
		const bool paused = _hooks.isPaused && _hooks.isPaused();
		if (_previousPause != paused && _hooks.pauseGame)
		{
			_hooks.pauseGame(_previousPause);
		}
		_alpha = FadeAlpha(frame, _fadeStartFrame, _endFrame);
	}
	// opaque and no new frame -> the original waits up to 1000 x 0.5 ms for one. (approximate) not blocking: the
	// picture of the last frame decoded stays, the frames come at the same times
	// drawn with VertexColour over FullScreenRect: the renderer reads GetFrame()
	_framesReady = 0;
}

void VideoPlayer::Poll()
{
	for (;;)
	{
		if (!_movie)
		{
			return;
		}
		auto& movie = *_movie;
		if (movie.frame > _endFrame)
		{
			Stop();
			return;
		}
		// no open film, or the Bink clock says it is not time yet
		if (!movie.open || static_cast<uint32_t>(std::max(movie.frame, 0)) >=
		                       FramesDue(movie.elapsedMs, movie.file.FpsNumerator(), movie.file.FpsDenominator()))
		{
			break;
		}
		DecodeNextFrame();
		++_framesReady;
	}
	// only the last picture of the frames decoded is shown: its colours are made once, for it
	auto& movie = *_movie;
	if (movie.pictureToCopy)
	{
		movie.decoder->CopyPicture(_format, movie.framebuffer, movie.texture);
		movie.pictureToCopy = false;
	}
}

void VideoPlayer::DecodeNextFrame()
{
	auto& movie = *_movie;
	if (!movie.open)
	{
		return;
	}
	if (movie.frame > movie.totalFrames)
	{
		++movie.frame; // past the end: only counted
		return;
	}
	// the next frame decoded; its picture is copied to the 16-bit framebuffer and the textures (sampled by the renderer
	// as Expand) at the end of the Poll, if it is the last one decoded
	const auto size = movie.decoder->DecodeOnly(static_cast<uint32_t>(movie.frame));
	if (size >= movie.framebuffer.size() * 4 && !movie.framebuffer.empty())
	{
		movie.pictureToCopy = true;
		++movie.serial;
	}
	++movie.frame;
}

void VideoPlayer::Skip()
{
	if (_fallingSpell)
	{
		// the falling spell's film ends
		if (_hooks.endFallingSpellVideo)
		{
			_hooks.endFallingSpellVideo();
		}
		return;
	}
	if (!_movie)
	{
		return;
	}
	const int32_t frame = _movie->frame;
	if (frame > _fadeStartFrame)
	{
		Stop(); // already fading, it ends now
		return;
	}
	_fadeStartFrame = frame;
	_endFrame = std::min(frame + k_SkipFadeFrames, _movie->totalFrames); // signed comparison
	if (_hooks.pauseGame)
	{
		_hooks.pauseGame(_previousPause);
	}
}

bool VideoPlayer::EscapeKey(bool shift, bool ctrl)
{
	if (!_movie)
	{
		return false; // the key's other use
	}
	if (shift || ctrl)
	{
		return true;
	}
	if (_noSkip)
	{
		return true;
	}
	Skip();
	if (_hooks.stopScriptMusic)
	{
		_hooks.stopScriptMusic();
	}
	return true;
}

void VideoPlayer::Stop()
{
	if (_movie)
	{
		_movie.reset(); // closes the film
		_playing.store(false, std::memory_order_release);
		++_finished;
		_noSkip = false;
	}
	// the original also kills its poll timer: no timer here
}

void VideoPlayer::FinishedVideo()
{
	if (_hooks.pauseGame)
	{
		_hooks.pauseGame(_previousPause);
	}
	const int32_t wideScreen = _hooks.wideScreen ? _hooks.wideScreen() : 0;
	if (wideScreen != _previousWideScreen && _hooks.setWideScreen)
	{
		_hooks.setWideScreen(wideScreen == 0 ? 1 : 0); // toggled back
	}
	_noSkip = false;
	_finished = 0;
}

bool VideoPlayer::CoversScreen() const
{
	// a film, fully opaque, and not the falling spell's
	return _movie != nullptr && _alpha == 1.0f && !_fallingSpell;
}

std::optional<VideoPlayer::Frame> VideoPlayer::GetFrame() const
{
	if (!_movie)
	{
		return std::nullopt;
	}
	const auto& movie = *_movie;
	return Frame {
	    .rgba = movie.texture,
	    .rgb16 = movie.framebuffer,
	    .format = _format,
	    .width = movie.width,
	    .height = movie.height,
	    .alpha = _alpha,
	    .colour = VertexColour(_alpha, _fallingSpell),
	    .fallingSpell = _fallingSpell,
	    .frame = movie.frame,
	    .serial = movie.serial,
	};
}

int32_t VideoPlayer::Fps() const
{
	return _movie ? _movie->fps : 0;
}

int32_t VideoPlayer::TotalFrames() const
{
	return _movie ? _movie->totalFrames : 0;
}

int32_t VideoPlayer::CurrentFrame() const
{
	return _movie ? _movie->frame : 0;
}

VideoPlayer& video::Get()
{
	return Locator::videoSystem::value().Player();
}

bool video::IsPlaying()
{
	return Get().IsPlaying();
}
