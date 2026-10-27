/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <vector>

#include "TestbedScenarioComputerPlayers.h"
#include "TestbedScenarioRegistry.h"

using namespace openblack;
using namespace openblack::testbed_scenarios;

void testbed_scenarios::AddComputerGodWorshipScenarios(std::vector<Scenario>& all)
{
	all.push_back({
	    .id = "mind.computer_god_site_workers",
	    .name = "A computer god that carries a free villager to a building site of its town",
	    .facet = Facet::Mind,
	    .description = "Lines of a land switch on the computer mind of player two, with a temple to the south and a "
	                   "Celtic town to the north. A hut of the town stands a fifth built north of the town, with a site "
	                   "for its builders.",
	    .expected = "The god finds the free adult of the town that is best for the work, nearest its hand and with the "
	                "most life. The hand picks it up and puts it down beside the hut, a quarter more than the hut's "
	                "radius from its middle, at a point with wood on it if there is one. The players.list query shows "
	                "the GiveBuildingSiteWorkers need and the steps.",
	    .framing = {.shot = Shot::Overview, .include = {glm::vec2(-80.0f, -110.0f), glm::vec2(80.0f, 130.0f)}},
	    .objects = {{.type = AbodeInfo::CelticHut, .offset = {0.0f, 115.0f}, .built = 0.2f}},
	    .landScript = k_ComputerGodTownScript,
	    .objectsAfterLandScript = true,
	});
	all.push_back({
	    .id = "mind.computer_god_tired_worshippers",
	    .name = "A computer god that takes its tired worshippers home",
	    .facet = Facet::Mind,
	    .description = "The town of the food miracle scenario sends all of its people to worship at its built site. "
	                   "Each chant costs a dancer some of its life.",
	    .expected = "As the dancers chant they lose life. When the dancers want rest by more than a quarter, the hand "
	                "picks up the worshipper with the least life and puts it down outside its home, on the side of the "
	                "door, a quarter more than the home's radius from its middle. The players.list query shows the "
	                "GiveWorshippersRest need and the steps.",
	    .framing = {.shot = Shot::Overview, .include = {glm::vec2(-80.0f, -110.0f), glm::vec2(80.0f, 100.0f)}},
	    .landScript = k_ComputerGodTownScript,
	    .worshipShare = 1.0f,
	});
	all.push_back({
	    .id = "mind.computer_god_breeder",
	    .name = "A computer god that makes a breeder for its town",
	    .facet = Facet::Mind,
	    .description = "The town of the food miracle scenario, with no worship. The town has no breeders.",
	    .expected = "The god picks a town of its own with fewer than two breeders. It looks for an adult in the "
	                "homes of that town that is of an age to make love and that the hand cannot reach, such as one "
	                "inside its home, a man first. It makes that villager a breeder at once, picks it up and puts it "
	                "down where it stood. The villager glows as a disciple. The players.list query shows the "
	                "MakeDiscipleBreader need.",
	    .framing = {.shot = Shot::Overview, .include = {glm::vec2(-80.0f, -110.0f), glm::vec2(80.0f, 100.0f)}},
	    .landScript = k_ComputerGodTownScript,
	});
	all.push_back({
	    .id = "mind.computer_god_worshippers_food",
	    .name = "A computer god that carries food from a field to its hungry worshippers",
	    .facet = Facet::Mind,
	    .description = "The town of the field food scenario, 260 metres east of the temple, with two ripe wheat fields. "
	                   "Half of its people worship at the site, which stands built to the west.",
	    .expected = "When the worshippers get hungry and the site's pot has less food than they need, the hand takes the "
	                "food of a field in a pot and drops the pot on the worship site. The players.list query shows the "
	                "GiveWorshippersFood need, with the food that the site needs from its towns as the node's amount.",
	    .framing = {.shot = Shot::Overview, .include = {glm::vec2(-80.0f, -110.0f), glm::vec2(340.0f, 140.0f)}},
	    .objects = {{.type = FieldTypeInfo::Wheat, .offset = {240.0f, 125.0f}, .ripeCrop = true},
	                {.type = FieldTypeInfo::Wheat, .offset = {300.0f, 125.0f}, .ripeCrop = true}},
	    .landScript = k_ComputerGodFarTownScript,
	    .worshipShare = 0.5f,
	    .objectsAfterLandScript = true,
	});
}
