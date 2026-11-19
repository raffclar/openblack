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

#include <glm/vec2.hpp>

#include "TestbedScenarioRegistry.h"

using namespace openblack;
using namespace openblack::testbed_scenarios;

namespace
{

/// The line of the Island Wars land that makes an anti-influence of 20 m for player one, here 40 m north of the middle
constexpr std::string_view k_AntiInfluenceRingScript = R"(CREATE_INFLUENCE_RING("0.0,40.0", 0, 20.000000, 202594004)
)";

/// The town centre of the scenario of the symbol of a computer god in an empty slot, from the middle
constexpr glm::vec2 k_SlotTownCentre {0.0f, 40.0f};
/// The belief of that town in each of the two players
constexpr float k_SlotTownBelief = 1000.0f;

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
	    .id = "multiplayer.computer_slot_god_symbol",
	    .name = "The symbol of belief of a computer god in an empty slot of a network game",
	    .facet = Facet::Multiplayer,
	    .description = "A temple of player two stands to the north, and no human or computer plays player two. As in a "
	                   "network game, the land waits for its humans, and then player one joins. The empty slot of player "
	                   "two then gets a computer god. A Norse town centre of the player's town believes in the player and "
	                   "in player two. The camera is still, a little above the top of the totem.",
	    .expected = "Two symbols show at the totem: the symbol of the player and the picture of the computer god of "
	                "player two (a cross), in the colour of player two. Before the fix the town believed in player two, "
	                "but the symbol of player two did not show.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed,
	                .eye = {k_SlotTownCentre.x - 12.7f, 24.0f, k_SlotTownCentre.y - 27.2f},
	                .look = {k_SlotTownCentre.x, 21.0f, k_SlotTownCentre.y}},
	    .objects = {{.type = AbodeInfo::NorseTownCentre, .offset = k_SlotTownCentre}},
	    .temples = {{.offset = {0.0f, 140.0f}, .owner = PlayerNames::PLAYER_TWO}},
	    .townBeliefInPlayer = k_SlotTownBelief,
	    .townBeliefInRival = k_SlotTownBelief,
	    .humansJoinLater = true,
	});

	all.push_back({
	    .id = "multiplayer.map_land_after_start",
	    .name = "A land of a network game that loads after the humans joined",
	    .facet = Facet::Multiplayer,
	    .description = "After a second, the game loads the network land for three players in place of the testbed. In a "
	                   "network game of two humans, every computer loads it before the same turn. Fly to the temple of "
	                   "player three.",
	    .expected = "In a network game, the empty slot of player three gets a computer god and a fully grown creature at "
	                "its temple, on the server and on each client. Each human god has its hands on the new land. The "
	                "players.list query shows player three as a computer god with a creature. Before the fix the slot "
	                "stayed empty, because the humans joined only on the first land.",
	    .environment = {.clockRuns = true, .dispenserGrid = false},
	    .commands = {{.kind = Command::Kind::LoadMapLand, .delaySeconds = 1.0f, .land = "Online Maps/mpm_3p_1.map"}},
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
