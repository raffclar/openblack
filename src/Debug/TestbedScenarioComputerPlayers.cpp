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
	    .id = "multiplayer.computer_slot_creature_name",
	    .name = "The name of the creature of a computer god in an empty slot",
	    .facet = Facet::Multiplayer,
	    .description =
	        "The player's tiger stands to the west at size 0.3. A temple of player two stands to the north, and no human or "
	        "computer plays player two. As at the start of a network game, the empty slot of player two gets a "
	        "computer god and a creature.",
	    .expected = "A fully grown creature of a random species stands at the creature home of the temple of player two. "
	                "Its name is the name of its species, for example \"Tiger\" or \"Polar bear\", not a line of help "
	                "text. The game.entity query shows the name. Start a fight between the tiger and the creature: the fight "
	                "view shows the name of each creature beside its bar.",
	    .environment = {.dispenserGrid = false},
	    .creatures = {{.label = "the player's", .species = CreatureType::Tiger, .offset = {-25.0f, 60.0f}, .size = 0.3f}},
	    .temples = {{.offset = {0.0f, 90.0f}, .owner = PlayerNames::PLAYER_TWO}},
	    .fillEmptySlots = true,
	});

	all.push_back({
	    .id = "multiplayer.computer_slot_creature_name_in_fight",
	    .name = "The fight panel with the name of the creature of a computer god in an empty slot",
	    .facet = Facet::Multiplayer,
	    .description =
	        "The player's tiger stands to the west at size 0.3. A temple of player two stands to the north, and no human or "
	        "computer plays player two. The empty slot of player two gets a computer god and a creature, as at the start "
	        "of a network game. After 1 second the tiger starts a fight with that creature. When the arena is there, 7 "
	        "seconds after that, the camera flies to the fight.",
	    .expected = "The camera shows the fight, and the fight panel is in the top-left corner of the screen. The first "
	                "row has the name of the tiger and its bars. The second row has the name of the other creature, which "
	                "is the name of its species, for example \"Tortoise\" or \"Polar bear\", and its bars.",
	    .environment = {.dispenserGrid = false},
	    .creatures = {{.label = "the player's", .species = CreatureType::Tiger, .offset = {-25.0f, 60.0f}, .size = 0.3f}},
	    .temples = {{.offset = {0.0f, 90.0f}, .owner = PlayerNames::PLAYER_TWO}},
	    .commands = {{.kind = Command::Kind::StartFight, .creature = 0, .delaySeconds = 1.0f, .value = 1},
	                 {.kind = Command::Kind::FightView, .creature = 0, .delaySeconds = 7.0f}},
	    .fillEmptySlots = true,
	});

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
