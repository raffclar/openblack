/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <gtest/gtest.h>

#include "Graphics/RenderModes.h"

using namespace openblack::graphics;

namespace
{
/// The lion's body and mane strands, as its base mesh gives their materials: the body textured with its back faces
/// culled, the strands textured with a chroma key at alpha 150 and both faces shown
constexpr auto k_BodyMode = render_modes::Mode::Textured;
constexpr uint32_t k_BodyCull = 4;
constexpr auto k_ManeMode = render_modes::Mode::TexturedChroma;
constexpr uint32_t k_ManeCull = 5;
} // namespace

TEST(CreatureManeMaterial, TheStrandsAreCutBelowTheirReferenceAndBlendByTheirAlpha)
{
	const auto& mane = render_modes::Desc(k_ManeMode);
	EXPECT_TRUE(mane.alphaTest);
	EXPECT_EQ(mane.blend, render_modes::Blend::Standard);
	EXPECT_TRUE(mane.zWrite);
	EXPECT_TRUE(mane.textured);
	// The alpha is the texture's alone
	EXPECT_FALSE(mane.alphaModulate);
}

TEST(CreatureManeMaterial, TheBodyIsOpaque)
{
	const auto& body = render_modes::Desc(k_BodyMode);
	EXPECT_FALSE(body.alphaTest);
	EXPECT_EQ(body.blend, render_modes::Blend::Disabled);
	EXPECT_TRUE(body.zWrite);
}

TEST(CreatureManeMaterial, OnlyTheStrandsShowTheirBackFaces)
{
	EXPECT_TRUE(render_modes::DrawsBothSides(k_ManeCull));
	EXPECT_FALSE(render_modes::DrawsBothSides(k_BodyCull));
	EXPECT_FALSE(render_modes::DrawsBothSides(0));
	EXPECT_TRUE(render_modes::DrawsBothSides(1));
}
