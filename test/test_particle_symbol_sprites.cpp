/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <gtest/gtest.h>

#include "Particles/ParticleDrawFrame.h"
#include "Particles/ParticleSprites.h"

using namespace openblack::particles;

namespace
{
/// A sprite with the shape of a symbol's sprite
sprites::SpriteInstance SymbolShaped()
{
	return {
	    .positionHalfWidth = {10.0f, 20.0f, 30.0f, 2.5f},
	    .shape = {2.5f, 0.75f, 0.0f, 0.0f},
	    .uv = {0.25f, 0.5f, 0.125f, 0.125f},
	    .colour = {0.5f, 0.25f, 1.0f, 0.75f},
	    .flags = {0.0f, 0.0f, 0.0f, 0.0f},
	};
}
} // namespace

TEST(ParticleSymbolSprites, ASymbolShapedSpriteKeepsEveryValueTheShaderReads)
{
	const auto sprite = SymbolShaped();
	const auto symbol = sprites::AsSymbolSprite(sprite);
	ASSERT_TRUE(symbol.has_value());
	EXPECT_EQ(symbol->positionHalfWidth, sprite.positionHalfWidth);
	EXPECT_EQ(symbol->rollCell, glm::vec4(0.75f, 0.25f, 0.5f, 0.125f));
	EXPECT_EQ(symbol->colour, sprite.colour);
}

TEST(ParticleSymbolSprites, AnyOtherShapeKeepsTheFullSprite)
{
	auto taller = SymbolShaped();
	taller.shape.x = 3.0f;
	EXPECT_FALSE(sprites::AsSymbolSprite(taller).has_value());
	auto movedOrigin = SymbolShaped();
	movedOrigin.shape.z = 1.0f;
	EXPECT_FALSE(sprites::AsSymbolSprite(movedOrigin).has_value());
	movedOrigin = SymbolShaped();
	movedOrigin.shape.w = 1.0f;
	EXPECT_FALSE(sprites::AsSymbolSprite(movedOrigin).has_value());
	auto wideCell = SymbolShaped();
	wideCell.uv.z = 0.25f;
	EXPECT_FALSE(sprites::AsSymbolSprite(wideCell).has_value());
	auto flat = SymbolShaped();
	flat.flags.x = 1.0f;
	EXPECT_FALSE(sprites::AsSymbolSprite(flat).has_value());
	auto landTinted = SymbolShaped();
	landTinted.flags.y = 1.0f;
	EXPECT_FALSE(sprites::AsSymbolSprite(landTinted).has_value());
	auto lit = SymbolShaped();
	lit.flags.z = 65536.0f;
	EXPECT_FALSE(sprites::AsSymbolSprite(lit).has_value());
}

TEST(ParticleSymbolSprites, EverySpriteOfASymbolHasTheShapeOfASymbolsSprite)
{
	const draw::SymbolAtom atom {
	    .position = {100.0f, 20.0f, 300.0f}, .scale = 3.0f, .alpha = 200.0f, .age = 1.5f, .rgb = {255, 128, 64}};
	for (const int symbolsInARow : {1, 4, 8})
	{
		const auto symbol = draw::SymbolOf(atom, 0x00FF8040u, 3, symbolsInARow);
		for (const auto& sprite : symbol.instances)
		{
			EXPECT_TRUE(sprites::AsSymbolSprite(sprite).has_value());
		}
	}
}
