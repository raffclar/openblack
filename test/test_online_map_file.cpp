/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstddef>
#include <cstdint>

#include <string_view>
#include <vector>

#include <OnlineMapFile.h>
#include <gtest/gtest.h>

using namespace openblack;

namespace
{
/// The bytes of a ".map" file with this script and this landscape, and lengths that can differ from them
std::vector<std::byte> MapFile(std::string_view script, std::vector<uint8_t> landscape, uint32_t scriptLength,
                               uint32_t landscapeLength)
{
	std::vector<std::byte> bytes;
	for (const auto length : {scriptLength, landscapeLength})
	{
		for (int i = 0; i < 4; ++i)
		{
			bytes.push_back(static_cast<std::byte>((length >> (8 * i)) & 0xFF));
		}
	}
	for (const char c : script)
	{
		bytes.push_back(static_cast<std::byte>(c));
	}
	for (const auto b : landscape)
	{
		bytes.push_back(static_cast<std::byte>(b));
	}
	return bytes;
}
} // namespace

TEST(OnlineMapFile, TheFileGivesItsScriptAndItsLandscape)
{
	const std::string_view script = "VERSION(2.300000)\nq\n";
	const std::vector<uint8_t> landscape {1, 2, 3, 0xFF};
	const auto bytes = MapFile(script, landscape, static_cast<uint32_t>(script.size()), 4);
	const auto file = onlinemap::Parse(bytes);
	ASSERT_TRUE(file.has_value());
	EXPECT_EQ(file->script, script);
	ASSERT_EQ(file->landscape.size(), 4u);
	EXPECT_EQ(file->landscape[3], std::byte {0xFF});
}

TEST(OnlineMapFile, LengthsThatDoNotAgreeWithTheFileGiveNothing)
{
	EXPECT_FALSE(onlinemap::Parse(MapFile("abc", {1}, 3, 2)).has_value());
	EXPECT_FALSE(onlinemap::Parse(MapFile("abc", {1}, 2, 0)).has_value());
	EXPECT_FALSE(onlinemap::Parse(MapFile("abc", {1}, 0xFFFFFFFF, 0xFFFFFFFF)).has_value());
	const std::vector<std::byte> tooShort(7, std::byte {0});
	EXPECT_FALSE(onlinemap::Parse(tooShort).has_value());
	EXPECT_TRUE(onlinemap::Parse(MapFile("", {}, 0, 0)).has_value());
}
