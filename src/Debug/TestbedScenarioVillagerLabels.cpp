/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The testbed scenarios for the labels over the villagers' heads. One shows the names display as it grows in and out.

#include <vector>

#include "Input/BindableActions.h"
#include "TestbedScenarioRegistry.h"

using namespace openblack;
using namespace openblack::testbed_scenarios;

void testbed_scenarios::AddVillagerLabelScenarios(std::vector<Scenario>& all)
{
	using Kind = Command::Kind;
	const Command pressNames {.kind = Kind::PressKey,
	                          .delaySeconds = 3.0f,
	                          .value = static_cast<size_t>(input::BindableActionMap::SHOW_VILLAGER_NAMES)};
	all.push_back({
	    .id = "villagers.names_fade",
	    .name = "The villagers' names grow in and out",
	    .facet = Facet::Hand,
	    .description =
	        "A named man and three named women of the player's town stand in a row in front of the camera. The Show Villager "
	        "Names key (N) is pressed every three seconds: on, off, on. Press N yourself to try it again.",
	    .expected = "Each press starts the names display. Over a third of a second of game time, each name grows from "
	                "nothing to full size, and its box, words and pointer become clear together. The next press makes "
	                "them shrink and fade in the same time. The pointers keep their size and are as clear as the boxes. "
	                "While the game is paused, the names do not change.",
	    .environment = {.dispenserGrid = false, .cursor = glm::vec2 {0.02f, 0.02f}},
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 6.0f, -14.0f}, .look = {0.0f, 2.0f, 0.0f}},
	    .objects = {{.type = VillagerInfo::CelticFarmerMale, .offset = {-6.0f, 0.0f}, .joinTown = true, .nameRow = 0},
	                {.type = VillagerInfo::CelticHousewifeFemale, .offset = {-2.0f, 2.0f}, .joinTown = true, .nameRow = 1},
	                {.type = VillagerInfo::CelticHousewifeFemale, .offset = {2.0f, 0.0f}, .joinTown = true, .nameRow = 2},
	                {.type = VillagerInfo::CelticHousewifeFemale, .offset = {6.0f, 2.0f}, .joinTown = true, .nameRow = 3}},
	    .commands = {pressNames, pressNames, pressNames},
	});
}
