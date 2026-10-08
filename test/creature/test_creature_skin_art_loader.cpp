/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// What creatures' tattoos and marks are painted with, read from raw images in a folder

#include <gtest/gtest.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#include "Creature/CreatureMarks.h"
#include "Creature/CreatureSkin.h"
#include "Creature/CreatureTattoo.h"
#include "FileSystem/FileSystemInterface.h"
#include "Resources/Loaders.h"
#include "creature/SyntheticCreatureBlock.h"
#include "support/TestServices.h"

using namespace openblack;
using namespace openblack::test::creature_block;

namespace
{
constexpr uint32_t k_Size = creature_marks::k_AtlasSize;

/// An atlas of red, green and blue, blank but for the top-left texel of one design's cell, whose blue is `blue`
std::vector<uint8_t> Atlas(uint32_t design, uint8_t blue)
{
	std::vector<uint8_t> bytes(static_cast<size_t>(k_Size) * k_Size * 3, 0);
	const auto left = (design % creature_tattoo::k_DesignsPerRow) * creature_tattoo::k_DesignSize;
	const auto top = (design / creature_tattoo::k_DesignsPerRow) * creature_tattoo::k_DesignSize;
	bytes.at(((static_cast<size_t>(top) * k_Size + left) * 3) + 2) = blue;
	return bytes;
}

class CreatureSkinArtLoaderTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		if (!spdlog::get("game"))
		{
			spdlog::create<spdlog::sinks::null_sink_mt>("game");
		}
		_folder.emplace(std::string("openblack_test_creature_skin_art_") +
		                ::testing::UnitTest::GetInstance()->current_test_info()->name());
		Locator::filesystem::value().SetGamePath(_folder->Path());
		const std::vector<uint8_t> colours(static_cast<size_t>(k_Size) * k_Size * 3, 0x80);
		const std::vector<uint8_t> alpha(static_cast<size_t>(k_Size) * k_Size, 0x40);
		_folder->Write("defaults.raw", Atlas(1, 0xF0));
		_folder->Write("fresh.raw", colours);
		_folder->Write("fresha.raw", alpha);
		_folder->Write("old.raw", colours);
		_folder->Write("olda.raw", alpha);
		_folder->Write("palette.raw",
		               std::vector<uint8_t>(creature_tattoo::k_PaletteColumns * creature_tattoo::k_PaletteRows * 3, 0x11));
	}

	[[nodiscard]] resources::CreatureSkinArtLoader::Paths Paths() const
	{
		const auto& root = _folder->Path();
		return {.symbols = root / "players.raw",
		        .defaultSymbols = root / "defaults.raw",
		        .freshDamage = root / "fresh.raw",
		        .freshDamageAlpha = root / "fresha.raw",
		        .oldDamage = root / "old.raw",
		        .oldDamageAlpha = root / "olda.raw",
		        .palette = root / "palette.raw"};
	}

	[[nodiscard]] static std::shared_ptr<creature_skin::Art> Load(const resources::CreatureSkinArtLoader::Paths& paths)
	{
		return resources::CreatureSkinArtLoader {}(resources::CreatureSkinArtLoader::FromDiskTag {}, paths);
	}

	const test::ScopedDefaultFileSystem _fileSystem;
	std::optional<TempFolder> _folder;
};
} // namespace

TEST_F(CreatureSkinArtLoaderTest, BlankSymbolCellsTakeTheDefaults)
{
	// the players' symbols fill design 0; design 1 is blank there and comes from the shipped symbols
	_folder->Write("players.raw", Atlas(0, 0xA0));
	const auto art = Load(Paths());
	EXPECT_EQ(art->designs[0].front().levels.front(), 0xA);
	EXPECT_EQ(art->designs[1].front().levels.front(), 0xF);
	EXPECT_EQ(art->designs[2].front().levels.front(), 0);
}

TEST_F(CreatureSkinArtLoaderTest, WithoutPlayersSymbolsEveryCellIsTheDefault)
{
	const auto art = Load(Paths());
	EXPECT_EQ(art->designs[0].front().levels.front(), 0);
	EXPECT_EQ(art->designs[1].front().levels.front(), 0xF);
	EXPECT_EQ(art->palette.size(), static_cast<size_t>(creature_tattoo::k_PaletteColumns) * creature_tattoo::k_PaletteRows);
	EXPECT_EQ(art->palette.front(), (std::array<uint8_t, 3> {0x11, 0x11, 0x11}));
	EXPECT_EQ(art->damage.fresh.alpha.size(), static_cast<size_t>(k_Size) * k_Size);
	EXPECT_EQ(art->damage.old.colours.front(), (std::array<uint8_t, 3> {0x80, 0x80, 0x80}));
}

TEST_F(CreatureSkinArtLoaderTest, ARequiredFileOfTheWrongSizeThrows)
{
	_folder->Write("palette.raw", std::vector<uint8_t>(100, 0));
	EXPECT_THROW(static_cast<void>(Load(Paths())), std::runtime_error);
	// the symbols the game ships with are needed too: a one-byte-a-pixel image is not them
	_folder->Write("palette.raw",
	               std::vector<uint8_t>(creature_tattoo::k_PaletteColumns * creature_tattoo::k_PaletteRows * 3, 0x11));
	_folder->Write("defaults.raw", std::vector<uint8_t>(static_cast<size_t>(k_Size) * k_Size, 0));
	EXPECT_THROW(static_cast<void>(Load(Paths())), std::runtime_error);
}
