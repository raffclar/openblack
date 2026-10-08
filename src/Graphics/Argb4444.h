/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <algorithm>
#include <array>
#include <span>
#include <string_view>
#include <vector>

/// The cut of the original's 8-bit .raw textures to ARGB4444, CPU only. Wiki: docs/bw1-notes/rendering.md, ARGB4444
/// textures.
///
/// The original keeps every texture created with the alpha flag 0x40 as a 16-bit ARGB4444 surface: the loader drops
/// the low nibble of each byte of x.raw and of its xa.raw and D3D7 expands the nibble back to 8 bits when it samples
/// (n * 17, inferred: done by the driver, not by the game).
/// There is no shader twin on purpose: the original filters nibbles that are already cut, so the cut has to happen
/// when the texture is loaded (Texture2DLoader) and not after the linear filtering of a fragment shader.
namespace openblack::graphics::argb4444
{

/// The nibble of an 8-bit value: v >> 4. The loader takes B = byte[2] >> 4, G = byte[1] & 0xF0,
/// R = (byte[0] & 0xF0) << 4, A = byte & 0xF0, and the blob shadow likewise. All of them keep the top 4 bits, none
/// rounds.
[[nodiscard]] constexpr uint8_t Quantize(uint8_t v) noexcept
{
	return static_cast<uint8_t>(v >> 4);
}

/// A nibble back to 8 bits as D3D7 samples an ARGB4444 surface: n * 17 = (n << 4) | n, 15 -> 255 (inferred: the
/// expansion is done by the driver)
[[nodiscard]] constexpr uint8_t Expand(uint8_t n) noexcept
{
	return static_cast<uint8_t>((n & 0xFu) * 17u);
}

/// What the original samples for an 8-bit .raw byte: Expand(Quantize(v)) = (v & 0xF0) | (v >> 4) (the loader + D3D)
[[nodiscard]] constexpr uint8_t Cut(uint8_t v) noexcept
{
	return Expand(Quantize(v));
}

/// An ARGB4444 texel to 8-bit R, G, B, A (this order, as RGBA8 in memory), each Expand-ed. The original's texture
/// format 0 is the D3D pixel format with the masks A 0xF000, R 0x0F00, G 0x00F0, B 0x000F.
[[nodiscard]] constexpr std::array<uint8_t, 4> Unpack(uint16_t texel) noexcept
{
	return {Expand(static_cast<uint8_t>((texel >> 8) & 0xFu)), Expand(static_cast<uint8_t>((texel >> 4) & 0xFu)),
	        Expand(static_cast<uint8_t>(texel & 0xFu)), Expand(static_cast<uint8_t>(texel >> 12))};
}

/// The ARGB4444 texel the loader writes: R, G, B nibbles from the colour (alpha 0) and the alpha nibble OR-ed in
[[nodiscard]] constexpr uint16_t Pack(uint8_t r, uint8_t g, uint8_t b, uint8_t a) noexcept
{
	return static_cast<uint16_t>(Quantize(a) << 12 | Quantize(r) << 8 | Quantize(g) << 4 | Quantize(b));
}

/// The pair x.raw (R, G, B bytes) + xa.raw (one byte per pixel) as the loader packs it, returned as the RGBA8 the
/// original samples (Cut in the four channels). An empty `alpha` is a missing xa.raw: its load failed, the loader only
/// reports it and carries on with the buffer that still holds the colour file, so pixel i takes the alpha (colour
/// stream byte i) & 0xF0 (R0, G0, B0, R1..). A shorter xa.raw is not an error either: the file read takes
/// min(length, 0x10000) over the same buffer, so the pixels past its end keep the colour stream; a longer one is
/// truncated.
[[nodiscard]] inline std::vector<uint8_t> PackRaw(std::span<const uint8_t> rgb, std::span<const uint8_t> alpha)
{
	const size_t pixels = rgb.size() / 3;
	std::vector<uint8_t> rgba(pixels * 4);
	for (size_t i = 0; i < pixels; ++i)
	{
		rgba[i * 4 + 0] = Cut(rgb[i * 3 + 0]);
		rgba[i * 4 + 1] = Cut(rgb[i * 3 + 1]);
		rgba[i * 4 + 2] = Cut(rgb[i * 3 + 2]);
		rgba[i * 4 + 3] = Cut(i < alpha.size() ? alpha[i] : rgb[i]);
	}
	return rgba;
}

/// The lower-case base names (no ".raw") of the colour textures the original creates with the alpha flag 0x40, that
/// is with create flags 0x41: the flag reaches the loader, and with it the 4444 branch runs (without it the 565 / 555
/// branch).
inline constexpr auto k_AlphaFlagStems = std::to_array<std::string_view>({
    "atmos",
    "blobs", // Data\blobs
    "burn",
    "c_ape_hair", // Data\C_Ape_Hair
    "choosesymbol",
    "cool_effect",
    "envmap",
    "envmap_glass_fx",
    "envmap_glass_fx2_inv",
    "envmap_glass_fx_inv",
    "forcefield",
    "front_end_buttons",
    "gatheringtext",
    "icons",
    "leash",
    "misc0",
    "mousehelp",
    "originalchoosesymbol",
    "p4",
    "p4t",
    "picturetexture",
    "pin",
    "playerssymbols",
    "rainbow",
    "s_beam",                      // spell files (*)
    "s_fire",                      // GlobalTextures (*)
    "s_gesture0",                  // GlobalTextures (*)
    "s_gesture1",                  // GlobalTextures (*)
    "s_hand_flow",                 // GlobalTextures (*)
    "s_iceenvmap",                 // (*)
    "s_iceenvmapcolor",            // (*), not in Data
    "s_iceenvmapgrey",             // (*), 194823 bytes: not a valid .raw
    "s_lightning",                 // spell files (*)
    "s_lightsheetstars",           // (*)
    "s_spangle_a",                 // spell files (*) (inferred)
    "s_spritesheet1",              // GlobalTextures (*)
    "s_spritesheet2",              // GlobalTextures (*)
    "s_spritesheet3",              // GlobalTextures (*)
    "s_static",                    // GlobalTextures (*)
    "s_teleport_stars",            // (*)
    "s_teleport_vortex_texture",   // spell files (*)
    "s_teleport_vortex_texture01", // spell files (*)
    "s_tilelandscape",             // (*)
    "s_volcano_base",              // the landscape vortex (*)
    "s_volcano_base_alpha",        // the landscape vortex (*)
    "s_volcano_fire",              // spell files (*)
    "s_volcano_rock",              // spell files (*)
    "s_vollight",                  // (*)
    "s_vollight2",                 // (*)
    "s_vollight3",                 // (*)
    "s_vollight4",                 // (*)
    "s_vortexbasealphacopy",       // the landscape vortex (*)
    "s_vortexbasemultiring",       // the landscape vortex (*)
    "sky",                         // the landscape
    "smallbump",
    "smoke",
    "snow",
    "weather",
    // (*) through the shared texture loader (create flags 0x41): GlobalTextures, the sprite and chain creators of the
    // spell files (TextureFileName), ZR_SurfRevol and the landscape vortex. The computed 0x41 of the .cmp landscape
    // converter (1, plus 0x40 if the a.raw exists) and the save-game pictures (made by the game) have no stem here.
});

/// Whether `stem` (the file name without ".raw", any case) is one of the colour stems of k_AlphaFlagStems
[[nodiscard]] constexpr bool IsAlphaFlagColour(std::string_view stem) noexcept
{
	const auto lower = [](char c) { return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c; };
	return std::ranges::any_of(k_AlphaFlagStems, [&](std::string_view entry) {
		return entry.size() == stem.size() && std::ranges::equal(entry, stem, [&](char a, char b) { return a == lower(b); });
	});
}

/// The colour stem of an alpha stem: `stem` without its final "a" (any case) when that is an IsAlphaFlagColour stem,
/// else empty. The loader builds the alpha name as the colour name without its last 4 characters plus "a.raw". That
/// alpha is cut only together with its colour: the loader runs only for a colour file of 0x30000 bytes and only then
/// reads xa.raw, so the caller checks the colour file too (S_IceEnvMapGreya.raw is not cut: its colour is 194823
/// bytes).
[[nodiscard]] constexpr std::string_view ColourOfAlpha(std::string_view stem) noexcept
{
	if (stem.empty() || (stem.back() != 'a' && stem.back() != 'A'))
	{
		return {};
	}
	const auto colour = stem.substr(0, stem.size() - 1);
	return IsAlphaFlagColour(colour) ? colour : std::string_view {};
}

/// Whether the texture `stem` (the file name without ".raw", any case) is cut to ARGB4444 by the original: a colour
/// stem of k_AlphaFlagStems or its alpha, the same stem plus "a" (ColourOfAlpha)
[[nodiscard]] constexpr bool HasAlphaFlag(std::string_view stem) noexcept
{
	return IsAlphaFlagColour(stem) || !ColourOfAlpha(stem).empty();
}

/// The blob shadow is built from ".\Data\Textures\human_shadow.raw" (0x400 bytes) as its own memory texture (create
/// flags 0x44): texel = (v & 0xF0) << 8, alpha Quantize(v) with R = G = B = 0. Not an alpha flag texture, but the same
/// cut.
inline constexpr std::string_view k_HumanShadowStem = "human_shadow";

/// The colour file the loader accepts: exactly 0x30000 bytes, 256 x 256 R, G, B, and the 0x10000 bytes of its alpha
inline constexpr size_t k_ColourBytes = 0x30000;
inline constexpr size_t k_AlphaBytes = 0x10000;

} // namespace openblack::graphics::argb4444
