/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <array>
#include <vector>

#include <gtest/gtest.h>

#include "Creature/CreatureMarks.h"
#include "Creature/CreatureSkin.h"

using namespace openblack;
using namespace openblack::creature_marks;

namespace
{
constexpr size_t k_SkinTexels = 256 * 256;

/// Damage atlases of one colour and alpha each
DamageArt FakeArt(std::array<uint8_t, 3> fresh, uint8_t freshAlpha, std::array<uint8_t, 3> old, uint8_t oldAlpha)
{
	const auto texels = static_cast<size_t>(k_AtlasSize) * k_AtlasSize;
	return {
	    .fresh = {.colours = std::vector(texels, fresh), .alpha = std::vector(texels, freshAlpha)},
	    .old = {.colours = std::vector(texels, old), .alpha = std::vector(texels, oldAlpha)},
	};
}
} // namespace

TEST(CreatureMarks, AFullListLosesItsOldestMark)
{
	std::vector<Mark> marks;
	for (size_t i = 0; i < k_MaxMarks; ++i)
	{
		Add(marks, {.u = 0, .v = 0, .skin = 0, .age = static_cast<uint8_t>(i == 10 ? 50 : 3), .type = 0, .column = 0});
	}
	ASSERT_EQ(marks.size(), k_MaxMarks);
	const Mark fresh {.u = 9, .v = 9, .skin = 1, .age = 0, .type = 3, .column = 2};
	Add(marks, fresh);
	EXPECT_EQ(marks.size(), k_MaxMarks);
	EXPECT_EQ(marks.at(10), fresh);
}

TEST(CreatureMarks, MarksAgeEvery600Counts)
{
	Marks marks;
	Add(marks.wounds, {.u = 0, .v = 0, .skin = 0, .age = 0, .type = 3, .column = 0});
	Add(marks.blood, {.u = 0, .v = 0, .skin = 0, .age = 0, .type = 0, .column = 0});
	EXPECT_FALSE(Heal(marks, 599));
	EXPECT_EQ(marks.wounds[0].age, 0);
	EXPECT_TRUE(Heal(marks, 1));
	EXPECT_EQ(marks.wounds[0].age, 1);
	EXPECT_EQ(marks.blood[0].age, 1);
	EXPECT_EQ(marks.counts, 0u);
	// A heal effect's counts age them many steps at once
	EXPECT_TRUE(Heal(marks, k_CountsPerStep * 10 + 5));
	EXPECT_EQ(marks.wounds[0].age, 11);
	EXPECT_EQ(marks.counts, 5u);
}

TEST(CreatureMarks, MarksGoOnceTheirKindHasLasted)
{
	Marks marks;
	// A wound of the first kind lasts 32 steps, the last kind one, the others 64; blood 63
	Add(marks.wounds, {.u = 0, .v = 0, .skin = 0, .age = 31, .type = 0, .column = 0});
	Add(marks.wounds, {.u = 0, .v = 0, .skin = 0, .age = 31, .type = 3, .column = 0});
	Add(marks.wounds, {.u = 0, .v = 0, .skin = 0, .age = 0, .type = 7, .column = 0});
	Add(marks.blood, {.u = 0, .v = 0, .skin = 0, .age = 62, .type = 0, .column = 0});
	Add(marks.blood, {.u = 0, .v = 0, .skin = 0, .age = 61, .type = 0, .column = 0});
	Heal(marks, k_CountsPerStep);
	ASSERT_EQ(marks.wounds.size(), 1u);
	EXPECT_EQ(marks.wounds[0].type, 3);
	ASSERT_EQ(marks.blood.size(), 1u);
	EXPECT_EQ(marks.blood[0].age, 62);
}

TEST(CreatureMarks, WoundsFadeFromFreshToOld)
{
	const auto art = FakeArt({200, 0, 0}, 255, {0, 100, 0}, 55);
	const Mark fresh {.u = 0, .v = 0, .skin = 0, .age = 0, .type = 3, .column = 0};
	EXPECT_EQ(WoundTexel(art, fresh, 0, 0), glm::u8vec4(200, 0, 0, 255));
	// Half way through its 64 steps: half way between, in 256ths
	auto half = fresh;
	half.age = 32;
	EXPECT_EQ(WoundTexel(art, half, 0, 0), glm::u8vec4(100, 50, 0, 155));
	auto old = fresh;
	old.age = 64;
	EXPECT_EQ(WoundTexel(art, old, 0, 0), glm::u8vec4(0, 100, 0, 55));
}

