/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cmath>
#include <cstdint>

#include <algorithm>

/// When the player takes the camera back from a miracle's camera path. The path keeps the camera until a key moving it
/// left, right, forwards or backwards would move it this frame, or the hand grips the land. A movement key moves the
/// camera 400 units a second of the camera's frame time, and only a step of at least a whole unit counts: the camera's
/// frame time is the frame's whole milliseconds as seconds, at most a tenth of a second, so a key held through a frame
/// of 2 ms or less doesn't take the camera back. Rotating, tilting and zooming never do.
namespace openblack::camera_path
{

/// The camera's frame time is no longer than this, in seconds
inline constexpr float k_MaxFrameSeconds = 0.1f;
/// Seconds in a millisecond, as the game's single precision constant
inline constexpr float k_SecondsPerMillisecond = 0.001f;
/// How far a movement key moves the camera in a second of its frame time
inline constexpr float k_KeyStepPerSecond = 400.0f;

/// The camera's frame time for a frame of whole milliseconds
[[nodiscard]] constexpr float FrameSeconds(uint32_t frameMilliseconds)
{
	return std::min(static_cast<float>(frameMilliseconds) * k_SecondsPerMillisecond, k_MaxFrameSeconds);
}

/// Whether a movement key held this frame would move the camera at least a whole unit
[[nodiscard]] constexpr bool KeyStepMoves(uint32_t frameMilliseconds)
{
	return static_cast<int32_t>(FrameSeconds(frameMilliseconds) * k_KeyStepPerSecond) != 0;
}

/// Whether the player takes the camera back: a movement key that would move it, or the hand gripping the land
[[nodiscard]] constexpr bool TakesCameraBack(bool movementKey, uint32_t frameMilliseconds, bool grippingLand)
{
	return (movementKey && KeyStepMoves(frameMilliseconds)) || grippingLand;
}

/// One play of the animation that a miracle's camera path goes with is this many frames
inline constexpr int32_t k_AnimationFrames = 1000;
/// A looped animation draws up to this many steps past its last step
inline constexpr float k_MaxLoopedFraction = 5.0f;

/// How many of the animation's frames go by in a second: one play takes the animation's cycle time, shortened by the
/// speed-up. Each step rounds to single precision, in this order.
[[nodiscard]] inline float FrameRate(int32_t cycleMilliseconds, float speedUp)
{
	const float playsPerMillisecond = 1000.0f / static_cast<float>(cycleMilliseconds);
	const float sped = playsPerMillisecond * speedUp;
	return sped * 1000.0f;
}

/// The whole frame that a draw shows, between the frames of the last two steps by the fraction of the step that has
/// gone by. A frame that does not loop stays in the first to the last frame; a looped frame goes round the frames. The
/// part after the point is cut off.
[[nodiscard]] inline int32_t DrawnFrame(float previous, float current, float fraction, int32_t frames, bool loop)
{
	const float limit = loop ? k_MaxLoopedFraction : 1.0f;
	float share = 0.0f;
	if (fraction > 0.0f)
	{
		share = fraction < limit ? fraction : limit;
	}
	const float frame = (current - previous) * share + previous;
	if (loop)
	{
		const auto count = static_cast<float>(frames);
		float wrapped = std::fmod(frame, count);
		if (wrapped < 0.0f)
		{
			wrapped += count;
		}
		return static_cast<int32_t>(wrapped);
	}
	const auto whole = static_cast<int32_t>(frame);
	const int32_t last = frames - 1;
	if (whole < 1)
	{
		return 0;
	}
	return whole < last ? whole : last;
}

/// The time on the path, in whole milliseconds, of a frame of the animation: the cycle time over a thousand frames,
/// cut off towards zero
[[nodiscard]] constexpr int32_t PathMilliseconds(int32_t cycleMilliseconds, int32_t frame)
{
	return (cycleMilliseconds * frame) / k_AnimationFrames;
}

/// Whether a drawn frame ends the path: the animation has come to its last frame
[[nodiscard]] constexpr bool EndsPath(int32_t drawnFrame, int32_t frames)
{
	return drawnFrame >= frames - 1;
}

} // namespace openblack::camera_path
