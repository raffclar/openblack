/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// argb_colour:: (src/Graphics/ArgbColour.h) against the original's integer colour maths: the diffuse tint
// (emulated below), the divide by 255 done as a reciprocal multiply (signed and unsigned forms), the alpha rules of
// the colour helpers, the mist and the creature draw, and the model light's (c * f) >> 8 (model_light::Apply)

#include <cmath>
#include <cstdint>

#include <gtest/gtest.h>

#include "Graphics/ArgbColour.h"
#include "Graphics/ModelLight.h"

using namespace openblack;

namespace
{
/// The original's diffuse tint step by step: c = the object's colour, t = the tint
uint32_t EmulateDiffuse(uint32_t c, uint32_t t)
{
	uint32_t esi = c;
	uint32_t ecx = (t >> 16) & 0xFFu;
	uint32_t edi = esi >> 8;
	uint32_t edx = edi & 0xFF00u;
	uint32_t eax = t;
	ecx *= edx;
	edx = edi & 0xFF0000u;
	const uint32_t ebx = eax >> 24;
	edx *= ebx;
	edx &= 0xFF00FFFFu;
	ecx |= edx;
	edx = (eax >> 8) & 0xFFu; // the green byte
	edi &= 0xFFu;
	esi &= 0xFFu;
	eax &= 0xFFu;
	esi *= eax;
	edx *= edi;
	ecx &= 0xFFFF00FFu;
	ecx |= edx;
	esi >>= 8;
	ecx &= 0xFFFFFF00u;
	ecx |= esi;
	return ecx;
}

/// The mist's divide by 255: a signed reciprocal multiply, + x, shift right 7, + the sign bit
int32_t EmulateSignedDiv255(int32_t x)
{
	const auto product = static_cast<int64_t>(static_cast<int32_t>(0x80808081u)) * x;
	auto edx = static_cast<int32_t>(static_cast<uint64_t>(product) >> 32);
	edx += x;
	edx >>= 7;
	return edx + static_cast<int32_t>(static_cast<uint32_t>(edx) >> 31);
}

/// The unsigned divide by 255: a reciprocal multiply, shift right 7
uint32_t EmulateUnsignedDiv255(uint32_t x)
{
	return static_cast<uint32_t>((static_cast<uint64_t>(0x80808081u) * x) >> 32) >> 7;
}

/// A small LCG so the sweep is reproducible
uint32_t Next(uint32_t& state)
{
	state = state * 1664525u + 1013904223u;
	return state;
}
} // namespace

TEST(ArgbColour, MultiplyArgbShift8MatchesTheDiffuseEmulation)
{
	uint32_t state = 12345u;
	for (int i = 0; i < 100000; ++i)
	{
		const uint32_t c = Next(state);
		const uint32_t t = Next(state);
		ASSERT_EQ(argb_colour::MultiplyArgbShift8(c, t), EmulateDiffuse(c, t)) << std::hex << c << " " << t;
	}
	for (const uint32_t c : {0u, 0xFFFFFFFFu, 0xFF000000u, 0x00FFFFFFu, 0x80808080u})
	{
		for (const uint32_t t : {0u, 0xFFFFFFFFu, 0x96FFFFFFu, 0xFF505050u, 0x01010101u})
		{
			EXPECT_EQ(argb_colour::MultiplyArgbShift8(c, t), EmulateDiffuse(c, t));
		}
	}
}

TEST(ArgbColour, MultiplyArgbShift8Values)
{
	// a tint of 0xFF takes one off every channel, so a field (blend alpha 0xFF) ends at 254
	EXPECT_EQ(argb_colour::MultiplyArgbShift8(0xFFFFFFFFu, 0xFFFFFFFFu), 0xFEFEFEFEu);
	// the one-shot orb: the land colour's alpha 0xFF times the orb's alpha 0x96 = 0x95
	EXPECT_EQ(argb_colour::Alpha(argb_colour::MultiplyArgbShift8(0xFF000000u, 0x96FFFFFFu)), 0x95u);
}

TEST(ArgbColour, AddArgbSaturated)
{
	// the diffuse tint: all four channels, the alpha too
	EXPECT_EQ(argb_colour::AddArgbSaturated(0xF0F01020u, 0x20200101u), 0xFFFF1121u);
	EXPECT_EQ(argb_colour::AddArgbSaturated(0u, 0u), 0u);
	EXPECT_EQ(argb_colour::AddArgbSaturated(0xFFFFFFFFu, 0xFFFFFFFFu), 0xFFFFFFFFu);
	// the add that keeps the first argument's alpha
	EXPECT_EQ(argb_colour::AddRgbSaturatedKeepAlpha(0x40F01020u, 0xFF200101u), 0x40FF1121u);
	EXPECT_EQ(argb_colour::AddRgbSaturatedKeepAlpha(0x00000000u, 0xFFFFFFFFu), 0x00FFFFFFu);
}