TEST(CreatureMarks, ColoursBlendAt8BitsAndCutTo4)
{
	EXPECT_EQ(BlendTexel(0x0123, {0xFF, 0xFF, 0xFF}, 255), 0xFFFF);
	EXPECT_EQ(BlendTexel(0x0123, {0xFF, 0xFF, 0xFF}, 0), 0xF123);
	// Blue: (0x30 * 128 + 0xC0 * 127) / 255 = 119.8, so 119 >> 4 = 7
	EXPECT_EQ(BlendTexel(0x0003, {0, 0, 0xC0}, 127) & 0xF, 7);
}

TEST(CreatureMarks, WoundsAreCentredAndCutAtTheEdges)
{
	std::vector<uint16_t> skin(k_SkinTexels, 0x0000);
	const auto art = FakeArt({0xFF, 0xFF, 0xFF}, 255, {0xFF, 0xFF, 0xFF}, 255);
	PaintWound(skin, 256, art, {.u = 4, .v = 100, .skin = 0, .age = 0, .type = 3, .column = 1});
	const auto at = [&skin](int x, int y) { return skin.at(static_cast<size_t>((y * 256) + x)); };
	// 32 texels square about its place, the part off the left edge left out
	EXPECT_EQ(at(0, 84), 0xFFFF);
	EXPECT_EQ(at(19, 115), 0xFFFF);
	EXPECT_EQ(at(20, 100), 0x0000);
	EXPECT_EQ(at(5, 83), 0x0000);
	EXPECT_EQ(at(255, 100), 0x0000);
}

TEST(CreatureMarks, AWoundPaintsEachTexelOfItsPictureAlsoWhereItsAlphaIsNone)
{
	// The colour stays as it is and the texel becomes opaque
	std::vector<uint16_t> skin(k_SkinTexels, 0x0123);
	const auto art = FakeArt({0xFF, 0xFF, 0xFF}, 0, {0xFF, 0xFF, 0xFF}, 0);
	PaintWound(skin, 256, art, {.u = 100, .v = 100, .skin = 0, .age = 0, .type = 3, .column = 0});
	EXPECT_EQ(skin.at((100 * 256) + 100), 0xF123);
	EXPECT_EQ(skin.at((100 * 256) + 120), 0x0123);
}

TEST(CreatureMarks, OnAHalfSizeSkinAWoundIsAtHalfItsPlaceAndTakesTheSecondTexelOfEachSquare)
{
	constexpr size_t k_HalfTexels = 128 * 128;
	std::vector<uint16_t> skin(k_HalfTexels, 0x0000);
	// The fresh picture is white in its odd rows and columns and black in the rest
	auto art = FakeArt({0, 0, 0}, 255, {0, 0, 0}, 255);
	for (uint32_t y = 0; y < k_AtlasSize; ++y)
	{
		for (uint32_t x = 0; x < k_AtlasSize; ++x)
		{
			if ((x % 2) == 1 && (y % 2) == 1)
			{
				art.fresh.colours.at((static_cast<size_t>(y) * k_AtlasSize) + x) = {0xFF, 0xFF, 0xFF};
			}
		}
	}
	PaintWound(skin, 128, art, {.u = 101, .v = 41, .skin = 0, .age = 0, .type = 3, .column = 1});
	const auto at = [&skin](int x, int y) { return skin.at(static_cast<size_t>((y * 128) + x)); };
	// 16 texels square about (50, 20), every one from an odd texel of the picture
	EXPECT_EQ(at(42, 12), 0xFFFF);
	EXPECT_EQ(at(57, 27), 0xFFFF);
	EXPECT_EQ(at(41, 12), 0x0000);
	EXPECT_EQ(at(58, 27), 0x0000);
	EXPECT_EQ(at(50, 28), 0x0000);
	// A wound on a skin of another size paints nothing
	std::vector<uint16_t> other(64 * 64, 0x0000);
	PaintWound(other, 64, art, {.u = 32, .v = 32, .skin = 0, .age = 0, .type = 3, .column = 1});
	EXPECT_EQ(other.at((32 * 64) + 32), 0x0000);
}

