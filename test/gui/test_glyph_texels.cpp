/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <filesystem>
#include <vector>

#include <gtest/gtest.h>

#include "Gui/GlyphTexture.h"
#include "Resources/Rereads.h"

using namespace openblack;

TEST(GlyphTexels, EachGlyphTexelIsWhiteWithItsCoverageInAlpha)
{
	const std::vector<uint8_t> coverage {0x00, 0x80, 0xFF};
	const auto texels = gui::GlyphTexels(coverage);
	EXPECT_EQ(texels, (std::vector<uint8_t> {0xFF, 0xFF, 0xFF, 0x00, 0xFF, 0xFF, 0xFF, 0x80, 0xFF, 0xFF, 0xFF, 0xFF}));
}

TEST(GlyphTexels, AFontSourceReadsTheFontFilesAgainAndGivesNoTexelsWhenTheyAreMissing)
{
	std::vector<std::filesystem::path> reads;
	resources::Rereads rereads([&reads](const std::filesystem::path& path) {
		reads.push_back(path);
		return std::vector<uint8_t> {};
	});
	const auto source = gui::FontGlyphSource("j0.met", "j0.fnt", false);
	EXPECT_TRUE(source(rereads).empty());
	EXPECT_EQ(reads, (std::vector<std::filesystem::path> {"j0.met", "j0.fnt"}));
}
