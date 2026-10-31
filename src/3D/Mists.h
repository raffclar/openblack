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

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

/// Black & White's puffs of mist and cloud: their animation, a counter that runs to 900 picking one of sixteen frames of
/// the smoke texture of 8 by 8 frames, and their shape.
namespace openblack::mists
{

inline constexpr int k_CounterWrap = 900;
/// The counter moves this much each millisecond of game time
inline constexpr float k_Rate = 0.255f;

/// The counter's frame: a twentieth of it, in sixteen frames. The counter reaches 900 itself, so the frames run 0 to 15
/// twice, then 0 to 14, before starting again.
[[nodiscard]] constexpr int Frame(int counter) noexcept
{
	return (counter * 45 / k_CounterWrap) & 15;
}

/// Where a frame is in the texture: a row of eight frames to each eighth. The map's mists use its first two rows, and
/// the clouds and the mists that shrink edge on the next two.
[[nodiscard]] constexpr glm::vec2 FrameOffset(int frame, bool lowerRows) noexcept
{
	return {static_cast<float>(frame & 7) * 0.125f,
	        (static_cast<float>((frame >> 3) & 7) * 0.125f) + (lowerRows ? 0.25f : 0.0f)};
}

/// A new mist starts at a random counter of the first sixteen, from a random number between 0 and 16
[[nodiscard]] constexpr int StartCounter(float random0To16) noexcept
{
	return static_cast<int>(random0To16) & 15;
}

/// The size of a mist that shrinks edge on, seen along `toMist` from the camera: its full size seen from straight above
/// or below, `edgeShrink` times smaller seen level
[[nodiscard]] float EdgeOnSize(float size, float edgeShrink, const glm::vec3& toMist);

/// Moves the counter on by the milliseconds of game time. The game drops the fraction each frame; it is kept here, so
/// the animation runs at the same pace however fast the frames come.
void Advance(int& counter, float& remainder, float milliseconds) noexcept;

/// The colour, 0xAARRGGBB, of a mist that a script makes: each channel the whole part of its number (0 to 255), cut to
/// its low byte, and the alpha the whole part of 255 times the transparency
[[nodiscard]] uint32_t ScriptColour(float red, float green, float blue, float transparency) noexcept;

/// The game turns in each second of a fade
inline constexpr float k_FadeTurnsPerSecond = 10.0f;

/// A script's fade of a mist: its size and its alpha (0 to 255) go in equal steps, one step each game turn, from a
/// start to an end
struct Fade
{
	/// How many turns the fade lasts
	float turns;
	/// How many turns of it have gone
	float elapsed;
	/// What each turn adds to the size and to the alpha
	float sizeStep;
	float alphaStep;
	/// The alpha now, before it is cut to a whole number
	float alpha;
};

/// The fade from `startSize` to `endSize` and from the alpha `startAlpha` to `endAlpha` over `seconds`. The mist takes
/// the start size and the start alpha at once.
[[nodiscard]] Fade StartFade(float startSize, float endSize, float startAlpha, float endAlpha, float seconds) noexcept;

/// The alpha byte of a fade's alpha: its whole part
[[nodiscard]] uint8_t AlphaByte(float alpha) noexcept;

/// The whole alpha that a fade gives to the object of a land mist: the whole part of the alpha, not cut to a byte
[[nodiscard]] int32_t WholeAlpha(float alpha) noexcept;

/// The bits of a whole alpha above its low byte
[[nodiscard]] constexpr int32_t AlphaHighBits(int32_t wholeAlpha) noexcept
{
	return wholeAlpha >> 8;
}

/// The whole alpha of a land mist from its colour, whose alpha byte is the low byte, and the bits above that byte
[[nodiscard]] constexpr int32_t WholeAlphaOf(uint32_t colour, int32_t alphaHighBits) noexcept
{
	return static_cast<int32_t>((static_cast<uint32_t>(alphaHighBits) << 8u) | (colour >> 24u));
}

/// The alpha that a land mist draws with. It is 255 times the whole alpha of its object, divided by 256 and cut to the
/// low byte. Thus a whole alpha from 1 to 255 draws 1 less, and 256 draws 255.
[[nodiscard]] constexpr uint8_t DrawnAlpha(int32_t wholeAlpha) noexcept
{
	return static_cast<uint8_t>(((static_cast<uint32_t>(wholeAlpha) * 255u) >> 8u) & 0xFFu);
}

/// One game turn of a fade: when a turn of it is left, it adds a step to the size and to the alpha, keeps the alpha in 0
/// to 255 and gives true. When no turn is left, it changes nothing and gives false: the fade is done.
[[nodiscard]] bool StepFade(Fade& fade, float& size) noexcept;

} // namespace openblack::mists
