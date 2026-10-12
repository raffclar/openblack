/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <optional>
#include <string>

#include <gtest/gtest.h>

#include "Creature/CreatureNames.h"

using namespace openblack;
using creature_names::Owner;

namespace
{
// The four creature names of the help texts, by number; any other number is named by its number
std::u16string FakeHelpWords(uint32_t text)
{
	switch (text)
	{
	case 5008:
		return u"Rootle";
	case 5009:
		return u"Chugworth";
	case 5010:
		return u"Samson";
	case 5011:
		return u"Brinsley";
	default:
		return u"text " + std::u16string(1, static_cast<char16_t>(u'0' + text % 10));
	}
}
} // namespace

TEST(CreatureNames, ThePlayersCreatureTakesTheProfilesCreatureName)
{
	EXPECT_EQ(creature_names::NewName(Owner::Player, 1, u"Bob", FakeHelpWords), u"Bob");
	EXPECT_EQ(creature_names::NewName(Owner::Player, 6, u"", FakeHelpWords), u"");
	// Without a profile it has no name
	EXPECT_EQ(creature_names::NewName(Owner::Player, 1, std::nullopt, FakeHelpWords), u"");
}

TEST(CreatureNames, OtherCreaturesTakeOneOfFourNamesByHowManyCreaturesThereAre)
{
	// The count takes in the new creature: the first one made is the second name
	EXPECT_EQ(creature_names::NewName(Owner::Nobody, 1, u"Bob", FakeHelpWords), u"Chugworth");
	EXPECT_EQ(creature_names::NewName(Owner::Nobody, 2, u"Bob", FakeHelpWords), u"Samson");
	EXPECT_EQ(creature_names::NewName(Owner::Computer, 3, std::nullopt, FakeHelpWords), u"Brinsley");
	EXPECT_EQ(creature_names::NewName(Owner::Computer, 4, std::nullopt, FakeHelpWords), u"Rootle");
	EXPECT_EQ(creature_names::NewName(Owner::Nobody, 9, std::nullopt, FakeHelpWords), u"Chugworth");
	static_assert(creature_names::StrayNameText(0) == 5008);
	static_assert(creature_names::StrayNameText(7) == 5011);
}

TEST(CreatureNames, AllButAComputerPlayersCreatureTakeTheNameItsMindFileKeeps)
{
	static_assert(creature_names::TakesNameFromFile(Owner::Player));
	static_assert(creature_names::TakesNameFromFile(Owner::Nobody));
	static_assert(!creature_names::TakesNameFromFile(Owner::Computer));
}
