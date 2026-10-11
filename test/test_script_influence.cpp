/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <array>
#include <optional>

#include <gtest/gtest.h>

#include "ScriptHeaders/ScriptInfluence.h"

using namespace openblack::script::influence;

TEST(ScriptInfluence, APlayerNotInTheGameHasNone)
{
	constexpr std::array k_Allies {1.0f};
	EXPECT_FLOAT_EQ(Answer(std::nullopt, false, k_Allies), 0.0f);
	EXPECT_FLOAT_EQ(Answer(std::nullopt, true, k_Allies), 0.0f);
}

TEST(ScriptInfluence, ThePlayersOwnInfluenceIsTheAnswerWhereTheyHaveSome)
{
	constexpr std::array k_Allies {0.5f};
	EXPECT_FLOAT_EQ(Answer(30.0f, false, k_Allies), 30.0f);
	EXPECT_FLOAT_EQ(Answer(30.0f, true, k_Allies), 30.0f);
	// What their hand keeps past the border counts as theirs
	EXPECT_FLOAT_EQ(Answer(0.3f, false, {}), 0.3f);
}

TEST(ScriptInfluence, WhereThePlayerHasNoneTheFirstAllyWithSomeLendsTheirs)
{
	constexpr std::array k_Allies {0.0f, 25.0f, 40.0f};
	EXPECT_FLOAT_EQ(Answer(0.0f, false, k_Allies), 25.0f);
	// Without allies there, none
	constexpr std::array k_None {0.0f, 0.0f};
	EXPECT_FLOAT_EQ(Answer(0.0f, false, k_None), 0.0f);
	EXPECT_FLOAT_EQ(Answer(0.0f, false, {}), 0.0f);
}

TEST(ScriptInfluence, TheRawInfluenceIsThePlayersOwnOnly)
{
	constexpr std::array k_Allies {25.0f};
	EXPECT_FLOAT_EQ(Answer(0.0f, true, k_Allies), 0.0f);
	EXPECT_FLOAT_EQ(Answer(-2.0f, true, k_Allies), -2.0f);
}

TEST(ScriptInfluence, BelowNoneWithoutAnAllyIsAnsweredAsNone)
{
	EXPECT_FLOAT_EQ(Answer(-2.0f, false, {}), 0.0f);
}
