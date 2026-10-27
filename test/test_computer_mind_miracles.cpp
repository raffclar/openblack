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

TEST(ComputerMindMiracles, OwnsItsKindsOfNodes)
{
	EXPECT_EQ(DomainOf(NodeKind::ActionChargeUpSpell), MindDomain::Miracles);
	EXPECT_EQ(DomainOf(NodeKind::ObjectCastWaterSpell), MindDomain::Miracles);
	EXPECT_EQ(DomainOf(NodeKind::ObjectCastShieldToProtect), MindDomain::Miracles);
}

TEST(ComputerMindRules, ChargeShareCountsTheSiteAndTheIcon)
{
	const IconFacts icon {
	    .site = true, .store = 1.0f, .cost = 10.0f, .battery = 2.0f, .available = 5.0f, .used = 1.0f, .capacity = 3.0f};
	EXPECT_FLOAT_EQ(ChargeShare(icon), (4.0f + 2.0f + 1.001f) / 10.001f);
	// With no dancers and too little, nothing
	auto still = icon;
	still.capacity = 0.0f;
	EXPECT_EQ(ChargeShare(still), 0.0f);
	// Without a site only the store counts
	EXPECT_FLOAT_EQ(ChargeShare({.store = 1.0f, .cost = 10.0f}), 1.001f / 10.001f);
}

TEST(ComputerMindRules, TurnsToChargeTheRest)
{
	const IconFacts icon {.site = true, .store = 1.0f, .cost = 10.0f, .available = 3.0f, .capacity = 2.0f};
	EXPECT_FLOAT_EQ(TurnsToCharge(icon), 3.0f);
	auto enough = icon;
	enough.available = 9.0f;
	EXPECT_EQ(TurnsToCharge(enough), 0.0f);
	auto still = icon;
	still.capacity = 0.0f;
	EXPECT_EQ(TurnsToCharge(still), -1.0f);
	EXPECT_EQ(TurnsToCharge({.store = 1.0f, .cost = 10.0f}), -1.0f);
}

TEST(ComputerMindRules, WishesAndChoices)
{
	EXPECT_FLOAT_EQ(RequestShare(30), 0.3f);
	EXPECT_EQ(RequestShare(500), 1.0f);
	EXPECT_EQ(SpellValue(2.0f, 0.0f, 1.0f), 1.0f);
	EXPECT_EQ(SpellValue(0.5f, 0.0f, 0.0f), 0.0f);
	const std::vector<float> desires {0.2f, 0.5f, 0.5f};
	EXPECT_EQ(MostWanting(desires), 1u);
	EXPECT_FALSE(MostWanting(std::vector<float> {0.0f, -1.0f}).has_value());
	const std::vector<ChargeCandidate> icons {{.charging = true, .requests = 90},
	                                          {.store = 1.0f, .requests = 90},
	                                          {.requests = 20},
	                                          {.requests = 40},
	                                          {.requests = 40}};
	const auto choice = ChooseCharge(icons);
	ASSERT_TRUE(choice.has_value());
	EXPECT_EQ(choice->index, 3u);
	EXPECT_FLOAT_EQ(choice->value, 0.4f);
}
