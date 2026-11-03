/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The testbed's scenario of the two totems of the undead village of the fourth land, which the player and the creature
// pull up out of the land

#include <glm/vec2.hpp>

#include "TestbedScenarioRegistry.h"

using namespace openblack;
using namespace openblack::testbed_scenarios;

namespace
{
using Argument = ScriptArgument;
using Kind = Command::Kind;

/// The middle of the blocks of the fourth land. The testbed measures the points of a scenario from it.
constexpr glm::vec2 k_LandMiddle {2720.0f, 2800.0f};
/// The two totems, from the middle of the land
constexpr glm::vec2 k_FirstTotem = glm::vec2(3016.44f, 3584.36f) - k_LandMiddle;
constexpr glm::vec2 k_SecondTotem = glm::vec2(2922.68f, 3550.94f) - k_LandMiddle;
/// The point between the two totems
constexpr glm::vec2 k_Between = (k_FirstTotem + k_SecondTotem) * 0.5f;
} // namespace

void testbed_scenarios::AddUndeadTotemScenarios(std::vector<Scenario>& all)
{
	all.push_back({
	    .id = "story.l4_undead_totems",
	    .name = "Story: the totems of the undead village",
	    .facet = Facet::Story,
	    .description = "The fourth land loads with its own scripts and sky. The land's script makes the two totems of the "
	                   "undead village, down in the land, and then the script that looks after them starts. Take hold of "
	                   "a totem with the Action button and move the mouse up to pull it out of the land. Let go, and look "
	                   "at it sink. Pull a totem fully up with the creature near it.",
	    .expected = "Each totem shows only its top above the land. The totem follows the hand up and down, between its "
	                "top and its full height. After the hand lets go, the totem sinks slowly and dust comes out of the "
	                "land round it. While the hand holds it, it does not sink. When a totem goes above nine tenths of its "
	                "height with the creature within 50, the creature raises it fully the next time it is down. When "
	                "both totems are fully up, the cut scene of the skeleton man starts.",
	    .environment = {.land = Land::Story, .storyLand = "Land4.txt", .dispenserGrid = false},
	    .framing = {.shot = Shot::Placed,
	                .eye = {k_Between.x - 10.0f, 70.0f, k_Between.y - 90.0f},
	                .look = {k_Between.x, 15.0f, k_Between.y}},
	    .creatures = {{.label = "player's chimp",
	                   .species = CreatureType::Chimp,
	                   .offset = k_FirstTotem + glm::vec2(-20.0f, -20.0f),
	                   .facingDegrees = 40.0f}},
	    .commands = {{.kind = Kind::RunScript, .delaySeconds = 0.5f, .script = "SetupUndeadVillage"},
	                 {.kind = Kind::RunScript,
	                  .delaySeconds = 1.0f,
	                  .script = "LookAfterTotems",
	                  .scriptGlobals = {{"MyCreature", {.kind = Argument::Kind::Creature, .index = 0}}}}},
	});
}
