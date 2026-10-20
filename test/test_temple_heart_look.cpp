/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <gtest/gtest.h>

#include "Graphics/RenderModes.h"
#include "Graphics/TempleHeartLook.h"
#include "Temple/TempleDestruction.h"

using namespace openblack::graphics;
using namespace openblack::graphics::temple_heart_look;
using render_modes::Mode;
using render_modes::Properties;

TEST(ChangedMaterials, WithoutDepthTheSolidTexturedModesBlendByTheirTextureAlpha)
{
	constexpr Properties k_Faded {.additive = false, .writesDepth = false, .bothSides = false, .textureAlpha = true};
	EXPECT_EQ(render_modes::Changed(Mode::Textured, k_Faded), Mode::TexturedAlphaNz);
	EXPECT_EQ(render_modes::Changed(Mode::TexturedChroma, k_Faded), Mode::TexturedChromaAlphaNz);
	EXPECT_EQ(render_modes::Changed(Mode::TexturedAlpha, k_Faded), Mode::TexturedAlphaNz);
	EXPECT_EQ(render_modes::Changed(Mode::AlphaTextured, k_Faded), Mode::AlphaTexturedAlphaNz);
	EXPECT_EQ(render_modes::Changed(Mode::AlphaTexturedAlpha, k_Faded), Mode::AlphaTexturedAlphaNz);
	EXPECT_EQ(render_modes::Changed(Mode::AlphaTexturedAlphaAdditive, k_Faded), Mode::AlphaTexturedAlphaAdditiveNz);
	// The modes with no texture keep their mode
	EXPECT_EQ(render_modes::Changed(Mode::Smooth, k_Faded), Mode::Smooth);
}

TEST(ChangedMaterials, WithDepthTheNoDepthModesWriteDepthAgain)
{
	constexpr Properties k_Depth {.additive = false, .writesDepth = true, .bothSides = false, .textureAlpha = true};
	EXPECT_EQ(render_modes::Changed(Mode::AlphaTextured, k_Depth), Mode::AlphaTexturedAlpha);
	EXPECT_EQ(render_modes::Changed(Mode::TexturedAlphaNz, k_Depth), Mode::TexturedAlpha);
	EXPECT_EQ(render_modes::Changed(Mode::TexturedChromaAlphaNz, k_Depth), Mode::TexturedChroma);
	EXPECT_EQ(render_modes::Changed(Mode::AlphaTexturedAlphaAdditiveNz, k_Depth), Mode::AlphaTexturedAlphaAdditive);
	// A solid textured mode stays solid
	EXPECT_EQ(render_modes::Changed(Mode::Textured, k_Depth), Mode::Textured);
}

TEST(ChangedMaterials, AddedOrWithoutTextureAlphaEveryModeTakesOneMode)
{
	constexpr Properties k_Added {.additive = true, .writesDepth = true, .bothSides = false, .textureAlpha = true};
	EXPECT_EQ(render_modes::Changed(Mode::Textured, k_Added), Mode::AlphaTexturedAlphaAdditive);
	EXPECT_EQ(render_modes::Changed(Mode::Smooth, {.additive = true, .writesDepth = false}),
	          Mode::AlphaTexturedAlphaAdditiveNz);
	EXPECT_EQ(render_modes::Changed(Mode::AlphaTextured, {.textureAlpha = false}), Mode::TexturedAlpha);
	EXPECT_EQ(render_modes::Changed(Mode::AlphaTextured, {.writesDepth = false, .textureAlpha = false}), Mode::TexturedAlphaNz);
}

TEST(ChangedMaterials, OnlyTheFirstBitOfTheCullByteChanges)
{
	EXPECT_EQ(render_modes::ChangedCull(0x05, {.bothSides = false}), 0x04);
	EXPECT_EQ(render_modes::ChangedCull(0x04, {.bothSides = true}), 0x05);
	EXPECT_EQ(render_modes::ChangedCull(0x45, {.bothSides = true}), 0x45);
}

TEST(TempleHeartLook, TheHeartIsDrawnAtItsColoursAlphaInWholeSteps)
{
	EXPECT_EQ(DrawnAlpha(255), 254);
	EXPECT_EQ(DrawnAlpha(242), 241);
	EXPECT_EQ(DrawnAlpha(241), 240);
	EXPECT_EQ(DrawnAlpha(0), 0);
}

