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

#include <algorithm>
#include <optional>

#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

/// The byte arithmetic of the engine's colours, CPU side; the GPU twin is assets/shaders/argb_colour.sh and the two
/// must stay the same. Wiki: docs/bw1-notes/rendering-objects.md, colour arithmetic.
///
/// A colour is a D3DCOLOR, 0xAARRGGBB. The engine combines two of them in two families, and every routine of both
/// truncates, none rounds:
/// - (c t) >> 8 per channel: a multiply over the masked byte and the bits of the byte kept (the land colour tint, the
///   object and mesh part colours, the trees, the model light). With t = 0xFF a channel loses 1, so 0xFF stays at 0xFE.
/// - c l / 255 per channel: a multiply-and-shift division by 255, which is trunc(x / 255) for every x in 0..65025 (the
///   primitive colours, the mists, the creature body).
/// Each routine has its own alpha rule, so the name says it: the Argb ones work the alpha like the other channels, the
/// KeepAlpha ones keep the FIRST argument's alpha (that is what every original does: the object colour and specular keep
/// the object's, the mist the colour's and not the light's, the trees the object's), and the Opaque one writes 0xFF.
///
/// ToAbgr / ToVec4 / ToVec3 have no original behind them: bgfx wants the bytes as ABGR (R first in memory) and the
/// shaders want 0..1 floats.
namespace openblack::argb_colour
{

[[nodiscard]] constexpr uint32_t Alpha(uint32_t argb) noexcept
{
	return argb >> 24;
}
[[nodiscard]] constexpr uint32_t Red(uint32_t argb) noexcept
{
	return (argb >> 16) & 0xFFu;
}
[[nodiscard]] constexpr uint32_t Green(uint32_t argb) noexcept
{
	return (argb >> 8) & 0xFFu;
}
[[nodiscard]] constexpr uint32_t Blue(uint32_t argb) noexcept
{
	return argb & 0xFFu;
}
/// 0xAARRGGBB from four 0..255 bytes (the bits above the byte are dropped)
[[nodiscard]] constexpr uint32_t Argb(uint32_t red, uint32_t green, uint32_t blue, uint32_t alpha = 0) noexcept
{
	return (alpha & 0xFFu) << 24 | (red & 0xFFu) << 16 | (green & 0xFFu) << 8 | (blue & 0xFFu);
}

/// The diffuse of the draw with the land colour (where the PSys property DrawWithLandscapeColor sends a particle mesh,
/// inferred): (c t) >> 8 in the four channels, the alpha too (((c >> 8) & 0xFF0000) t.A, masked with 0xFF00FFFF), so a
/// 0xFF alpha tinted with 0xFF comes out 0xFE. The same inline in the field and spell wolf draws (the colour times the
/// charring grey).
[[nodiscard]] constexpr uint32_t MultiplyArgbShift8(uint32_t c, uint32_t t) noexcept
{
	return Argb((Red(c) * Red(t)) >> 8, (Green(c) * Green(t)) >> 8, (Blue(c) * Blue(t)) >> 8, (Alpha(c) * Alpha(t)) >> 8);
}

/// The specular of the same draw: min(a + b, 255) in the four channels, the alpha too
[[nodiscard]] constexpr uint32_t AddArgbSaturated(uint32_t a, uint32_t b) noexcept
{
	return Argb(std::min(Red(a) + Red(b), 0xFFu), std::min(Green(a) + Green(b), 0xFFu), std::min(Blue(a) + Blue(b), 0xFFu),
	            std::min(Alpha(a) + Alpha(b), 0xFFu));
}

/// The object's colour times the mesh part's: (a b) >> 8 in R, G and B, the alpha of `a`
[[nodiscard]] constexpr uint32_t MultiplyRgbShift8KeepAlpha(uint32_t a, uint32_t b) noexcept
{
	return Argb((Red(a) * Red(b)) >> 8, (Green(a) * Green(b)) >> 8, (Blue(a) * Blue(b)) >> 8, Alpha(a));
}

/// The scalar form of the same product: (c k) >> 8 in R, G and B through the masks 0xFF0000FF / 0xFF0000 / 0xFF00 and
/// a shift by 8, the alpha of `c` kept. The model light (k = the factor f), the tree draw (k = the tree brightness) and
/// the burning tree (k = the burning grey). The masks come after each multiply, so the channels never run into each
/// other: each is ((c k) >> 8) & 0xFF modulo 2^32 for any k, what Argb's masks give here. The callers pass 0..255 (the
/// brightness, the grey, the light factor <= 254): a fact of the callers, not a condition of this function.
[[nodiscard]] constexpr uint32_t ScaleRgbShift8KeepAlpha(uint32_t c, uint32_t k) noexcept
{
	return Argb((Red(c) * k) >> 8, (Green(c) * k) >> 8, (Blue(c) * k) >> 8, Alpha(c));
}

/// The object's specular plus the mesh part's: min(a + b, 255) in R, G and B, the alpha of `a`
[[nodiscard]] constexpr uint32_t AddRgbSaturatedKeepAlpha(uint32_t a, uint32_t b) noexcept
{
	return Argb(std::min(Red(a) + Red(b), 0xFFu), std::min(Green(a) + Green(b), 0xFFu), std::min(Blue(a) + Blue(b), 0xFFu),
	            Alpha(a));
}

/// One byte times another over 255, truncated: the cloud's edge alpha times the alignment colour's alpha, and every
/// MultiplyBytes below
[[nodiscard]] constexpr uint32_t MultiplyBytes(uint32_t a, uint32_t b) noexcept
{
	return a * b / 255u;
}

/// A primitive's colour product: trunc(a b / 255) in the four channels, the alpha too
[[nodiscard]] constexpr uint32_t MultiplyArgb(uint32_t a, uint32_t b) noexcept
{
	return Argb(MultiplyBytes(Red(a), Red(b)), MultiplyBytes(Green(a), Green(b)), MultiplyBytes(Blue(a), Blue(b)),
	            MultiplyBytes(Alpha(a), Alpha(b)));
}

/// The mist's colour times the land light after the haze, trunc(c l / 255) in R, G and B; the alpha is the colour's,
/// not the light's
[[nodiscard]] constexpr uint32_t MultiplyRgbKeepAlpha(uint32_t colour, uint32_t light) noexcept
{
	return Argb(MultiplyBytes(Red(colour), Red(light)), MultiplyBytes(Green(colour), Green(light)),
	            MultiplyBytes(Blue(colour), Blue(light)), Alpha(colour));
}

/// The creature draw: the body's colour times the object's, trunc(a b / 255) in R, G and B, the alpha forced to 0xFF
[[nodiscard]] constexpr uint32_t MultiplyRgbOpaque(uint32_t a, uint32_t b) noexcept
{
	return Argb(MultiplyBytes(Red(a), Red(b)), MultiplyBytes(Green(a), Green(b)), MultiplyBytes(Blue(a), Blue(b)), 0xFFu);
}

/// bgfx's packed colour (no original): 0xAARRGGBB to 0xAABBGGRR, R and B swapped
[[nodiscard]] constexpr uint32_t ToAbgr(uint32_t argb) noexcept
{
	return (argb & 0xFF00FF00u) | ((argb >> 16) & 0xFFu) | ((argb & 0xFFu) << 16);
}
/// The same with the alpha replaced by a 0..255 byte
[[nodiscard]] constexpr uint32_t ToAbgr(uint32_t argb, uint32_t alpha) noexcept
{
	return (ToAbgr(argb) & 0x00FFFFFFu) | (alpha & 0xFFu) << 24;
}
/// The same from 0..1 floats, each clamped and rounded to the nearest byte (halves up)
[[nodiscard]] inline uint32_t ToAbgr(const glm::vec4& rgba) noexcept
{
	const auto byte = [](float v) { return static_cast<uint32_t>(std::clamp(v, 0.0f, 1.0f) * 255.0f + 0.5f); };
	return byte(rgba.a) << 24 | byte(rgba.b) << 16 | byte(rgba.g) << 8 | byte(rgba.r);
}

/// The colour as 0..1 floats for a uniform (no original): r, g, b, a = byte / 255
[[nodiscard]] inline glm::vec4 ToVec4(uint32_t argb) noexcept
{
	return glm::vec4(static_cast<float>(Red(argb)), static_cast<float>(Green(argb)), static_cast<float>(Blue(argb)),
	                 static_cast<float>(Alpha(argb))) /
	       255.0f;
}
/// The RGB of ToVec4
[[nodiscard]] inline glm::vec3 ToVec3(uint32_t argb) noexcept
{
	return glm::vec3(static_cast<float>(Red(argb)), static_cast<float>(Green(argb)), static_cast<float>(Blue(argb))) / 255.0f;
}

/// The colour fields of an object on their way to vs_object, in the fifth column of its instance (i_data4). No
/// original: the engine keeps them in the 3D object and lights on the CPU; openblack carries them per instance, one
/// float each, every one an integer of at most 2^24 that the float holds exactly:
/// - x, the object's colour. 0: the land light alone (buildings, animals, ...). Negative, PackInstanceTint: -1 - the
///   rgb of the tint t that multiplies the land light (the draw with the land colour, or the building draw; or the
///   tree draw's own product, see w). Positive, PackInstanceColour: 1 + the rgb of a colour set explicitly instead of
///   the land light (the power-up bands, the PSys mesh atoms).
/// - y, PackInstanceSpecular: the specular's rgb, 8 bits a channel, added to the land light's with saturation or, with
///   an explicit colour, the specular itself.
/// - z, PackInstanceWindow: 0, or 1 + the rgb of the window colour the abode draw sets (0 when the windows are not
///   lit).
/// - w, PackInstanceTreeTint: 1 = the tint goes after the haze. The tree draw does not use the land colour tint: it
///   lights, hazes and only then multiplies the hazed colour by the brightness or by the burning colour. 0 for every
///   other tint (before the haze).
/// The alpha bytes are not carried. The object draw copies the whole colour (and the specular) into a global for every
/// object, and that global is read by several render routines. (inferred) its alpha byte only matters for the fading
/// objects: those readers were not followed to the vertex colour. openblack gives a fading object its alpha in
/// components::Alpha (approximate: the original's is (0xFF x t.A) >> 8, so 0xFE for the physical shield's white tint
/// and (A x 0xFF) >> 8 for a spell wolf's; the one-shot orb's caller already does that product by hand).
/// vs_object.sc decodes them (UnpackRgb24, argb_colour.sh); the Instance* readers below are its CPU twin.
[[nodiscard]] constexpr float PackRgb24(uint32_t argb) noexcept
{
	return static_cast<float>(argb & 0x00FFFFFFu);
}
/// The land colour tint t: vs_object multiplies the land light by it, (c t) >> 8 per channel
inline void PackInstanceTint(glm::vec4& lh3d, uint32_t tint) noexcept
{
	lh3d.x = -1.0f - PackRgb24(tint);
}
/// The tree draw's tint (the brightness, or the burning colour): the same (c t) >> 8, after the haze
inline void PackInstanceTreeTint(glm::vec4& lh3d, uint32_t tint) noexcept
{
	lh3d.x = -1.0f - PackRgb24(tint);
	lh3d.w = 1.0f;
}
/// An explicitly set colour: drawn instead of the land light, without the haze
inline void PackInstanceColour(glm::vec4& lh3d, uint32_t colour) noexcept
{
	lh3d.x = 1.0f + PackRgb24(colour);
}
/// The specular set with an explicit colour, or the specular the land colour draw adds
inline void PackInstanceSpecular(glm::vec4& lh3d, uint32_t specular) noexcept
{
	lh3d.y = PackRgb24(specular);
}
/// The window colour: 0 = the windows are not lit
inline void PackInstanceWindow(glm::vec4& lh3d, uint32_t window) noexcept
{
	lh3d.z = window == 0 ? 0.0f : 1.0f + PackRgb24(window);
}

/// What vs_object reads back, the rgb (alpha 0): nullopt when the slot is empty or holds the other kind
[[nodiscard]] inline std::optional<uint32_t> InstanceTint(const glm::vec4& lh3d) noexcept
{
	return lh3d.x < -0.5f ? std::optional(static_cast<uint32_t>(-lh3d.x - 1.0f)) : std::nullopt;
}
[[nodiscard]] inline bool InstanceTintAfterHaze(const glm::vec4& lh3d) noexcept
{
	return lh3d.w > 0.5f;
}
[[nodiscard]] inline std::optional<uint32_t> InstanceColour(const glm::vec4& lh3d) noexcept
{
	return lh3d.x > 0.5f ? std::optional(static_cast<uint32_t>(lh3d.x - 1.0f)) : std::nullopt;
}
[[nodiscard]] inline uint32_t InstanceSpecular(const glm::vec4& lh3d) noexcept
{
	return static_cast<uint32_t>(lh3d.y);
}
[[nodiscard]] inline std::optional<uint32_t> InstanceWindow(const glm::vec4& lh3d) noexcept
{
	return lh3d.z > 0.5f ? std::optional(static_cast<uint32_t>(lh3d.z - 1.0f)) : std::nullopt;
}

} // namespace openblack::argb_colour
