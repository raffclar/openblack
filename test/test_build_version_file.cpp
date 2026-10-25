/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <string>
#include <string_view>

#include <gtest/gtest.h>

#include "Common/BuildVersionFile.h"

using namespace openblack::build_version;

namespace
{
constexpr std::string_view k_Hash = "4f4c9db94a1b2c3d4e5f60718293a4b5c6d7e8f9";
} // namespace

TEST(BuildVersion, ReadsTheFullHashOfTheFile)
{
	const auto version = Parse(std::string(k_Hash) + "\n");
	ASSERT_TRUE(version.Known());
	EXPECT_FALSE(version.dirty);
	EXPECT_EQ(version.Text(), k_Hash);
	EXPECT_EQ(version.Short(), "4f4c9db");
	EXPECT_EQ(version.CommitAddress(), "https://github.com/raffclar/openblack/commit/" + std::string(k_Hash));
}

TEST(BuildVersion, KeepsTheChangesMarkOfTheTree)
{
	const auto version = Parse("  " + std::string(k_Hash) + "-dirty\r\n");
	ASSERT_TRUE(version.Known());
	EXPECT_TRUE(version.dirty);
	EXPECT_EQ(version.Text(), std::string(k_Hash) + "-dirty");
	EXPECT_EQ(version.Short(), "4f4c9db-dirty");
	// The address is of the commit, without the mark
	EXPECT_EQ(version.CommitAddress(), "https://github.com/raffclar/openblack/commit/" + std::string(k_Hash));
}

TEST(BuildVersion, UpperCaseHashGivesTheSameVersion)
{
	EXPECT_EQ(Parse("4F4C9DB94A1B2C3D4E5F60718293A4B5C6D7E8F9").Text(), k_Hash);
}

TEST(BuildVersion, OtherTextIsAnUnknownVersion)
{
	for (const std::string_view text : {"", "\n", "unknown\n", "4f4c9db", "4f4c9db94a1b2c3d4e5f60718293a4b5c6d7e8f9a",
	                                    "zf4c9db94a1b2c3d4e5f60718293a4b5c6d7e8f9", "-dirty"})
	{
		const auto version = Parse(text);
		EXPECT_FALSE(version.Known()) << text;
		EXPECT_EQ(version.Text(), "unknown") << text;
		EXPECT_EQ(version.Short(), "unknown") << text;
		EXPECT_FALSE(version.CommitAddress().has_value()) << text;
	}
}

TEST(BuildVersion, NoVersionIsUnknown)
{
	const BuildVersion version;
	EXPECT_EQ(version.Short(), "unknown");
}
