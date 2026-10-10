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

#include <array>
#include <chrono>
#include <optional>
#include <vector>

/// The rules of the game's full-screen videos (the intro and the falling spell's film): when a video fades and ends, how
/// strongly and where it is drawn, when its frames are due, and the falling spell's timeline. Free of any game state, so
/// that the video system and the tests share them.
///
/// A video's `frame` here is the number of frames decoded so far, which is also the next frame to decode.
namespace openblack::video
{

/// A video fades out over its last five seconds unless told otherwise
inline constexpr int32_t k_FadeSeconds = 5;
/// Escape fades a video out over 48 frames from where it is (two seconds at 24 frames a second)
inline constexpr int32_t k_SkipFadeFrames = 48;
/// The intro fades from 58 s and ends at 60 s, so its last 160 frames are never seen
inline constexpr int32_t k_IntroFadeStartSeconds = 58;
inline constexpr int32_t k_IntroEndSeconds = 60;
/// How opaque a video is at full strength: the falling spell's film is drawn faintly over the scene
inline constexpr uint8_t k_OpaqueStrength = 0xFF;
inline constexpr uint8_t k_FallingSpellStrength = 0x50;
/// The height of a 16:9 picture over its width
inline constexpr float k_SixteenByNine = 0.5625f;

/// From which frame a video fades and at which it ends
struct Schedule
{
	int32_t fadeStart {0};
	int32_t end {0};

	bool operator==(const Schedule&) const = default;
};

/// To its last frame, fading over the last five seconds of frames
[[nodiscard]] constexpr Schedule DefaultSchedule(int32_t frameCount, int32_t fps) noexcept
{
	return {.fadeStart = frameCount - fps * k_FadeSeconds, .end = frameCount};
}

/// The intro: a fade from 58 s, the end at 60 s
[[nodiscard]] constexpr Schedule IntroSchedule(int32_t fps) noexcept
{
	return {.fadeStart = fps * k_IntroFadeStartSeconds, .end = fps * k_IntroEndSeconds};
}

/// The falling spell's film has no fade of its own: it only fades when the spell ends
[[nodiscard]] constexpr Schedule WithoutFade(Schedule schedule) noexcept
{
	return {.fadeStart = schedule.end, .end = schedule.end};
}

/// Escape at `frame`: none when already fading, and the video ends at once; otherwise the fade starts now and lasts 48
/// frames, never past the file's last frame
[[nodiscard]] std::optional<Schedule> SkipSchedule(int32_t frame, Schedule schedule, int32_t frameCount) noexcept;

/// What a video does on a frame
enum class Phase : uint8_t
{
	Playing, ///< opaque
	Fading,  ///< fading out, the game running again under it
	Ended,   ///< gone, not drawn this frame
};

[[nodiscard]] constexpr Phase PhaseAt(int32_t frame, Schedule schedule) noexcept
{
	if (frame >= schedule.end)
	{
		return Phase::Ended;
	}
	return frame > schedule.fadeStart ? Phase::Fading : Phase::Playing;
}

/// How opaque a video is at `frame`: 1 until it fades, then 1 - (frame - start) / (|end - start| + 1) in single
/// precision; 0 once it has ended
[[nodiscard]] float VideoAlpha(int32_t frame, Schedule schedule) noexcept;

/// The alpha the picture is drawn with, 0 to 255: its full strength times the video's alpha, rounded down
[[nodiscard]] uint8_t DrawAlpha(float alpha, bool fallingSpell) noexcept;

/// Where on the screen a video is drawn, in pixels
struct ScreenRect
{
	int32_t x {0};
	int32_t y {0};
	int32_t width {0};
	int32_t height {0};

	bool operator==(const ScreenRect&) const = default;
};

/// The screen between bars as tall as a 16:9 picture leaves, one pixel larger each way. The bars aren't clamped: on a
/// screen wider than 16:9 they are negative and the picture runs over the top and bottom of the screen
[[nodiscard]] ScreenRect LetterboxRect(int32_t screenWidth, int32_t screenHeight) noexcept;

/// How many frames are due `elapsed` after a video's first frame: frame i is due at i / fps seconds, the file's exact
/// fraction
[[nodiscard]] uint32_t FramesDue(std::chrono::microseconds elapsed, uint32_t fpsNumerator, uint32_t fpsDenominator) noexcept;

/// The falling spell's time into its film: frame * 1000 / fps in 32-bit integers, wrapping as the game's do
[[nodiscard]] int32_t FilmMilliseconds(int32_t frame, int32_t fps) noexcept;

/// The sounds and events of the falling spell's timeline, in the order the game makes them
enum class FallingSpellCue : uint8_t
{
	Rumble,            ///< the screen rumble
	LaserExplode,      ///< a laser beam's explosion
	VolcanoStop,       ///< the volcano's rumble stops
	Creed,             ///< the chant
	CitadelExplode,    ///< the citadel's explosion
	Volcano,           ///< the volcano's rumble
	HealChakra,        ///< the healing chakra
	CreedHigh,         ///< the chant again, higher
	CreedStop,         ///< the chant stops
	CreedHighStop,     ///< the higher chant stops
	MusicFadeOut,      ///< all music fades out
	WhiteFadeStart,    ///< the screen fades to white
	CitadelExplodeEnd, ///< the citadel's explosion again
	WhiteFadeBack,     ///< the screen fades back from white
};

/// The falling spell's timeline: two counters moved on by the film's time, each step firing its cues. The last step
/// waits for the white fade to reach white; the spell ends at state 4
class FallingSpellTimeline
{
public:
	static constexpr int32_t k_EndState = 4;

	/// Moves on to `filmMs`, appending the cues fired to `cues`. `whiteReached`: the white fade has reached white
	void Advance(int32_t filmMs, bool whiteReached, std::vector<FallingSpellCue>& cues);
	/// Without a film the state just counts up each frame
	void AdvanceWithoutFilm() noexcept { ++_state; }

	[[nodiscard]] int32_t State() const noexcept { return _state; }
	[[nodiscard]] int32_t SoundState() const noexcept { return _soundState; }
	[[nodiscard]] bool HasEnded() const noexcept { return _state >= k_EndState; }

private:
	int32_t _state {0};
	int32_t _soundState {0};
};

} // namespace openblack::video
