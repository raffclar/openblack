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

TEST(ComputerMindTowns, OwnsItsKindsOfNodes)
{
	EXPECT_EQ(DomainOf(NodeKind::ActionTownWithHighestFoodDesire), MindDomain::Towns);
	EXPECT_EQ(DomainOf(NodeKind::ObjectShallCastFoodSpell), MindDomain::Towns);
	EXPECT_EQ(DomainOf(NodeKind::ObjectGetFoodFromField), MindDomain::Towns);
}

TEST(ComputerMindRules, FieldWishAndChoice)
{
	// A full field at the hand with a full pull is wanted almost fully
	const float full = FieldWish(100.0f, 100.0f, 0.0f, 1.0f);
	EXPECT_FLOAT_EQ(full, gutils::GetDistanceModifier(0.0f, k_FieldReach) * ((100.0f + 0.001f) / (100.0f + 0.001f)));
	// An empty field is wanted little, and a field outside the influence that does not pull is not wanted
	EXPECT_LT(FieldWish(0.0f, 100.0f, 0.0f, 1.0f), 0.001f);
	EXPECT_FLOAT_EQ(FieldWish(100.0f, 100.0f, 0.0f, 0.0f), 0.0f);
	// The first of the largest over a tenth wins; none at a tenth or less
	EXPECT_EQ(MostWantedField(std::vector<float> {0.1f, 0.05f}), std::nullopt);
	EXPECT_EQ(MostWantedField(std::vector<float> {0.2f, 0.5f, 0.5f}), std::optional<size_t> {1});
}
