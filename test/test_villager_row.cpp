/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// A villager uses the row of the villager table that the game made it from, not the last row of its tribe and work

#include <gtest/gtest.h>

#include "ECS/Components/Villager.h"
#include "Enums.h"

using namespace openblack;
using namespace openblack::ecs::components;

namespace
{

/// A finder that gives the last row of the special villagers for every villager of no tribe, as the table does
VillagerInfo LastSpecialRow(Tribe tribe, VillagerNumber /*number*/)
{
	return tribe == Tribe::NONE ? VillagerInfo::SailorAccordian : VillagerInfo::NorseHousewifeFemale;
}

Villager Special(VillagerInfo kind)
{
	Villager villager {};
	villager.tribe = Tribe::NONE;
	villager.number = VillagerNumber::Forester;
	villager.kind = kind;
	return villager;
}

} // namespace

TEST(VillagerRow, TheRowTheVillagerWasMadeFromWins)
{
	EXPECT_EQ(RowOf(Special(VillagerInfo::PiedPiper), LastSpecialRow), VillagerInfo::PiedPiper);
	EXPECT_EQ(RowOf(Special(VillagerInfo::Hermit), LastSpecialRow), VillagerInfo::Hermit);
}

TEST(VillagerRow, AVillagerWithNoKeptRowUsesItsTribeAndWork)
{
	EXPECT_EQ(RowOf(Special(VillagerInfo::None), LastSpecialRow), VillagerInfo::SailorAccordian);
	Villager norse {};
	norse.tribe = Tribe::NORSE;
	norse.number = VillagerNumber::Housewife;
	EXPECT_EQ(RowOf(norse, LastSpecialRow), VillagerInfo::NorseHousewifeFemale);
}
