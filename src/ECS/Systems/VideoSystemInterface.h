/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <chrono>
#include <filesystem>
#include <optional>
#include <span>

#include "Video/VideoRules.h"

namespace openblack::ecs::systems
{

/// The picture of the video playing, for the renderer
struct VideoPicture
{
	/// The decoded planes, Y full size and U, V half size each way, rows `stride` bytes apart. Empty while there is no
	/// picture (a build without the decoder, a video turned off): the picture is then black
	std::span<const uint8_t> y;
	std::span<const uint8_t> u;
	std::span<const uint8_t> v;
	uint32_t yStride {0};
	uint32_t chromaStride {0};
	uint32_t width {0};
	uint32_t height {0};
	/// Goes up with each new picture: upload when it changes
	uint32_t serial {0};
	/// The alpha it is drawn with, 0 to 255
	uint8_t alpha {0};
};

/// The game's full-screen videos: the intro and the falling spell's film. Playing one pauses the game and brings the
/// cinema bars in at once; it is paced by the real clock, never skipping a frame; it fades out at its end or when
/// Escape skips it, the game running again under the fade; when it ends the pause and the bars go back as they were.
class VideoSystemInterface
{
public:
	virtual ~VideoSystemInterface() = default;

	/// Plays a video, replacing any playing. A video that can't be opened (or while films are turned off) still exists
	/// for a frame and then ends, with nothing shown. True when the file opened
	virtual bool Play(const std::filesystem::path& path) = 0;
	/// The intro's schedule: a fade from 58 s, the end at 60 s. An intro whose file didn't open counts no frames, so it
	/// stays black and paused until Escape ends it, as the game does; with films turned off it ends at once instead
	virtual void ScheduleIntro() = 0;
	/// The falling spell's film: drawn faintly over the scene with no fade of its own, the world hidden behind it, its
	/// timeline of sounds and white fade running; nothing unless the player has a creature
	virtual void StartFallingSpell() = 0;
	/// Ends the falling spell, fading its film out as Escape would
	virtual void EndFallingSpell() = 0;
	/// Once a frame, with the real clock's time now
	virtual void Update(std::chrono::steady_clock::time_point now) = 0;
	/// The Escape key: true when a video took it
	virtual bool Escape(bool shift, bool ctrl) = 0;
	/// Skips the video as Escape does, without the key's checks
	virtual void Skip() = 0;
	/// Ends the video at once
	virtual void Stop() = 0;
	/// While set (a new player's first video), Escape does nothing; ending the video clears it
	virtual void SetNoSkip(bool noSkip) = 0;

	/// A video is playing. Safe to ask from any thread
	[[nodiscard]] virtual bool IsPlaying() const = 0;
	/// The video covers the screen: the world is not drawn under it
	[[nodiscard]] virtual bool CoversScreen() const = 0;
	/// The falling spell hides the world, its film over a clear screen
	[[nodiscard]] virtual bool HidesWorld() const = 0;
	/// The picture to draw this frame, none when nothing is to be drawn
	[[nodiscard]] virtual std::optional<VideoPicture> GetPicture() const = 0;

	/// Films can be turned off: they then play as videos that can't be opened
	virtual void SetFilmsEnabled(bool enabled) = 0;
	[[nodiscard]] virtual bool AreFilmsEnabled() const = 0;
	/// The picture as the game showed it, through a 16-bit copy that keeps 5 bits a channel (on), or at the decoder's
	/// full colour
	virtual void SetSixteenBitColour(bool on) = 0;
	[[nodiscard]] virtual bool IsSixteenBitColour() const = 0;

	/// For the debug window
	struct Status
	{
		int32_t frame {0};
		int32_t frameCount {0};
		int32_t fps {0};
		video::Schedule schedule;
		float alpha {0.0f};
		bool fallingSpell {false};
		int32_t fallingSpellState {0};
	};
	[[nodiscard]] virtual std::optional<Status> GetStatus() const = 0;
};

} // namespace openblack::ecs::systems
