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

/// The game's 19 render modes: each sets a draw's blending, alpha test, depth write and how the texture's alpha is
/// combined with the vertex colour. An L3D material's type is the number of the mode it is drawn in.
namespace openblack::graphics::render_modes
{

enum class Mode : uint8_t
{
	Smooth = 0,
	SmoothAlpha = 1,
	Textured = 2,
	TexturedAlpha = 3,
	AlphaTextured = 4,
	AlphaTexturedAlpha = 5,
	AlphaTexturedAlphaNz = 6,
	SmoothAlphaNz = 7,
	TexturedAlphaNz = 8,
	TexturedChroma = 9,
	AlphaTexturedAlphaAdditiveChroma = 10,
	AlphaTexturedAlphaAdditiveChromaNz = 11,
	AlphaTexturedAlphaAdditive = 12,
	AlphaTexturedAlphaAdditiveNz = 13,
	Landscape = 14, ///< the land blocks, drawn as AlphaTexturedAlpha
	TexturedChromaAlpha = 15,
	TexturedChromaAlphaNz = 16,
	Mode17 = 17, ///< drawn as Textured; no material uses it
	ChromaJustZ = 18,
};
inline constexpr uint32_t k_ModeCount = 19;

enum class Blend : uint8_t
{
	Disabled,
	Standard, ///< source alpha, 1 - source alpha
	Additive, ///< source alpha, 1
	JustZ,    ///< nothing of the colour is written, only the depth where the alpha test passes
};

struct ModeDesc
{
	Blend blend;
	/// Fragments whose alpha is below the material's reference are dropped
	bool alphaTest;
	bool zWrite;
	/// The alpha is the texture's times the vertex colour's, else the texture's alone (or none without a texture)
	bool alphaModulate;
	/// Whether the mode draws the material's texture
	bool textured;
};

// clang-format off
inline constexpr std::array<ModeDesc, k_ModeCount> k_Modes = {{
	{Blend::Disabled, false, true,  false, false}, // 0
	{Blend::Standard, false, true,  false, false}, // 1
	{Blend::Disabled, false, true,  false, true},  // 2
	{Blend::Standard, false, true,  true,  true},  // 3
	{Blend::Standard, false, true,  false, true},  // 4
	{Blend::Standard, false, true,  true,  true},  // 5
	{Blend::Standard, false, false, true,  true},  // 6
	{Blend::Standard, false, false, false, false}, // 7
	{Blend::Standard, false, false, true,  true},  // 8
	{Blend::Standard, true,  true,  false, true},  // 9
	{Blend::Additive, true,  true,  true,  true},  // 10
	{Blend::Additive, true,  false, true,  true},  // 11
	{Blend::Additive, false, true,  true,  true},  // 12
	{Blend::Additive, false, false, true,  true},  // 13
	{Blend::Standard, false, true,  true,  true},  // 14, as 5
	{Blend::Standard, true,  true,  true,  true},  // 15
	{Blend::Standard, true,  false, true,  true},  // 16
	{Blend::Disabled, false, true,  false, true},  // 17, as 2
	{Blend::JustZ,    true,  true,  false, true},  // 18
}};
// clang-format on

[[nodiscard]] constexpr const ModeDesc& Desc(Mode mode)
{
	return k_Modes.at(static_cast<uint8_t>(mode));
}

/// The mode that a material draws in when its whole object is see-through: the game's second table of modes. The opaque
/// modes become their blended forms, and the chroma mode becomes the blended chroma mode. The other modes stay.
[[nodiscard]] constexpr Mode SeeThrough(Mode mode)
{
	switch (mode)
	{
	case Mode::Smooth:
		return Mode::SmoothAlpha;
	case Mode::Textured:
	case Mode::Mode17:
		return Mode::TexturedAlpha;
	case Mode::AlphaTextured:
		return Mode::AlphaTexturedAlpha;
	case Mode::TexturedChroma:
		return Mode::TexturedChromaAlpha;
	default:
		return mode;
	}
}

/// Whether a see-through object lowers the alpha reference of the mode by its alpha. Only the two chroma modes with
/// an alpha test and depth do this, and the second table uses only the blended one.
[[nodiscard]] constexpr bool SeeThroughScalesRef(Mode seeThroughMode)
{
	return seeThroughMode == Mode::TexturedChromaAlpha || seeThroughMode == Mode::TexturedChroma;
}

/// The alpha reference that `SeeThroughScalesRef` gives for the reference `ref` and an object alpha `alpha` of 255. It
/// is the reference times the alpha over 255, less 5, cut to a whole number, and never below 0.
///
/// The game multiplies by a float a little more than 1 over 255. This float never gives a result below the true
/// quotient, and never gives the next whole number. Thus a division of whole numbers gives the same result.
[[nodiscard]] constexpr uint8_t SeeThroughRef(uint8_t ref, uint8_t alpha)
{
	constexpr int32_t k_Less = 5;
	const int32_t scaled = (static_cast<int32_t>(ref) * static_cast<int32_t>(alpha)) / 255 - k_Less;
	return static_cast<uint8_t>(scaled < 0 ? 0 : scaled);
}

/// Whether a material shows both faces of its triangles: the first bit of its cull byte set. Otherwise the back faces
/// are not drawn.
[[nodiscard]] constexpr bool DrawsBothSides(uint32_t cullByte)
{
	return (cullByte & 1U) != 0;
}

/// What the game can change of the materials of a model while it runs
struct Properties
{
	/// The texture is added to what is behind it
	bool additive {false};
	bool writesDepth {true};
	bool bothSides {false};
	/// The material uses the alpha of its texture
	bool textureAlpha {true};
};

/// The mode of a material after the game changes its properties
[[nodiscard]] constexpr Mode Changed(Mode mode, const Properties& properties)
{
	if (mode == Mode::AlphaTextured)
	{
		mode = Mode::AlphaTexturedAlphaNz;
	}
	if (!properties.textureAlpha)
	{
		mode = Mode::TexturedAlpha;
	}
	if (properties.additive)
	{
		mode = Mode::AlphaTexturedAlphaAdditiveNz;
	}
	if (properties.writesDepth)
	{
		switch (mode)
		{
		case Mode::AlphaTexturedAlphaNz:
			return Mode::AlphaTexturedAlpha;
		case Mode::AlphaTexturedAlphaAdditiveNz:
			return Mode::AlphaTexturedAlphaAdditive;
		case Mode::TexturedAlphaNz:
			return Mode::TexturedAlpha;
		case Mode::TexturedChromaAlphaNz:
			return Mode::TexturedChroma;
		default:
			return mode;
		}
	}
	switch (mode)
	{
	case Mode::AlphaTexturedAlpha:
		return Mode::AlphaTexturedAlphaNz;
	case Mode::AlphaTexturedAlphaAdditive:
		return Mode::AlphaTexturedAlphaAdditiveNz;
	case Mode::TexturedAlpha:
	case Mode::Textured:
		return Mode::TexturedAlphaNz;
	case Mode::TexturedChroma:
		return Mode::TexturedChromaAlphaNz;
	default:
		return mode;
	}
}

/// The cull byte of a material after the game changes its properties: only its first bit changes
[[nodiscard]] constexpr uint8_t ChangedCull(uint8_t cullByte, const Properties& properties)
{
	return properties.bothSides ? static_cast<uint8_t>(cullByte | 1U) : static_cast<uint8_t>(cullByte & ~1U);
}

} // namespace openblack::graphics::render_modes
