/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The testbed's scenario of a disciple under the player's hand, showing its work's symbol

#include <vector>

#include "TestbedScenarioRegistry.h"

using namespace openblack;
using namespace openblack::testbed_scenarios;

void testbed_scenarios::AddDiscipleScenarios(std::vector<Scenario>& all)
{
	all.push_back({
	    .id = "villagers.disciple_symbol",
	    .name = "A disciple's symbol under the hand",
	    .facet = Facet::Hand,
	    .description = "A grown villager of the player's town, made a farming disciple and held still by a script, stands "
	                   "in the middle of the screen with the player's hand over it.",
	    .expected = "The villager glows orange, its puffs turning, and the yellow wheat symbol of a farmer stands over the "
	                "glow, the same size on the screen wherever the camera is. On every screen it takes the same share "
	                "of the screen's height (of its 4:3 middle) as on the game's default 800x600 screen.",
	    .environment = {.dispenserGrid = false, .cursor = glm::vec2 {0.5f, 0.5f}},
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 8.0f, -14.0f}, .look = {0.0f, 1.0f, 0.0f}},
	    .objects = {{.type = VillagerInfo::CelticFarmerMale,
	                 .offset = {0.0f, 0.0f},
	                 .joinTown = true,
	                 .inScript = true,
	                 .disciple = VillagerDisciple::Farmer}},
	});
}