TEST(ArgbColour, MultiplyRgbShift8KeepAlpha)
{
	// the product that keeps the first argument's alpha
	EXPECT_EQ(argb_colour::MultiplyRgbShift8KeepAlpha(0x12FFFFFFu, 0xFF808080u), 0x127F7F7Fu);
	EXPECT_EQ(argb_colour::MultiplyRgbShift8KeepAlpha(0xFFFFFFFFu, 0x00FFFFFFu), 0xFFFEFEFEu);
	// the RGB is the same as the four-channel product's
	uint32_t state = 777u;
	for (int i = 0; i < 10000; ++i)
	{
		const uint32_t a = Next(state);
		const uint32_t b = Next(state);
		ASSERT_EQ(argb_colour::MultiplyRgbShift8KeepAlpha(a, b) & 0x00FFFFFFu,
		          argb_colour::MultiplyArgbShift8(a, b) & 0x00FFFFFFu);
		ASSERT_EQ(argb_colour::Alpha(argb_colour::MultiplyRgbShift8KeepAlpha(a, b)), argb_colour::Alpha(a));
	}
	// the scalar form: a grey t = k in every channel
	for (uint32_t k = 0; k <= 255; ++k)
	{
		const uint32_t grey = argb_colour::Argb(k, k, k, 0);
		ASSERT_EQ(argb_colour::ScaleRgbShift8KeepAlpha(0xA0C08040u, k),
		          argb_colour::MultiplyRgbShift8KeepAlpha(0xA0C08040u, grey));
	}
}

TEST(ArgbColour, ScaleRgbShift8KeepAlphaMatchesModelLight)
{
	// model_light::Apply is ScaleRgbShift8KeepAlpha by the factor: I = 255 with amb 90 gives f = 254
	EXPECT_EQ(model_light::Apply(0x80FFFFFFu, 255, 90), 0x80FDFDFDu);
	for (int intensity = -255; intensity <= 255; intensity += 3)
	{
		const auto f = static_cast<uint32_t>(model_light::Factor(intensity, 90));
		EXPECT_EQ(model_light::Apply(0x7F10E0A5u, intensity, 90), argb_colour::ScaleRgbShift8KeepAlpha(0x7F10E0A5u, f));
	}
}

TEST(ArgbColour, MultiplyBytesIsTheBinarysDivide)
{
	// trunc(x / 255) for every product of two bytes, by the signed (mist) and unsigned forms of the reciprocal
	// multiply
	for (uint32_t x = 0; x <= 255u * 255u; ++x)
	{
		ASSERT_EQ(static_cast<uint32_t>(EmulateSignedDiv255(static_cast<int32_t>(x))), x / 255u) << x;
		ASSERT_EQ(EmulateUnsignedDiv255(x), x / 255u) << x;
	}
	for (uint32_t a = 0; a <= 255u; ++a)
	{
		for (uint32_t b = 0; b <= 255u; ++b)
		{
			ASSERT_EQ(argb_colour::MultiplyBytes(a, b), EmulateUnsignedDiv255(a * b));
		}
	}
}

TEST(ArgbColour, MultiplyAlphaRules)
{
	// the four channels
	EXPECT_EQ(argb_colour::MultiplyArgb(0x80808080u, 0xFFFFFFFFu), 0x80808080u);
	EXPECT_EQ(argb_colour::MultiplyArgb(0xC8C8C8C8u, 0x80808080u), 0x64646464u);
	// the mist: the colour's alpha, not the light's
	EXPECT_EQ(argb_colour::MultiplyRgbKeepAlpha(0x80C8C8C8u, 0x00808080u), 0x80646464u);
	EXPECT_EQ(argb_colour::MultiplyRgbKeepAlpha(0x10FF8040u, 0xFFFFFFFFu), 0x10FF8040u);
	// the creature draw: opaque
	EXPECT_EQ(argb_colour::MultiplyRgbOpaque(0x00FFFFFFu, 0x00FFFFFFu), 0xFFFFFFFFu);
	EXPECT_EQ(argb_colour::MultiplyRgbOpaque(0x12C8C8C8u, 0x34808080u), 0xFF646464u);
}