TEST(CreatureMarks, OnAHalfSizeSkinBloodIsAtHalfItsPlace)
{
	std::vector<uint16_t> skin(128 * 128, 0x0FFF);
	PaintBlood(skin, 128, {.u = 7, .v = 5, .skin = 0, .age = 0, .type = 0, .column = 0});
	EXPECT_EQ(skin.at((2 * 128) + 3), BloodTexel(0x0FFF, {150, 0, 20}));
	EXPECT_EQ(skin.at((5 * 128) + 7), 0x0FFF);
}

TEST(CreatureMarks, BloodDarkensAsItDries)
{
	EXPECT_EQ(BloodColour(0), glm::u8vec3(150, 0, 20));
	EXPECT_EQ(BloodColour(21), glm::u8vec3(150, 0, 20));
	EXPECT_EQ(BloodColour(22), glm::u8vec3(120, 20, 20));
	EXPECT_EQ(BloodColour(43), glm::u8vec3(80, 40, 0));
	std::vector<uint16_t> skin(k_SkinTexels, 0x0FFF);
	PaintBlood(skin, 256, {.u = 3, .v = 2, .skin = 0, .age = 0, .type = 0, .column = 0});
	// Three quarters red over white
	EXPECT_EQ(skin.at((2 * 256) + 3), BloodTexel(0x0FFF, {150, 0, 20}));
	EXPECT_EQ(skin.at((2 * 256) + 4), 0x0FFF);
}

TEST(CreatureMarks, BloodCoversThreeQuartersRoundedDown)
{
	// White under fresh blood: (240 + 3 x 150) / 4 = 172 -> 10; (240 + 0) / 4 = 60 -> 3; (240 + 60) / 4 = 75 -> 4
	EXPECT_EQ(BloodTexel(0x0FFF, {150, 0, 20}), 0xFA34);
	// A quarter of 16 and three quarters of 80 make 64 exactly, which keeps 4 and not 3
	EXPECT_EQ(BloodTexel(0x0111, {80, 80, 80}), 0xF444);
	// Black under the oldest blood: 60 -> 3, 30 -> 1, 0 -> 0
	EXPECT_EQ(BloodTexel(0x0000, {80, 40, 0}), 0xF310);
}

TEST(CreatureMarks, SkinsArePaintedTattoosThenWoundsThenBlood)
{
	using namespace openblack::creature_skin;
	auto art = std::make_unique<Art>();
	art->damage = FakeArt({0, 0, 0}, 255, {0, 0, 0}, 255);
	for (size_t d = 0; d < art->designs.size(); ++d)
	{
		for (size_t level = 0; level < creature_tattoo::k_MaskLevels; ++level)
		{
			const auto size = creature_tattoo::k_DesignSize >> level;
			art->designs.at(d).at(level) = {.size = size, .levels = std::vector<uint8_t>(static_cast<size_t>(size) * size, 15)};
		}
	}
	creature_tattoo::Slots tattoos {};
	tattoos[0] = {.design = 1, .site = 0, .colour = {0xFF, 0xFF, 0xFF}};
	std::optional<creature_tattoo::Sites> sites = creature_tattoo::Sites {};
	sites->at(0) = {.enabled = true, .u = 128, .v = 128, .skin = 0, .size = 1.0f, .mirror = false, .rotation = 0};
	Marks marks;
	Add(marks.wounds, {.u = 128, .v = 128, .skin = 0, .age = 0, .type = 3, .column = 0});
	Add(marks.blood, {.u = 128, .v = 128, .skin = 0, .age = 0, .type = 0, .column = 0});
	// A wound on another skin isn't painted on this one
	Add(marks.wounds, {.u = 10, .v = 10, .skin = 1, .age = 0, .type = 3, .column = 0});

	creature_tattoo::Masks masks {};
	masks[0] = art->designs.at(1);

	const std::vector<uint16_t> base(k_SkinTexels, 0x0000);
	const std::vector<uint16_t> variant(k_SkinTexels, 0x0FFF);
	std::vector<uint16_t> out(k_SkinTexels);
	Compose(out, base, variant, 255,
	        {.skinIndex = 0, .tattoos = tattoos, .sites = sites, .marks = marks, .art = art.get(), .masks = &masks});
	const auto at = [&out](int x, int y) { return out.at(static_cast<size_t>((y * 256) + x)); };
	// The variant at full weight, white; a white tattoo over the middle; a black wound over that; blood on top
	EXPECT_EQ(at(5, 5), 0x0FFF);
	EXPECT_EQ(at(100, 100), 0xFFFF);
	EXPECT_EQ(at(120, 120), 0xF000);
	EXPECT_EQ(at(128, 128), BloodTexel(0xF000, BloodColour(0)));
	EXPECT_EQ(at(10, 10), 0x0FFF);

	// Without the art, the blend alone
	Compose(out, base, variant, 0,
	        {.skinIndex = 0, .tattoos = tattoos, .sites = sites, .marks = marks, .art = nullptr, .masks = &masks});
	EXPECT_EQ(at(128, 128), 0x0000);
}

