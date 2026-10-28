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

void testbed_scenarios::AddComputerGodTownsScenarios(std::vector<Scenario>& all)
{
	all.push_back({
	    .id = "mind.computer_god_food_miracle",
	    .name = "A computer god that charges and casts the food miracle for its town",
	    .facet = Facet::Mind,
	    .description = "Lines of a land switch on the computer mind of player two. Player two has a temple to the south "
	                   "and a Celtic town to the north that offers the food miracle. The town's storage pit is empty, and "
	                   "half of its people go to worship at its site, which stands built.",
	    .expected = "Once the dancers chant and the town wants food, the god's hand moves from its temple to the food "
	                "icon of the site, and the worshippers chant for the miracle. When the charge is full, the hand flies "
	                "to the storage pit and casts the food miracle on it, then stays over the pit for the cast. The "
	                "players.list query shows what the god wants, its hand and its steps.",
	    .framing = {.shot = Shot::Overview, .include = {glm::vec2(-80.0f, -110.0f), glm::vec2(80.0f, 100.0f)}},
	    .landScript = k_ComputerGodTownScript,
	    .worshipShare = 0.5f,
	});
	all.push_back({
	    .id = "mind.computer_god_field_food",
	    .name = "A computer god that carries villagers to worship, food from a field, and draws the miracle gesture",
	    .facet = Facet::Mind,
	    .description = "The town of the food miracle scenario stands 260 metres further east, with two ripe wheat "
	                   "fields beside it. The temple of player two and its worship site stay to the west. The hand of "
	                   "the god starts at the temple, but the god measures from the middle of the land until its first "
	                   "action.",
	    .expected = "The hand carries villagers from the town to the gate of the worship site, waits a second and drops "
	                "them. It takes the food of a field in a pot and drops the pot in the storage pit of the town. When it "
	                "next charges the food miracle from beside the pit, 200 metres or more from the icon, it first draws "
	                "the spiral gesture in the air, 30 metres across, and the shape of the spiral shows on the land with "
	                "the sound of a recognised gesture. Small sparkles in the colour of player two show the hand.",
	    .framing = {.shot = Shot::Overview, .include = {glm::vec2(-80.0f, -110.0f), glm::vec2(340.0f, 140.0f)}},
	    .objects = {{.type = FieldTypeInfo::Wheat, .offset = {240.0f, 125.0f}, .ripeCrop = true},
	                {.type = FieldTypeInfo::Wheat, .offset = {300.0f, 125.0f}, .ripeCrop = true}},
	    .landScript = k_ComputerGodFarTownScript,
	    .worshipShare = 0.5f,
	    .objectsAfterLandScript = true,
	});
}
