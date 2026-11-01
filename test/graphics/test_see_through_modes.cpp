/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>
#include <cstdint>

#include <algorithm>
#include <array>
#include <bit>
#include <optional>

#include <bgfx/defines.h>
#include <gtest/gtest.h>

#include "Common/X87.h"
#include "ECS/ScaffoldGhostRules.h"
#include "Graphics/SeeThroughState.h"
#include "Visual/RenderModes.h"

using namespace openblack;
using graphics::render_modes::Mode;

namespace
{

/// The game's second table of modes, for each mode of a material. The land mode keeps its entry, which draws as the
/// blended alpha textured mode in both tables.
constexpr std::array<Mode, graphics::render_modes::k_ModeCount> k_SecondTable {
    Mode::SmoothAlpha,
    Mode::SmoothAlpha,
    Mode::TexturedAlpha,
    Mode::TexturedAlpha,
    Mode::AlphaTexturedAlpha,
    Mode::AlphaTexturedAlpha,
    Mode::AlphaTexturedAlphaNz,
    Mode::SmoothAlphaNz,
    Mode::TexturedAlphaNz,
    Mode::TexturedChromaAlpha,
    Mode::AlphaTexturedAlphaAdditiveChroma,
    Mode::AlphaTexturedAlphaAdditiveChromaNz,
    Mode::AlphaTexturedAlphaAdditive,
    Mode::AlphaTexturedAlphaAdditiveNz,
    Mode::Landscape,
    Mode::TexturedChromaAlpha,
    Mode::TexturedChromaAlphaNz,
    Mode::TexturedAlpha,
    Mode::ChromaJustZ,
};

/// The reference as the game calculates it: in floats, the product times a float a little more than 1 over 255, less
/// 5, never below 0, cut to a whole number
uint8_t GameRef(uint8_t ref, uint8_t alpha)
{
	constexpr float k_OneOver255 = std::bit_cast<float>(0x3B808081U);
	const float product = static_cast<float>(static_cast<int32_t>(ref) * static_cast<int32_t>(alpha));
	float scaled = x87::Sub(x87::Mul(product, k_OneOver255), 5.0f);
	if (scaled < 0.0f)
	{
		scaled = 0.0f;
	}
	return static_cast<uint8_t>(std::trunc(scaled));
}

/// The reference as the object shader calculates it, with a division that may not round correctly
float ShaderRef(float ref, float alpha)
{
	return std::max(std::floor((ref * alpha + 0.5f) / 255.0f) - 5.0f, 0.0f);
}

} // namespace

TEST(SeeThroughModes, EachModeTakesItsModeOfTheSecondTable)
{
	for (uint32_t mode = 0; mode < graphics::render_modes::k_ModeCount; ++mode)
	{
		EXPECT_EQ(graphics::render_modes::SeeThrough(static_cast<Mode>(mode)), k_SecondTable.at(mode)) << mode;
	}
}

TEST(SeeThroughModes, OnlyTheBlendedChromaModeLowersItsReference)
{
	for (uint32_t mode = 0; mode < graphics::render_modes::k_ModeCount; ++mode)
	{
		const auto seeThrough = graphics::render_modes::SeeThrough(static_cast<Mode>(mode));
		EXPECT_EQ(graphics::render_modes::SeeThroughScalesRef(seeThrough), seeThrough == Mode::TexturedChromaAlpha) << mode;
	}
}

TEST(SeeThroughModes, TheLoweredReferenceIsTheGamesForEveryReferenceAndAlpha)
{
	for (uint32_t ref = 0; ref < 256; ++ref)
	{
		for (uint32_t alpha = 0; alpha < 256; ++alpha)
		{
			const auto expected = GameRef(static_cast<uint8_t>(ref), static_cast<uint8_t>(alpha));
			ASSERT_EQ(graphics::render_modes::SeeThroughRef(static_cast<uint8_t>(ref), static_cast<uint8_t>(alpha)), expected)
			    << ref << " " << alpha;
			ASSERT_EQ(ShaderRef(static_cast<float>(ref), static_cast<float>(alpha)), static_cast<float>(expected))
			    << ref << " " << alpha;
		}
	}
}

TEST(SeeThroughModes, ThePhantomOnASiteDropsNoFragmentAtItsAlphas)
{
	// The phantom on a site has an alpha from 85 down to 14
	for (uint32_t alpha = 14; alpha <= 85; ++alpha)
	{
		EXPECT_EQ(graphics::render_modes::SeeThroughRef(ecs::scaffold_ghost::k_SiteAlphaRef, static_cast<uint8_t>(alpha)), 0);
	}
	EXPECT_EQ(ecs::scaffold_ghost::SeeThroughRef(false), std::optional<uint8_t>(10));
	EXPECT_EQ(ecs::scaffold_ghost::SeeThroughRef(true), std::nullopt);
}

TEST(SeeThroughModes, TheModeSetsTheBlendingAndTheDepthWrite)
{
	using graphics::render_modes::Desc;
	using graphics::see_through::State;
	EXPECT_EQ(State(Desc(Mode::TexturedChromaAlpha)), BGFX_STATE_WRITE_Z | BGFX_STATE_BLEND_ALPHA);
	EXPECT_EQ(State(Desc(Mode::TexturedAlpha)), BGFX_STATE_WRITE_Z | BGFX_STATE_BLEND_ALPHA);
	EXPECT_EQ(State(Desc(Mode::TexturedAlphaNz)), BGFX_STATE_BLEND_ALPHA);
	EXPECT_EQ(State(Desc(Mode::AlphaTexturedAlphaAdditive)),
	          BGFX_STATE_WRITE_Z | BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_SRC_ALPHA, BGFX_STATE_BLEND_ONE));
	EXPECT_EQ(State(Desc(Mode::ChromaJustZ)),
	          BGFX_STATE_WRITE_Z | BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_ZERO, BGFX_STATE_BLEND_ONE));
}

TEST(SeeThroughModes, TheModeSetsTheAlphaTest)
{
	using graphics::see_through::AlphaTest;
	using graphics::see_through::TestOf;
	EXPECT_EQ(TestOf(Mode::TexturedChromaAlpha), AlphaTest::ScaledReference);
	EXPECT_EQ(TestOf(Mode::TexturedChromaAlphaNz), AlphaTest::Reference);
	EXPECT_EQ(TestOf(Mode::ChromaJustZ), AlphaTest::Reference);
	EXPECT_EQ(TestOf(Mode::TexturedAlpha), AlphaTest::Off);
	EXPECT_EQ(TestOf(Mode::AlphaTexturedAlpha), AlphaTest::Off);
}
