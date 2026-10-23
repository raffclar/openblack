/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <algorithm>
#include <string>

#include <gtest/gtest.h>

#include "Common/BuildVersion.h"

using namespace openblack;

TEST(BuildVersion, TheTextIsTheSemanticVersionAndTheCommit)
{
	const auto text = std::string(build_version::Text());
	EXPECT_EQ(text, std::string(build_version::Semantic()) + "+" + std::string(build_version::Commit()));
}

TEST(BuildVersion, TheSemanticVersionHasThreeNumbers)
{
	const auto semantic = build_version::Semantic();
	EXPECT_EQ(std::ranges::count(semantic, '.'), 2);
	EXPECT_TRUE(std::ranges::all_of(semantic, [](char c) { return c == '.' || (c >= '0' && c <= '9'); }));
	EXPECT_NE(semantic.front(), '.');
	EXPECT_NE(semantic.back(), '.');
}

TEST(BuildVersion, TheCommitIsTwelveHexDigitsOrUnknown)
{
	const auto commit = build_version::Commit();
	if (commit == "unknown")
	{
		return;
	}
	EXPECT_EQ(commit.size(), 12u);
	EXPECT_TRUE(std::ranges::all_of(commit, [](char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'); }));
}
