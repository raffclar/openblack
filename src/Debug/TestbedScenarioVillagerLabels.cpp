/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The testbed scenarios for the labels over the villagers' heads. One shows the names display as it grows in and out.
// One shows how the world in front of a label hides it.

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
	all.push_back({
	    .id = "villagers.names_behind_hut",
	    .name = "A hut in front of a villager hides its name",
	    .facet = Facet::Hand,
	    .description = "Two named women of the player's town stand on each side of a hut, one in front of it and one behind "
	                   "it. The Show Villager Names key (N) is pressed after a second.",
	    .expected = "The name of the woman in front of the hut shows in full. The hut hides the part of the other "
	                "name that is behind it, as it hides her body. Turn the camera to look down: the hidden part of the "
	                "name then shows, because the names come nearer to the camera as it looks down.",
	    .environment = {.dispenserGrid = false, .cursor = glm::vec2 {0.02f, 0.02f}},
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 2.0f, -9.0f}, .look = {0.0f, 3.0f, 0.0f}},
	    .objects = {{.type = AbodeInfo::CelticHut, .offset = {0.0f, 0.0f}},
	                {.type = VillagerInfo::CelticHousewifeFemale, .offset = {2.0f, -4.0f}, .joinTown = true, .nameRow = 1},
	                {.type = VillagerInfo::CelticHousewifeFemale, .offset = {0.5f, 5.0f}, .joinTown = true, .nameRow = 2}},
	    .commands = {{.kind = Kind::PressKey,
	                  .delaySeconds = 1.0f,
	                  .value = static_cast<size_t>(input::BindableActionMap::SHOW_VILLAGER_NAMES)}},
	});
	all.push_back({
	    .id = "villagers.names_behind_smoke",
	    .name = "Smoke in front of a villager's name",
	    .facet = Facet::Hand,
	    .description = "Two named women of the player's town stand in front of the camera, one near and one further "
	                   "away. Smoke goes up between them. The Show Villager Names key (N) is pressed after a second.",
	    .expected = "The game sorts the names among the see-through things of the world by distance. The smoke is "
	                "nearer to the camera than the far woman's name, so it goes over that name and makes it dim. The "
	                "near woman's name is in front of the smoke and shows clearly over it.",
	    .environment = {.dispenserGrid = false, .cursor = glm::vec2 {0.02f, 0.02f}},
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 2.0f, -9.0f}, .look = {0.0f, 3.0f, 0.0f}},
	    .objects = {{.type = VillagerInfo::CelticHousewifeFemale, .offset = {-1.5f, -4.0f}, .joinTown = true, .nameRow = 1},
	                {.type = VillagerInfo::CelticHousewifeFemale, .offset = {0.5f, 5.0f}, .joinTown = true, .nameRow = 2}},
	    .particles = {{.type = ParticleType::Smoke, .offset = {0.5f, 1.0f}, .height = 3.0f, .magnitude = 4.0f}},
	    .commands = {{.kind = Kind::PressKey,
	                  .delaySeconds = 1.0f,
	                  .value = static_cast<size_t>(input::BindableActionMap::SHOW_VILLAGER_NAMES)}},
	});
}
