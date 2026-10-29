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

#include "Visual/RenderModes.h"

/// How the heart of a temple shows: the model of the temple outside. Its light is always multiplied by white, which
/// takes one 256th off it. While the temple is destroyed, the heart glows in the colour of its player and fades out.
/// Free of state, so it is tested on its own.
namespace openblack::graphics::temple_heart_look
{

/// The colour that the light of the heart is multiplied by, 0xRRGGBB
inline constexpr uint32_t k_Colour = 0xFFFFFFu;

/// The alpha that the heart is drawn with, of 255, for the alpha of its colour: the light, of alpha 255, multiplied by
/// it in 256ths
[[nodiscard]] constexpr uint8_t DrawnAlpha(uint8_t colourAlpha) noexcept
{
	return static_cast<uint8_t>((255u * colourAlpha) >> 8u);
}

/// At this drawn alpha or below, the materials of a heart in its destruction change, and stay changed
inline constexpr uint8_t k_ChangedAt = 240;
/// The changed materials blend by the alpha of their texture and write no depth, from the front only
inline constexpr render_modes::Properties k_Changed {
    .additive = false,
    .writesDepth = false,
    .bothSides = false,
    .textureAlpha = true,
};
[[nodiscard]] constexpr bool MaterialsChange(bool destroying, uint8_t drawnAlpha) noexcept
{
	return destroying && drawnAlpha <= k_ChangedAt;
}

/// The snow level that a heart with changed materials shows, from the level of the snow where it stands and its drawn
/// alpha: none at an alpha of 128 or below, the full level at 256
[[nodiscard]] constexpr int32_t FadedSnowLevel(int32_t level, uint8_t drawnAlpha) noexcept
{
	constexpr int32_t k_Half = 0x80;
	constexpr int32_t k_Steps = 0x7F;
	constexpr int32_t k_Most = 0x7F80;
	const int32_t scaled = (static_cast<int32_t>(drawnAlpha) - k_Half) * level;
	const int32_t whole = scaled & ~k_Steps;
	if (whole <= 0)
	{
		return 0;
	}
	return whole >= k_Most ? 255 : scaled >> 7;
}

/// The heart glows in its destruction once its clock is past this many seconds
inline constexpr float k_GlowFrom = 3.5f;
inline constexpr float k_GlowOver = 3.0f;
/// The strength of the glow, of 256, grows from the low to the high strength as the glow starts, by the heartbeat
inline constexpr float k_GlowLow = 10.0f;
inline constexpr float k_GlowHigh = 60.0f;
[[nodiscard]] constexpr bool Glows(bool destroying, float clock) noexcept
{
	return destroying && clock > k_GlowFrom;
}
/// The strength of the glow for the clock and the pulse of its player's heartbeat (0 between beats, 1 at the top)
[[nodiscard]] uint8_t GlowStrength(float clock, float pulse) noexcept;
/// The colour, 0xRRGGBB, that the glow adds to the heart: each channel of its player's colour by the strength in 256ths
[[nodiscard]] constexpr uint32_t Glow(uint32_t playerColour, uint8_t strength) noexcept
{
	const uint32_t s = strength;
	return ((((playerColour & 0xFF0000u) * s) & 0xFF0000FFu) | (((playerColour & 0xFF00u) * s) & 0xFF0000u) |
	        (((playerColour & 0xFFu) * s) & 0xFF00u)) >>
	       8u;
}

/// Whether the heart is drawn: always, but in its destruction only until a moment before the temple goes
[[nodiscard]] bool Drawn(bool destroying, float clock) noexcept;

/// From this time in its destruction to its end, a shell of the creed's light in its player's colour covers the heart.
/// The creed's model itself is not drawn: only its glowing shell, centred on the heart.
inline constexpr float k_ShellFrom = 14.0f;
/// The shell's texture slides this fast across and up, in sheets a second of game time, and spans this much of it
inline constexpr float k_ShellSlideAcross = 0.6f;
inline constexpr float k_ShellSlideUp = 0.2f;
inline constexpr float k_ShellTextureAcross = 0.7f;
inline constexpr float k_ShellTextureUp = 0.1f;
/// The number of the shell's texture in the list of the glowing shells' textures (S_TileLandscape)
inline constexpr int k_ShellTexture = 6;
/// The colour that the shell's corners on the model add after the texture
inline constexpr uint32_t k_ShellSpecular = 0x141414u;
[[nodiscard]] constexpr bool ShellShown(bool destroying, float clock) noexcept
{
	return destroying && clock >= k_ShellFrom;
}
/// The shell's size, the same on each axis, at a time in the destruction: 0.3 until 20 seconds, then growing to 0.6 at
/// 22 seconds
[[nodiscard]] float ShellScale(float clock) noexcept;
/// How far the shell reaches out, in parts of the creed model's half diagonal, before the heartbeat's pulse multiplies
/// it: 5 until 20 seconds, then growing to 10 at 22 seconds
[[nodiscard]] float ShellReach(float clock) noexcept;

} // namespace openblack::graphics::temple_heart_look
