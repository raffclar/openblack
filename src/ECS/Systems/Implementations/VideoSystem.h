/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <optional>
#include <vector>

#include <BinkDecoder.h>
#include <BinkFile.h>

#include "ECS/Systems/VideoSystemInterface.h"
#include "Video/FrameQueue.h"
#include "Video/VideoRules.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs::systems
{

class VideoSystem final: public VideoSystemInterface
{
public:
	/// What the video system asks of the rest of the game; the tests give fakes
	struct Hooks
	{
		std::function<bool()> isPaused;
		std::function<void(bool paused)> setPaused;
		std::function<bool()> isWideScreenOn;
		/// The game's own cinema bars, in or out
		std::function<void(bool on)> setWideScreen;
		/// The bars all the way in at once
		std::function<void()> snapWideScreen;
		/// Lets the scripts' music go, as Escape does
		std::function<void()> releaseScriptMusic;
		/// A video's file through the resource cache; none when it can't be read
		std::function<std::shared_ptr<const bink::BinkFile>(const std::filesystem::path& path)> openFile;
		/// Lets the resource cache drop a video that has ended
		std::function<void(const std::filesystem::path& path)> closeFile;
		/// Whether the local player has a creature
		std::function<bool()> playerHasCreature;
		/// The falling spell's film
		std::function<std::filesystem::path()> fallingSpellPath;
		/// The falling spell's sounds, music and white fade
		std::function<void(video::FallingSpellCue cue)> fallingSpellCue;
		/// The white fade the falling spell started has reached white
		std::function<bool()> whiteFadeReached;
	};

	/// The game's: the time system's pause, the cinematic director's bars, the scripts' music, the resource cache
	VideoSystem();
	/// Its frames are decoded on a thread of their own unless `mode` says otherwise
	explicit VideoSystem(Hooks hooks, video::DecodeMode mode = video::DecodeMode::Worker);

	bool Play(const std::filesystem::path& path) override;
	void ScheduleIntro() override;
	void StartFallingSpell() override;
	void EndFallingSpell() override;
	void Update(std::chrono::steady_clock::time_point now) override;
	bool Escape(bool shift, bool ctrl) override;
	void Skip() override;
	void Stop() override;
	void SetNoSkip(bool noSkip) override { _noSkip = noSkip; }

	[[nodiscard]] bool IsPlaying() const override { return _playing.load(std::memory_order_acquire); }
	[[nodiscard]] bool CoversScreen() const override;
	[[nodiscard]] bool HidesWorld() const override { return _fallingSpell; }
	[[nodiscard]] std::optional<VideoPicture> GetPicture() const override;

	void SetFilmsEnabled(bool enabled) override { _filmsEnabled = enabled; }
	[[nodiscard]] bool AreFilmsEnabled() const override { return _filmsEnabled; }
	void SetSixteenBitColour(bool on) override { _sixteenBitColour = on; }
	[[nodiscard]] bool IsSixteenBitColour() const override { return _sixteenBitColour; }

	[[nodiscard]] std::optional<Status> GetStatus() const override;

private:
	struct Video
	{
		std::filesystem::path path;
		std::shared_ptr<const bink::BinkFile> file;
		/// Its frames, decoded ahead; none when they can't be decoded
		std::unique_ptr<video::FrameQueue> frames;
		/// The frame shown, held by the queue until the next update
		const video::DecodedFrame* shown {nullptr};
		/// Whole frames a second, for the schedule; 1 when the file didn't open
		int32_t fps {1};
		int32_t frameCount {0};
		/// The frames decoded so far: the next to decode
		int32_t frame {0};
		/// When its first frame was due
		std::optional<std::chrono::steady_clock::time_point> start;
	};

	/// Counts the frames due by the real clock and takes the newest of them decoded
	void DecodeDue(std::chrono::steady_clock::time_point now);
	/// The video goes; the next update puts the pause and the bars back
	void Delete();
	/// The update after a video ended: the pause and the bars as they were before it
	void Finished();
	void UpdateFallingSpell();

	Hooks _hooks;
	video::DecodeMode _decodeMode;
	std::optional<Video> _video;
	std::atomic<bool> _playing {false};
	video::Schedule _schedule;
	float _alpha {0.0f};
	bool _isIntro {false};
	bool _finishedPending {false};
	bool _previousPause {false};
	bool _previousWideScreen {false};
	bool _noSkip {false};
	bool _filmsEnabled {true};
	bool _sixteenBitColour {true};
	bool _fallingSpell {false};
	video::FallingSpellTimeline _timeline;
	std::vector<video::FallingSpellCue> _cues;
};

} // namespace openblack::ecs::systems
