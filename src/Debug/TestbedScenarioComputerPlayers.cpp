/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "TestbedScenarioComputerPlayers.h"

#include <string_view>
#include <vector>

#include "TestbedScenarioRegistry.h"

using namespace openblack;
using namespace openblack::testbed_scenarios;

namespace
{

/// The line of the Island Wars land that makes an anti-influence of 20 m for player one, here 40 m north of the middle
constexpr std::string_view k_AntiInfluenceRingScript = R"(CREATE_INFLUENCE_RING("0.0,40.0", 0, 20.000000, 202594004)
)";

} // namespace

void testbed_scenarios::AddComputerPlayerScenarios(std::vector<Scenario>& all)
{
	// Each part of the mind of a computer god adds its scenarios in its own file, in this order
	AddComputerGodCoreScenarios(all);
	AddComputerGodTownsScenarios(all);
	AddComputerGodWorshipScenarios(all);
	AddComputerGodScaffoldScenarios(all);
	AddComputerGodMiraclesScenarios(all);
	AddComputerGodCreatureCareScenarios(all);
	AddComputerGodCreaturePlayScenarios(all);
	AddComputerGodConquestScenarios(all);
	AddComputerGodWarScenarios(all);
	AddComputerGodDefenceScenarios(all);
	AddComputerGodAttitudesScenarios(all);

	all.push_back({
	    .id = "multiplayer.land_anti_influence_ring",
	    .name = "An anti-influence ring that a land's script makes",
	    .facet = Facet::Multiplayer,
	    .description = "The line of the Island Wars land that makes an influence ring. Its last value is not 0, so it is "
	                   "an anti-influence of 20 m for player one, here 40 m north of the middle, inside the player's "
	                   "testbed influence.",
	    .expected = "Inside 20 m of the spot the player has no influence: the hand cannot pick up, drop or cast there, and "
	                "the influence.at query gives 0 for player one. Just outside it the hand works as before.",
	    .environment = {.dispenserGrid = false},
	    .landScript = k_AntiInfluenceRingScript,
	});
}
