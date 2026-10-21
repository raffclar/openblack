/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// A villager uses the row of the villager table that the game made it from, not the last row of its tribe and work

#include <optional>
#include <vector>

#include <gtest/gtest.h>

#include "ECS/Components/Villager.h"
#include "Enums.h"
#include "InfoConstants.h"

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

TEST(VillagerRow, TheSearchByTribeAndWorkGivesTheFirstRow)
{
	// Two tribe rows, then three special rows with no tribe and the same kind of work
	std::vector<GVillagerInfo> rows(5);
	rows.at(0).tribeType = Tribe::CELTIC;
	rows.at(0).villagerNumber = VillagerNumber::Housewife;
	rows.at(1).tribeType = Tribe::CELTIC;
	rows.at(1).villagerNumber = VillagerNumber::Forester;
	for (size_t i = 2; i < rows.size(); ++i)
	{
		rows.at(i).tribeType = Tribe::NONE;
		rows.at(i).villagerNumber = VillagerNumber::Forester;
	}
	EXPECT_EQ(GVillagerInfo::FindIn(rows, Tribe::CELTIC, VillagerNumber::Forester), static_cast<VillagerInfo>(1));
	EXPECT_EQ(GVillagerInfo::FindIn(rows, Tribe::NONE, VillagerNumber::Forester), static_cast<VillagerInfo>(2));
	EXPECT_EQ(GVillagerInfo::FindIn(rows, Tribe::NORSE, VillagerNumber::Forester), std::nullopt);
}

TEST(VillagerRow, AVillagerThatChangesTribeUsesTheRowOfItsNewTribe)
{
	Villager villager {};
	villager.tribe = Tribe::CELTIC;
	villager.number = VillagerNumber::Fisherman;
	villager.kind = VillagerInfo::CelticFishermanMale;
	TakeRow(villager, Tribe::NORSE, VillagerInfo::NorseFishermanMale);
	EXPECT_EQ(villager.tribe, Tribe::NORSE);
	EXPECT_EQ(RowOf(villager, LastSpecialRow), VillagerInfo::NorseFishermanMale);
}
