/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The testbed's scenario of the hand knocking on a house at night, with its people asleep inside

#include <vector>

#include "TestbedScenarioRegistry.h"

using namespace openblack;
using namespace openblack::testbed_scenarios;

void testbed_scenarios::AddKnockScenarios(std::vector<Scenario>& all)
{
	using Kind = Command::Kind;
	constexpr glm::vec2 k_House {0.0f, 30.0f};
	all.push_back({
	    .id = "villagers.knock_at_night",
	    .name = "Knocking on a house at night",
	    .facet = Facet::Hand,
	    .description = "A Celtic hut at night. Two grown villagers that live in the hut stand beside it. The neutral "
	                   "player holds their town from the start, so no change of owner makes the town celebrate. At 60 "
	                   "seconds, when both have gone in to sleep, the player's hand knocks on the hut. It knocks again "
	                   "3 seconds later.",
	    .expected = "At night the town wants sleep, so they go home into the hut, where they are not drawn. The knock plays a "
	                "knocking sound at the hand, the hand stays upright where it was and plays its tap once, and a row "
	                "of little people grows in over the hut for a second, stays six and shrinks away over the last: a "
	                "gold one for each adult living there and a green one for each free place. Everyone inside comes out "
	                "a few steps from the door, yawns and decides what to do; they don't go back to bed for that "
	                "decision. The second knock plays the next of the nine knocking sounds and holds the row again.",
	    .environment = {.hour = 23.0f, .dispenserGrid = false, .cursor = glm::vec2 {0.5f, 0.5f}},
	    .framing = {.shot = Shot::Overview, .include = {k_House}, .distance = 0.3f},
	    .objects = {{.type = AbodeInfo::CelticHut, .offset = k_House, .yawDegrees = 180.0f},
	                {.type = VillagerInfo::CelticFarmerMale,
	                 .offset = k_House + glm::vec2 {-6.0f, -8.0f},
	                 .joinTown = true,
	                 .homeIn = 0},
	                {.type = VillagerInfo::CelticHousewifeFemale,
	                 .offset = k_House + glm::vec2 {6.0f, -8.0f},
	                 .joinTown = true,
	                 .homeIn = 0}},
	    // A new villager stands for 1 to 500 turns before it first decides, and then walks home for about 8 seconds.
	    // Thus at 60 seconds both are inside for all random draws.
	    .commands = {{.kind = Kind::HandTapObject, .delaySeconds = 60.0f, .object = 0},
	                 {.kind = Kind::HandTapObject, .delaySeconds = 3.0f, .object = 0}},
	    .townOwner = PlayerNames::NEUTRAL,
	});
}