TEST(ArgbColour, Conversions)
{
	EXPECT_EQ(argb_colour::Argb(1, 2, 3, 4), 0x04010203u);
	EXPECT_EQ(argb_colour::Argb(0x101, 0x102, 0x103), 0x00010203u);
	EXPECT_EQ(argb_colour::Red(0x11223344u), 0x22u);
	EXPECT_EQ(argb_colour::Green(0x11223344u), 0x33u);
	EXPECT_EQ(argb_colour::Blue(0x11223344u), 0x44u);
	EXPECT_EQ(argb_colour::Alpha(0x11223344u), 0x11u);
	EXPECT_EQ(argb_colour::ToAbgr(0x11223344u), 0x11443322u);
	EXPECT_EQ(argb_colour::ToAbgr(0x11223344u, 0x99u), 0x99443322u);
	EXPECT_EQ(argb_colour::ToAbgr(glm::vec4(1.0f, 0.0f, 0.5f, 1.0f)), 0xFF8000FFu);
	EXPECT_EQ(argb_colour::ToAbgr(glm::vec4(-1.0f, 2.0f, 0.0f, 0.0f)), 0x0000FF00u);
	EXPECT_EQ(argb_colour::ToVec4(0xFF000000u), glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
	EXPECT_EQ(argb_colour::ToVec4(0x00FF0000u), glm::vec4(1.0f, 0.0f, 0.0f, 0.0f));
	EXPECT_EQ(argb_colour::ToVec3(0x000000FFu), glm::vec3(0.0f, 0.0f, 1.0f));
	EXPECT_FLOAT_EQ(argb_colour::ToVec4(0x80808080u).g, 128.0f / 255.0f);
}

namespace
{
/// argb_colour.sh UnpackRgb24 in float, as vs_object runs it (the divisions are by powers of two: exact)
glm::uvec3 ShaderUnpack(float packed)
{
	const float red = std::floor(packed / 65536.0f);
	const float green = std::floor((packed - red * 65536.0f) / 256.0f);
	return {static_cast<uint32_t>(red), static_cast<uint32_t>(green),
	        static_cast<uint32_t>(packed - red * 65536.0f - green * 256.0f)};
}
glm::uvec3 Rgb(uint32_t argb)
{
	return {argb_colour::Red(argb), argb_colour::Green(argb), argb_colour::Blue(argb)};
}
} // namespace

TEST(ArgbColour, InstanceColumnExtremes)
{
	glm::vec4 lh3d(0.0f);
	// the white tint (all bits set) is the most negative value, -2^24, still exact
	argb_colour::PackInstanceTint(lh3d, 0xFFFFFFFFu);
	EXPECT_EQ(lh3d.x, -16777216.0f);
	EXPECT_EQ(argb_colour::InstanceTint(lh3d), 0x00FFFFFFu);
	EXPECT_FALSE(argb_colour::InstanceColour(lh3d).has_value());
	// a black tint is not "no tint"
	argb_colour::PackInstanceTint(lh3d, 0xFF000000u);
	EXPECT_EQ(lh3d.x, -1.0f);
	EXPECT_EQ(argb_colour::InstanceTint(lh3d), 0u);
	// SetColorSpecular's colour, up to 2^24
	argb_colour::PackInstanceColour(lh3d, 0x00FFFFFFu);
	EXPECT_EQ(lh3d.x, 16777216.0f);
	EXPECT_EQ(argb_colour::InstanceColour(lh3d), 0x00FFFFFFu);
	EXPECT_FALSE(argb_colour::InstanceTint(lh3d).has_value());
	argb_colour::PackInstanceColour(lh3d, 0u);
	EXPECT_EQ(argb_colour::InstanceColour(lh3d), 0u);
	// the zero column: the land light alone, no specular, no window
	const glm::vec4 none(0.0f);
	EXPECT_FALSE(argb_colour::InstanceTint(none).has_value());
	EXPECT_FALSE(argb_colour::InstanceColour(none).has_value());
	EXPECT_EQ(argb_colour::InstanceSpecular(none), 0u);
	EXPECT_FALSE(argb_colour::InstanceWindow(none).has_value());
	// the specular keeps its 8 bits (the 7 of the old 3e6 encoding lost the low one) and drops the alpha
	argb_colour::PackInstanceSpecular(lh3d, 0xFF001000u); // the poison's
	EXPECT_EQ(lh3d.y, 4096.0f);
	argb_colour::PackInstanceSpecular(lh3d, 0xFFFFFFFFu);
	EXPECT_EQ(argb_colour::InstanceSpecular(lh3d), 0x00FFFFFFu);
	argb_colour::PackInstanceSpecular(lh3d, 0x00010101u);
	EXPECT_EQ(ShaderUnpack(lh3d.y), glm::uvec3(1u, 1u, 1u));
	// the window: 0 = not lit; a lit black window (intensity 1: (0xE0 * 1) >> 8 = 0) is still lit
	argb_colour::PackInstanceWindow(lh3d, 0u);
	EXPECT_FALSE(argb_colour::InstanceWindow(lh3d).has_value());
	argb_colour::PackInstanceWindow(lh3d, 0xFF000000u);
	EXPECT_EQ(argb_colour::InstanceWindow(lh3d), 0u);
	argb_colour::PackInstanceWindow(lh3d, 0xFFFCFCFCu);
	EXPECT_EQ(argb_colour::InstanceWindow(lh3d), 0x00FCFCFCu);
	// each field has its own float: packing one leaves the others
	glm::vec4 all(0.0f);
	argb_colour::PackInstanceTint(all, 0xFFE8FFDDu); // the poison's diffuse
	argb_colour::PackInstanceSpecular(all, 0xFF001000u);
	argb_colour::PackInstanceWindow(all, 0xFFE0E0E0u);
	EXPECT_EQ(argb_colour::InstanceTint(all), 0x00E8FFDDu);
	EXPECT_EQ(argb_colour::InstanceSpecular(all), 0x00001000u);
	EXPECT_EQ(argb_colour::InstanceWindow(all), 0x00E0E0E0u);
	EXPECT_EQ(all.w, 0.0f);
}

TEST(ArgbColour, InstanceColumnRoundTripsThroughTheShader)
{
	uint32_t state = 4242u;
	for (int i = 0; i < 200000; ++i)
	{
		const uint32_t argb = Next(state);
		glm::vec4 lh3d(0.0f);
		argb_colour::PackInstanceTint(lh3d, argb);
		argb_colour::PackInstanceSpecular(lh3d, argb ^ 0x5A5A5Au);
		argb_colour::PackInstanceWindow(lh3d, argb | 0xFF000000u);
		// vs_object: -x - 1, y, z - 1 through UnpackRgb24
		ASSERT_EQ(ShaderUnpack(-lh3d.x - 1.0f), Rgb(argb)) << std::hex << argb;
		ASSERT_EQ(ShaderUnpack(lh3d.y), Rgb(argb ^ 0x5A5A5Au)) << std::hex << argb;
		ASSERT_EQ(ShaderUnpack(lh3d.z - 1.0f), Rgb(argb)) << std::hex << argb;
		argb_colour::PackInstanceColour(lh3d, argb);
		ASSERT_EQ(ShaderUnpack(lh3d.x - 1.0f), Rgb(argb)) << std::hex << argb;
		ASSERT_EQ(argb_colour::InstanceColour(lh3d), argb & 0x00FFFFFFu);
	}
}

TEST(ArgbColour, WhiteTintTakesOneOff)
{
	// the diffuse tint with t = 0xFFFFFFFF (the town centre, the villagers with a specular, the physical shield): the
	// land light loses 1 in each channel, as vs_object's MultiplyShift8 does with the unpacked tint
	glm::vec4 lh3d(0.0f);
	argb_colour::PackInstanceTint(lh3d, 0xFFFFFFFFu);
	const auto tint = ShaderUnpack(-lh3d.x - 1.0f);
	for (uint32_t c = 0; c <= 255u; ++c)
	{
		ASSERT_EQ((c * tint.r) >> 8, argb_colour::Red(argb_colour::MultiplyArgbShift8(c << 16, 0xFFFFFFFFu)));
		ASSERT_EQ((c * tint.r) >> 8, c == 0 ? 0u : c - 1u);
	}
}

TEST(ArgbColour, TreeTintGoesAfterTheHaze)
{
	// the tree draw multiplies the hazed colour: the same tint, flagged in w; every other tint leaves w at 0
	glm::vec4 tree(0.0f);
	argb_colour::PackInstanceTreeTint(tree, 0xFFC0C0C0u);
	ASSERT_EQ(argb_colour::InstanceTint(tree), 0x00C0C0C0u);
	ASSERT_TRUE(argb_colour::InstanceTintAfterHaze(tree));
	glm::vec4 other(0.0f);
	argb_colour::PackInstanceTint(other, 0xFFC0C0C0u);
	ASSERT_EQ(argb_colour::InstanceTint(other), 0x00C0C0C0u);
	ASSERT_FALSE(argb_colour::InstanceTintAfterHaze(other));
}
