/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The testbed's scenarios of two Guardian Stones of the fourth land: the Heartbroken Man, who keeps the stone of the
// darkness, and the bell towers round the stone of the fire

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
/// The gold scroll by the hut of the woman, from the middle of the land
constexpr glm::vec2 k_WomanScroll = glm::vec2(2930.707f, 2875.430f) - k_LandMiddle;
/// The gold scroll north of the bell towers, from the middle of the land
constexpr glm::vec2 k_TowerScroll = glm::vec2(2542.824f, 3109.667f) - k_LandMiddle;
/// The bell towers' stone, from the middle of the land
constexpr glm::vec2 k_FireStone = glm::vec2(2519.188f, 3081.405f) - k_LandMiddle;
/// The point that the land's script gives to find the Japanese village, and the radius of the search
constexpr glm::vec2 k_JapaneseVillage = glm::vec2(2590.363f, 3196.893f) - k_LandMiddle;
constexpr float k_VillageSearch = 50.0f;
/// The kinds of thing and of town that the land's script gives to find the village
constexpr float k_TownThing = 9.0f;
constexpr float k_AnyTown = 5000.0f;
/// The player that the script names first
constexpr float k_PlayerOne = 0.0f;
/// The belief of the village in the player, as a share of its belief in its owner, which makes the village the player's
constexpr float k_TakeVillage = 2.0f;

/// The command that finds the Japanese village as the land's script finds it
Command FindJapaneseVillage()
{
	return {.kind = Kind::CallNative,
	        .delaySeconds = 0.5f,
	        .script = "CALL_NEAR",
	        .scriptArguments = {{.kind = Argument::Kind::Integer, .number = k_TownThing},
	                            {.kind = Argument::Kind::Integer, .number = k_AnyTown},
	                            {.kind = Argument::Kind::Position, .point = k_JapaneseVillage},
	                            {.number = k_VillageSearch},
	                            {.kind = Argument::Kind::Integer, .number = 0.0f}}};
}

void AddHeartbrokenMan(std::vector<Scenario>& all)
{
	all.push_back({
	    .id = "story.l4_heartbroken_man",
	    .name = "Story: the Heartbroken Man of the fourth land",
	    .facet = Facet::Story,
	    .description = "The fourth land loads with its own scripts and sky. The land's script of the darkness starts, "
	                   "and the quest of the Heartbroken Man starts as the land's script starts it, with the flag of the "
	                   "elder's introduction already raised. The small Guardian Stone smokes by the hut of Adam on the "
	                   "mountain. Click the gold scroll by the hut of the woman. Then carry Keiko from the Aztec village "
	                   "to Adam, or kill Keiko, or kill Adam.",
	    .expected = "The gold scroll is by the hut of the woman. A click on it starts the cut scene: the woman walks out "
	                "and tells the story of Adam and Keiko. Keiko walks round three places at the edge of the Aztec "
	                "village. When Keiko comes within 50 of Adam, Adam comes out and waves. The reunion, the death of "
	                "Keiko or the death of Adam each has its own cut scene. After each, the stone rises, breaks with a "
	                "bang and a flash, and the dusk changes back to noon.",
	    .environment = {.land = Land::Story, .storyLand = "Land4.txt", .dispenserGrid = false},
	    .framing = {.shot = Shot::Placed,
	                .eye = {k_WomanScroll.x + 25.0f, 60.0f, k_WomanScroll.y - 35.0f},
	                .look = {k_WomanScroll.x, 31.0f, k_WomanScroll.y}},
	    .creatures = {{.label = "player's tiger",
	                   .species = CreatureType::Tiger,
	                   .offset = k_WomanScroll + glm::vec2(-15.0f, -10.0f),
	                   .facingDegrees = 300.0f}},
	    .commands = {{.kind = Kind::RunScript, .delaySeconds = 0.5f, .script = "Land4MeteoritesDarknessMeteorEffect"},
	                 {.kind = Kind::RunScript,
	                  .delaySeconds = 0.0f,
	                  .script = "Land4Nomad",
	                  .scriptGlobals = {{"StartLand4Nomad", {.number = 1.0f}},
	                                    {"MyCreature", {.kind = Argument::Kind::Creature, .index = 0}}}}},
	});
}

void AddTotemPuzzle(std::vector<Scenario>& all)
{
	all.push_back({
	    .id = "story.l4_totem_puzzle",
	    .name = "Story: the bell towers of the fourth land",
	    .facet = Facet::Story,
	    .description = "The fourth land loads with its own scripts and sky. The scenario finds the Japanese village as "
	                   "the land's script finds it, and starts the quest of the bell towers with it. Then the village "
	                   "becomes the player's. Click the gold scroll north of the towers, then click the towers in the "
	                   "order that they ring.",
	    .expected = "The small Guardian Stone smokes in its two shields, with five bell towers round it. Within 10 "
	                "seconds after the village becomes the player's, the gold scroll comes. A click on it starts the "
	                "elder's introduction. Then the towers ring a sequence, a note each second. Each round adds notes. "
	                "A wrong tower or a late click fails the attempt, and a new scroll comes. After four rounds the "
	                "towers break, the shields go, and the stone rises and breaks.",
	    .environment = {.land = Land::Story, .storyLand = "Land4.txt", .dispenserGrid = false},
	    .framing = {.shot = Shot::Placed,
	                .eye = {k_TowerScroll.x + 30.0f, 60.0f, k_TowerScroll.y + 10.0f},
	                .look = {k_FireStone.x, 35.0f, k_FireStone.y}},
	    .commands =
	        {FindJapaneseVillage(),
	         {.kind = Kind::RunScript,
	          .delaySeconds = 0.0f,
	          .script = "Land4Puzzle",
	          .scriptGlobals = {{"JapaneseTown", {.kind = Argument::Kind::LastResult}}}},
	         FindJapaneseVillage(),
	         {.kind = Kind::CallNative,
	          .delaySeconds = 2.0f,
	          .script = "SET_PLAYER_BELIEF",
	          .scriptArguments = {{.kind = Argument::Kind::LastResult}, {.number = k_PlayerOne}, {.number = k_TakeVillage}}}},
	});
}
} // namespace

void testbed_scenarios::AddLand4StoneScenarios(std::vector<Scenario>& all)
{
	AddHeartbrokenMan(all);
	AddTotemPuzzle(all);
}