TEST(TempleHeartLook, TheMaterialsChangeOnlyInTheDestructionAtAnAlphaOf240OrLess)
{
	EXPECT_TRUE(MaterialsChange(true, 240));
	EXPECT_FALSE(MaterialsChange(true, 241));
	EXPECT_FALSE(MaterialsChange(false, 0));
	// The fade starts at 14 seconds, and the drawn alpha comes to 240 between 14.2 and 14.3 seconds
	EXPECT_FALSE(MaterialsChange(true, DrawnAlpha(openblack::temple_destruction::HeartAlpha(14.2f))));
	EXPECT_TRUE(MaterialsChange(true, DrawnAlpha(openblack::temple_destruction::HeartAlpha(14.3f))));
	EXPECT_FALSE(k_Changed.writesDepth);
	EXPECT_FALSE(k_Changed.bothSides);
	EXPECT_FALSE(k_Changed.additive);
	EXPECT_TRUE(k_Changed.textureAlpha);
}

TEST(TempleHeartLook, TheSnowFadesWithTheHeartAndGoesAtHalfItsAlpha)
{
	EXPECT_EQ(FadedSnowLevel(255, 240), 223);
	EXPECT_EQ(FadedSnowLevel(255, 254), 251);
	EXPECT_EQ(FadedSnowLevel(255, 129), 1);
	EXPECT_EQ(FadedSnowLevel(255, 128), 0);
	EXPECT_EQ(FadedSnowLevel(255, 0), 0);
	EXPECT_EQ(FadedSnowLevel(0, 200), 0);
	// A level from the snow can come to more than 255 by the steps, which are kept to 255
	EXPECT_EQ(FadedSnowLevel(300, 254), 255);
}

TEST(TempleHeartLook, TheGlowGrowsFromThreeAndAHalfSecondsBeatingWithTheHeart)
{
	EXPECT_FALSE(Glows(false, 10.0f));
	EXPECT_FALSE(Glows(true, 3.5f));
	EXPECT_TRUE(Glows(true, 3.6f));
	EXPECT_EQ(GlowStrength(3.5f, 1.0f), 10);
	EXPECT_EQ(GlowStrength(6.5f, 1.0f), 60);
	EXPECT_EQ(GlowStrength(20.0f, 1.0f), 60);
	EXPECT_EQ(GlowStrength(5.0f, 0.5f), 22);
	EXPECT_EQ(GlowStrength(10.0f, 0.0f), 10);
	// Each channel of the player's colour by the strength, in 256ths
	EXPECT_EQ(Glow(0xFF4646u, 60), 0x3B1010u);
	EXPECT_EQ(Glow(0xFFFFFFFFu, 256 - 1), 0xFEFEFEu);
}

TEST(TempleHeartLook, TheHeartIsNotDrawnJustBeforeTheTempleGoes)
{
	EXPECT_TRUE(Drawn(false, 30.0f));
	EXPECT_TRUE(Drawn(true, 21.8f));
	EXPECT_FALSE(Drawn(true, 21.9f));
	EXPECT_EQ(k_Colour, 0xFFFFFFu);
}

// The creed's shell covers the heart from 14 seconds into its destruction to its end
TEST(TempleHeartLook, TheShellShowsFromFourteenSeconds)
{
	EXPECT_FALSE(openblack::graphics::temple_heart_look::ShellShown(false, 15.0f));
	EXPECT_FALSE(openblack::graphics::temple_heart_look::ShellShown(true, 13.9f));
	EXPECT_TRUE(openblack::graphics::temple_heart_look::ShellShown(true, 14.0f));
	EXPECT_TRUE(openblack::graphics::temple_heart_look::ShellShown(true, 21.9f));
}

// The shell keeps a size of 0.3 and a reach of 5 until 20 seconds, then grows to 0.6 and 10 at 22 seconds
TEST(TempleHeartLook, TheShellGrowsAtTheEnd)
{
	namespace look = openblack::graphics::temple_heart_look;
	EXPECT_FLOAT_EQ(look::ShellScale(15.0f), 0.3f);
	EXPECT_FLOAT_EQ(look::ShellReach(15.0f), 5.0f);
	EXPECT_FLOAT_EQ(look::ShellScale(21.0f), 0.45f);
	EXPECT_FLOAT_EQ(look::ShellReach(21.0f), 7.5f);
	EXPECT_FLOAT_EQ(look::ShellScale(22.0f), 0.6f);
	EXPECT_FLOAT_EQ(look::ShellReach(30.0f), 10.0f);
	// Before the first second the tables start from nothing
	EXPECT_FLOAT_EQ(look::ShellReach(0.5f), 2.5f);
	EXPECT_FLOAT_EQ(look::ShellScale(0.0f), 0.0f);
}
