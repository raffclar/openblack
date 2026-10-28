/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include <cstdint>

#include <algorithm>
#include <array>
#include <optional>
#include <vector>

#include <gtest/gtest.h>

#include "3D/MapCoords.h"
#include "Common/GUtilsDistance.h"
#include "ECS/ComputerMind.h"
#include "ECS/ComputerMindDomains.h"
#include "ECS/ComputerMindRules.h"
#include "ECS/Systems/Implementations/ComputerMindRuleTable.h"

using namespace openblack;
using namespace openblack::computer_mind;
using namespace openblack::computer_mind_rules;
namespace computer_world = openblack::ecs::systems::computer_world;

TEST(ComputerMindCore, EachPartAddsOnlyTheRulesOfItsOwnKinds)
{
	const std::array adders {computer_world::RegisterCoreRules,         computer_world::RegisterTownsRules,
	                         computer_world::RegisterWorshipRules,      computer_world::RegisterMiraclesRules,
	                         computer_world::RegisterCreatureCareRules, computer_world::RegisterCreaturePlayRules,
	                         computer_world::RegisterConquestRules,     computer_world::RegisterWarRules,
	                         computer_world::RegisterDefenceRules,      computer_world::RegisterAttitudesRules};
	static_assert(adders.size() == static_cast<size_t>(MindDomain::_Count));
	size_t rules = 0;
	for (size_t part = 0; part < adders.size(); ++part)
	{
		computer_world::RuleTable table {};
		adders.at(part)(table);
		for (size_t kind = 0; kind < table.size(); ++kind)
		{
			const auto& rule = table.at(kind);
			if (rule.intensity != nullptr || rule.perform != nullptr)
			{
				EXPECT_EQ(DomainOf(static_cast<NodeKind>(kind)), static_cast<MindDomain>(part)) << kind;
				++rules;
			}
		}
	}
	// The table of all the rules has each of them
	const auto& all = computer_world::Rules();
	const auto filled = std::ranges::count_if(
	    all, [](const computer_world::Rule& rule) { return rule.intensity != nullptr || rule.perform != nullptr; });
	EXPECT_EQ(static_cast<size_t>(filled), rules);
	EXPECT_NE(all.at(static_cast<size_t>(NodeKind::ActionChargeUpSpell)).perform, nullptr);
}

TEST(ComputerMindCore, OwnsItsKindsOfNodes)
{
	EXPECT_EQ(DomainOf(NodeKind::Root), MindDomain::Core);
	EXPECT_EQ(DomainOf(NodeKind::NeedAttendTownDesires), MindDomain::Core);
	EXPECT_EQ(DomainOf(NodeKind::ObjectDIRECTPickupAndDropObject), MindDomain::Core);
}

TEST(ComputerMindRules, NearestEdgeOfTheInfluence)
{
	const auto at = [](float x, float z) {
		return map_coords::MapCoords {.x = map_coords::ToFixed(x), .z = map_coords::ToFixed(z), .altitude = 0.0f};
	};
	EXPECT_FALSE(NearestInfluenceEdge(at(0.0f, 0.0f), {}).has_value());
	// The citadel at (1000, 1000) reaches 100 m; a town at (1500, 1000) reaches 300 m. The place is east of both.
	const std::vector<InfluenceCircle> circles {{.centre = at(1000.0f, 1000.0f), .radius = 100.0f},
	                                            {.centre = at(1500.0f, 1000.0f), .radius = 300.0f}};
	const auto place = at(1900.0f, 1000.0f);
	const auto edge = NearestInfluenceEdge(place, circles);
	ASSERT_TRUE(edge.has_value());
	// The town's edge is nearer: 100 m from the place, east of the town
	EXPECT_NEAR(map_coords::ToMetres(edge->x), 1800.0f, 0.5f);
	EXPECT_NEAR(map_coords::ToMetres(edge->z), 1000.0f, 0.5f);
	// Of two circles at the same distance the first wins
	const std::vector<InfluenceCircle> same {{.centre = at(1000.0f, 1000.0f), .radius = 100.0f},
	                                         {.centre = at(1000.0f, 1000.0f), .radius = 100.0f}};
	EXPECT_TRUE(NearestInfluenceEdge(place, same).has_value());
}

TEST(ComputerMindRules, PlaceOutsideTheInfluencePullsLessFurtherOut)
{
	// The pull is the square of the distance modifier, nearly full at the edge
	const float edge = gutils::GetDistanceModifier(0.0f, 200.0f);
	EXPECT_FLOAT_EQ(OutsidePlacePull(0.0f, 200.0f), edge * edge);
	EXPECT_GT(OutsidePlacePull(0.0f, 200.0f), 0.99f);
	const float modifier = gutils::GetDistanceModifier(50.0f, 200.0f);
	EXPECT_FLOAT_EQ(OutsidePlacePull(50.0f, 200.0f), modifier * modifier);
	// Past the reach it is nothing
	EXPECT_FLOAT_EQ(OutsidePlacePull(400.0f, 200.0f), 0.0f);
}
