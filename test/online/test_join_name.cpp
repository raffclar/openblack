/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The name that the developer option --join-name gives to a join on the local network

#include <optional>
#include <string>

#include <gtest/gtest.h>

#include "Online/JoinName.h"
#include "Server/LockstepHost.h"

using namespace openblack::online;

TEST(JoinName, AbsentOptionGivesNoName)
{
	const auto parsed = join_name::Parse(std::nullopt);
	EXPECT_TRUE(parsed.valid);
	EXPECT_FALSE(parsed.name.has_value());
}

TEST(JoinName, NormalNameIsKept)
{
	const auto parsed = join_name::Parse(std::string("Player 1"));
	EXPECT_TRUE(parsed.valid);
	ASSERT_TRUE(parsed.name.has_value());
	EXPECT_EQ(*parsed.name, "Player 1");
}

TEST(JoinName, EmptyNameIsNotValid)
{
	const auto parsed = join_name::Parse(std::string());
	EXPECT_FALSE(parsed.valid);
	EXPECT_FALSE(parsed.name.has_value());
}

TEST(JoinName, ControlCharacterIsNotValid)
{
	EXPECT_FALSE(join_name::Check(std::string("a\tb")).has_value());
	EXPECT_FALSE(join_name::Check(std::string("a\nb")).has_value());
	EXPECT_FALSE(join_name::Check(std::string("a\x7F")).has_value());
}

TEST(JoinName, LengthLimitIsExact)
{
	EXPECT_TRUE(join_name::Check(std::string(join_name::k_MostBytes, 'a')).has_value());
	EXPECT_FALSE(join_name::Check(std::string(join_name::k_MostBytes + 1, 'a')).has_value());
}

TEST(JoinName, SameLimitAsTheServer)
{
	// The server and this check must agree, or a name would pass here and be refused there
	EXPECT_EQ(join_name::k_MostBytes, openblack::server::k_MostLanNameBytes);
}