TEST(CreatureMarks, AHalfSizeSkinIsPaintedAtHalfThePlacesAndSizes)
{
	using namespace openblack::creature_skin;
	auto art = std::make_unique<Art>();
	art->damage = FakeArt({0, 0, 0}, 255, {0, 0, 0}, 255);
	for (auto& design : art->designs)
	{
		for (size_t level = 0; level < creature_tattoo::k_MaskLevels; ++level)
		{
			const auto size = creature_tattoo::k_DesignSize >> level;
			design.at(level) = {.size = size, .levels = std::vector<uint8_t>(static_cast<size_t>(size) * size, 15)};
		}
	}
	creature_tattoo::Slots tattoos {};
	tattoos[0] = {.design = 1, .site = 0, .colour = {0xFF, 0xFF, 0xFF}};
	std::optional<creature_tattoo::Sites> sites = creature_tattoo::Sites {};
	sites->at(0) = {.enabled = true, .u = 128, .v = 128, .skin = 0, .size = 1.0f, .mirror = false, .rotation = 0};
	Marks marks;
	Add(marks.wounds, {.u = 128, .v = 128, .skin = 0, .age = 0, .type = 3, .column = 0});
	Add(marks.blood, {.u = 128, .v = 128, .skin = 0, .age = 0, .type = 0, .column = 0});
	creature_tattoo::Masks masks {};
	masks[0] = art->designs.at(1);

	constexpr size_t k_HalfTexels = 128 * 128;
	const std::vector<uint16_t> base(k_HalfTexels, 0x0000);
	const std::vector<uint16_t> variant(k_HalfTexels, 0x0FFF);
	std::vector<uint16_t> out(k_HalfTexels);
	Compose(out, base, variant, 255,
	        {.skinIndex = 0, .tattoos = tattoos, .sites = sites, .marks = marks, .art = art.get(), .masks = &masks});
	const auto at = [&out](int x, int y) { return out.at(static_cast<size_t>((y * 128) + x)); };
	// The tattoo is 32 texels square about (64, 64), the wound 16 texels square, the blood one texel at the middle
	EXPECT_EQ(at(48, 48), 0xFFFF);
	EXPECT_EQ(at(47, 64), 0x0FFF);
	EXPECT_EQ(at(79, 79), 0xFFFF);
	EXPECT_EQ(at(80, 64), 0x0FFF);
	EXPECT_EQ(at(56, 56), 0xF000);
	EXPECT_EQ(at(55, 56), 0xFFFF);
	EXPECT_EQ(at(64, 64), BloodTexel(0xF000, BloodColour(0)));
}

