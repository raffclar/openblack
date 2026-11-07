/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <array>

#include <gtest/gtest.h>

#include "ECS/MainStoragePitRules.h"

using namespace openblack;
using namespace openblack::ecs::main_storage_pit;

TEST(MainStoragePit, EveryCivicBuildingCanTakeOverButNoHomeOrWonder)
{
	EXPECT_TRUE(CanTakeOver(AbodeType::StoragePit));
	EXPECT_TRUE(CanTakeOver(AbodeType::Civic));
	EXPECT_TRUE(CanTakeOver(AbodeType::Creche));
	EXPECT_TRUE(CanTakeOver(AbodeType::Workshop));
	EXPECT_TRUE(CanTakeOver(AbodeType::TownCentre));
	EXPECT_TRUE(CanTakeOver(AbodeType::Totem));
	EXPECT_TRUE(CanTakeOver(AbodeType::SpellDispenser));
	EXPECT_FALSE(CanTakeOver(AbodeType::LivingQuarters));
	EXPECT_FALSE(CanTakeOver(AbodeType::Windmill));
	EXPECT_FALSE(CanTakeOver(AbodeType::Wonder));
	EXPECT_FALSE(CanTakeOver(AbodeType::General));
}

TEST(MainStoragePit, TheNewestStoragePitThatWorksTakesOver)
{
	const std::array candidates {
	    Candidate {.type = AbodeType::LivingQuarters, .functional = true, .storagePit = false},
	    Candidate {.type = AbodeType::StoragePit, .functional = false, .storagePit = true},
	    Candidate {.type = AbodeType::StoragePit, .functional = true, .storagePit = true},
	    Candidate {.type = AbodeType::StoragePit, .functional = true, .storagePit = true},
	};
	EXPECT_EQ(Successor(candidates), 2u);
}

TEST(MainStoragePit, ANewerCivicBuildingThatWorksLeavesTheTownWithNoMainPit)
{
	const std::array candidates {
	    Candidate {.type = AbodeType::Creche, .functional = true, .storagePit = false},
	    Candidate {.type = AbodeType::StoragePit, .functional = true, .storagePit = true},
	};
	EXPECT_FALSE(Successor(candidates).has_value());
}

TEST(MainStoragePit, ACivicBuildingThatDoesNotWorkIsPassed)
{
	const std::array candidates {
	    Candidate {.type = AbodeType::Workshop, .functional = false, .storagePit = false},
	    Candidate {.type = AbodeType::StoragePit, .functional = true, .storagePit = true},
	};
	EXPECT_EQ(Successor(candidates), 1u);
}

TEST(MainStoragePit, NoCivicBuildingThatWorksGivesNoMainPit)
{
	EXPECT_FALSE(Successor({}).has_value());
	const std::array candidates {
	    Candidate {.type = AbodeType::LivingQuarters, .functional = true, .storagePit = false},
	    Candidate {.type = AbodeType::StoragePit, .functional = false, .storagePit = true},
	};
	EXPECT_FALSE(Successor(candidates).has_value());
}
