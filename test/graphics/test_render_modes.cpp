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

#include <bgfx/defines.h>
#include <gtest/gtest.h>

#include "Graphics/DrawOrder.h"
#include "Graphics/SeeThroughState.h"
#include "Visual/RenderModes.h"

using namespace openblack;
using graphics::render_modes::Mode;

namespace
{

/// The alpha test as the object and tree shaders do it (assets/shaders/alpha_test.sh), with the alpha and the reference
/// as the renderer gives them: 0 to 1
bool ShaderFailsAlphaTest(float alpha, float threshold)
{
	if (threshold < 0.0f)
	{
		return false;
	}
	const float reference = std::floor(threshold * 255.0f + 0.5f);
	if (reference < 0.5f)
	{
		return alpha <= 0.0f;
	}
	return std::floor(alpha * 255.0f + 0.5f) < reference;
}

} // namespace

TEST(RenderModes, TheAlphaTestKeepsTheTexelsAtTheReferenceOrMore)
{
	using graphics::render_modes::PassesAlphaTest;
	EXPECT_TRUE(PassesAlphaTest(220, 220));
	EXPECT_TRUE(PassesAlphaTest(221, 220));
	EXPECT_FALSE(PassesAlphaTest(219, 220));
	EXPECT_TRUE(PassesAlphaTest(0, 0));
	EXPECT_TRUE(PassesAlphaTest(255, 255));
	EXPECT_FALSE(PassesAlphaTest(254, 255));
}

TEST(RenderModes, TheShaderTestIsTheGamesForEveryAlphaAndReference)
{
	for (uint32_t ref = 1; ref < 256; ++ref)
	{
		for (uint32_t alpha = 0; alpha < 256; ++alpha)
		{
			const bool passes = graphics::render_modes::PassesAlphaTest(static_cast<uint8_t>(alpha), static_cast<uint8_t>(ref));
			ASSERT_EQ(ShaderFailsAlphaTest(static_cast<float>(alpha) / 255.0f, static_cast<float>(ref) / 255.0f), !passes)
			    << ref << " " << alpha;
		}
	}
}

TEST(RenderModes, AMaterialWithNoTestDropsOnlyTheTexelsWithNoAlpha)
{
	EXPECT_TRUE(ShaderFailsAlphaTest(0.0f, 0.0f));
	EXPECT_FALSE(ShaderFailsAlphaTest(1.0f / 255.0f, 0.0f));
}

TEST(RenderModes, TheAlphaTexturedModeKeepsItsTexelsWithNoAlphaAndTheirDepth)
{
	using graphics::render_modes::Desc;
	using graphics::render_modes::KeepsTexelsWithNoAlpha;
	EXPECT_TRUE(KeepsTexelsWithNoAlpha(Desc(Mode::AlphaTextured)));
	EXPECT_TRUE(KeepsTexelsWithNoAlpha(Desc(Mode::TexturedAlpha)));
	EXPECT_TRUE(KeepsTexelsWithNoAlpha(Desc(Mode::AlphaTexturedAlpha)));
	// The chroma modes have a test, and the modes without depth write show nothing there
	EXPECT_FALSE(KeepsTexelsWithNoAlpha(Desc(Mode::TexturedChroma)));
	EXPECT_FALSE(KeepsTexelsWithNoAlpha(Desc(Mode::AlphaTexturedAlphaNz)));
	EXPECT_FALSE(KeepsTexelsWithNoAlpha(Desc(Mode::AlphaTexturedAlphaAdditiveNz)));
	EXPECT_FALSE(KeepsTexelsWithNoAlpha(Desc(Mode::AlphaTexturedAlphaAdditiveChroma)));
	EXPECT_FALSE(KeepsTexelsWithNoAlpha(Desc(Mode::ChromaJustZ)));
	// The renderer gives such a material a reference below 0, so the shader keeps every texel and writes its depth
	EXPECT_FALSE(ShaderFailsAlphaTest(0.0f, -1.0f));
}

TEST(RenderModes, TheAddedModeThatWritesDepthKeepsTheDepthOfItsTexelsWithNoAlpha)
{
	using graphics::render_modes::Desc;
	using graphics::render_modes::KeepsTexelsWithNoAlpha;
	// The added mode with depth write has no alpha test. A texel with no alpha adds nothing, but it writes its depth.
	const auto& added = Desc(Mode::AlphaTexturedAlphaAdditive);
	EXPECT_EQ(added.blend, graphics::render_modes::Blend::Additive);
	EXPECT_FALSE(added.alphaTest);
	EXPECT_TRUE(added.zWrite);
	EXPECT_TRUE(KeepsTexelsWithNoAlpha(added));
}

TEST(RenderModes, TheTexturedModeShowsTheColourOfItsTexelsWithNoAlpha)
{
	using graphics::render_modes::Desc;
	using graphics::render_modes::KeepsTexelsWithNoAlpha;
	// The opaque textured mode has no alpha test and no blend, so each texel shows its colour
	const auto& textured = Desc(Mode::Textured);
	EXPECT_EQ(textured.blend, graphics::render_modes::Blend::Disabled);
	EXPECT_FALSE(textured.alphaTest);
	EXPECT_TRUE(KeepsTexelsWithNoAlpha(textured));
	EXPECT_TRUE(KeepsTexelsWithNoAlpha(Desc(Mode::Mode17)));
}

TEST(RenderModes, AOneSidedMaterialShowsOnlyItsFrontFaces)
{
	using graphics::render_modes::DrawsBothSides;
	// The first bit of the cull byte alone gives the two-sided material
	EXPECT_FALSE(DrawsBothSides(0));
	EXPECT_TRUE(DrawsBothSides(1));
	EXPECT_FALSE(DrawsBothSides(4));
	EXPECT_TRUE(DrawsBothSides(5));
}

TEST(RenderModes, TheChromaModeBlendsByTheTextureAlphaAfterItsTestAndWritesTheDepth)
{
	const auto& chroma = graphics::render_modes::Desc(Mode::TexturedChroma);
	EXPECT_EQ(chroma.blend, graphics::render_modes::Blend::Standard);
	EXPECT_TRUE(chroma.alphaTest);
	EXPECT_TRUE(chroma.zWrite);
	EXPECT_FALSE(chroma.alphaModulate);
	EXPECT_TRUE(chroma.textured);
	EXPECT_EQ(graphics::see_through::State(chroma), BGFX_STATE_WRITE_Z | BGFX_STATE_BLEND_ALPHA);
}

TEST(RenderModes, AStandingBlendedDrawKeepsItsPlaceInTheView)
{
	// A blended draw is never gathered with the opaque draws by its textures, so the view keeps the order of the draws
	constexpr uint64_t k_Chroma =
	    BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_Z | BGFX_STATE_DEPTH_TEST_GREATER | BGFX_STATE_BLEND_ALPHA;
	EXPECT_FALSE(graphics::draw_order::Reorderable(graphics::RenderPass::Main, k_Chroma, 0));
	EXPECT_TRUE(graphics::draw_order::Reorderable(graphics::RenderPass::Main, k_Chroma & ~BGFX_STATE_BLEND_MASK, 0));
}