TEST(CreatureMarks, EachSlotPaintsWithItsMaskAndALaterSlotPaintsOver)
{
	using namespace openblack::creature_skin;
	auto art = std::make_unique<Art>();
	art->damage = FakeArt({0, 0, 0}, 255, {0, 0, 0}, 255);
	const auto solid = [](uint8_t level) {
		creature_tattoo::Design design;
		for (size_t i = 0; i < creature_tattoo::k_MaskLevels; ++i)
		{
			const auto size = creature_tattoo::k_DesignSize >> i;
			design.at(i) = {.size = size, .levels = std::vector<uint8_t>(static_cast<size_t>(size) * size, level)};
		}
		return design;
	};
	std::optional<creature_tattoo::Sites> sites = creature_tattoo::Sites {};
	sites->at(0) = {.enabled = true, .u = 128, .v = 128, .skin = 0, .size = 1.0f, .mirror = false, .rotation = 0};
	sites->at(1) = {.enabled = true, .u = 140, .v = 128, .skin = 0, .size = 1.0f, .mirror = false, .rotation = 0};
	creature_tattoo::Slots tattoos {};
	// Slot 3 on site 0 in red, then slot 5 on site 1 in blue, which overlaps it
	tattoos[3] = {.design = 0, .site = 0, .colour = {0xFF, 0x00, 0x00}};
	tattoos[5] = {.design = 0, .site = 1, .colour = {0x00, 0x00, 0xFF}};
	// Slot 6 names a design but has no mask
	tattoos[6] = {.design = 2, .site = 0, .colour = {0x00, 0xFF, 0x00}};
	creature_tattoo::Masks masks {};
	// The slots paint with their masks, whatever design they name
	masks[3] = solid(15);
	masks[5] = solid(15);
	const Marks marks;
	const std::vector<uint16_t> base(k_SkinTexels, 0x0000);
	std::vector<uint16_t> out(k_SkinTexels);
	Compose(out, base, {}, 0,
	        {.skinIndex = 0, .tattoos = tattoos, .sites = sites, .marks = marks, .art = art.get(), .masks = &masks});
	const auto at = [&out](int x, int y) { return out.at(static_cast<size_t>((y * 256) + x)); };
	EXPECT_EQ(at(100, 128), 0xFF00);
	// Where they overlap, the later slot shows
	EXPECT_EQ(at(130, 128), 0xF00F);
	EXPECT_EQ(at(170, 128), 0xF00F);
	// Without masks, no tattoos
	Compose(out, base, {}, 0,
	        {.skinIndex = 0, .tattoos = tattoos, .sites = sites, .marks = marks, .art = art.get(), .masks = nullptr});
	EXPECT_EQ(at(130, 128), 0x0000);
}

TEST(CreatureScars, AHardBlowCutsDeepAndFurtherAlongItsRow)
{
	using namespace openblack::creature_marks::scar;
	EXPECT_EQ(BlowKind(0.49f), k_Cut);
	EXPECT_EQ(BlowKind(0.5f), k_DeepCut);
	// The weakest blow lands further back in the row, the hardest from the draw alone
	EXPECT_EQ(Column(0, 0.0f), 5);
	EXPECT_EQ(Column(4, 0.0f), 7);
	EXPECT_EQ(Column(0, 1.0f), 0);
	EXPECT_EQ(Column(4, 1.0f), 2);
}

TEST(CreatureScars, ABurnScarsOneTimeInThreeAndMoreRarelyOnAMarkedBody)
{
	using namespace openblack::creature_marks::scar;
	EXPECT_EQ(BurnKind(0), k_Burn);
	EXPECT_EQ(BurnKind(1), k_Cut);
	EXPECT_EQ(BurnChance(0), 10u);
	EXPECT_EQ(BurnChance(1024), 150u);
	EXPECT_EQ(BurnChance(512), 80u);
}

TEST(CreatureScars, CatchingFireBurnsInThreeTriesAnywhereAlongTheRow)
{
	using namespace openblack::creature_marks::scar;
	EXPECT_EQ(k_CatchingBurnTries, 3);
	EXPECT_EQ(CatchingBurnColumn(0), 0u);
	EXPECT_EQ(CatchingBurnColumn(7), 7u);
	EXPECT_EQ(CatchingBurnColumn(9), 7u);
}

TEST(CreatureScars, TheTexelIsTheTrianglesTextureAtThePoint)
{
	using namespace openblack::creature_marks::scar;
	const std::array<glm::vec2, 3> uvs {glm::vec2(0.0f, 0.0f), glm::vec2(1.0f, 0.0f), glm::vec2(0.0f, 0.5f)};
	EXPECT_EQ(TexelAt(uvs, 0.5f, 0.5f), glm::u8vec2(128, 64));
	EXPECT_EQ(TexelAt(uvs, 1.5f, 0.0f), glm::u8vec2(255, 0));
	// A point off the texture's near edge wraps round to its far edge
	EXPECT_EQ(TexelAt(uvs, -0.5f, 0.0f), glm::u8vec2(255, 0));
	// Only exactly nothing stays at nothing
	EXPECT_EQ(TexelAt(uvs, -0.001f, 0.0f), glm::u8vec2(0, 0));
}
