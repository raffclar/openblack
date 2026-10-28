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
#include <vector>

#include <gtest/gtest.h>

#include "3D/MapCoords.h"
#include "Common/GUtilsDistance.h"
#include "ECS/ComputerMind.h"
#include "ECS/ComputerMindDomains.h"
#include "ECS/ComputerMindRules.h"

using namespace openblack;
using namespace openblack::computer_mind;
using namespace openblack::computer_mind_rules;

TEST(ComputerMindWorship, OwnsItsKindsOfNodes)
{
	EXPECT_EQ(DomainOf(NodeKind::ActionWorshipSiteThatNeedsVillagers), MindDomain::Worship);
	EXPECT_EQ(DomainOf(NodeKind::ObjectTakeVillagerFromTownToWorshipSite), MindDomain::Worship);
	EXPECT_EQ(DomainOf(NodeKind::ActionGiveBuildingSiteWorkers), MindDomain::Worship);
}

TEST(ComputerMindRules, WorshipNeeds)
{
	// A town with no one at worship needs worshippers fully, and one with most of its people there needs none
	EXPECT_FLOAT_EQ(TownWorshipNeed(10, 0, 0), 1.0f);
	EXPECT_FLOAT_EQ(TownWorshipNeed(0, 0, 0), 0.0f);
	EXPECT_FLOAT_EQ(TownWorshipNeed(10, 5, 1), 0.0f);
	// A third at worship leaves the cube of two thirds
	const float missing = 1.0f - 1.0f / 3.0f;
	EXPECT_FLOAT_EQ(TownWorshipNeed(3, 1, 0), (missing * missing) * missing);
	// More than all counts as all
	EXPECT_FLOAT_EQ(TownWorshipNeed(2, 3, 1), 0.0f);
	// A site with no icons needs no dancers; four dancers for each icon fill it
	EXPECT_FLOAT_EQ(SiteDancerNeed(0, 0), 0.0f);
	EXPECT_FLOAT_EQ(SiteDancerNeed(0, 3), 1.0f);
	EXPECT_FLOAT_EQ(SiteDancerNeed(12, 3), 0.0f);
	EXPECT_FLOAT_EQ(SiteDancerNeed(6, 3), 1.0f - 0.0625f);
}
