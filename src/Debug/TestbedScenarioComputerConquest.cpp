/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <string_view>
#include <vector>

#include "TestbedScenarioComputerPlayers.h"
#include "TestbedScenarioRegistry.h"

using namespace openblack;
using namespace openblack::testbed_scenarios;

namespace
{

/// The lines of a land that give player two a computer mind, a temple and a Celtic town that offers the food miracle,
/// with a full storage pit. A neutral Celtic town to the east has an empty storage pit and two abodes of hungry people.
constexpr std::string_view k_NeutralTownScript = R"(TOGGLE_COMPUTER_PLAYER("PLAYER_TWO", 1)
CREATE_CITADEL("0.0,-90.0", 0, "PLAYER_TWO", 5690, 1000)
CREATE_TOWN(900, "0.0,40.0", "PLAYER_TWO", 0, "CELTIC")
SET_TOWN_BELIEF_CAP(900, "PLAYER_TWO", 2.0)
CREATE_NEW_TOWN_SPELL(900, "FOOD")
SET_TOWN_BELIEF(900, "PLAYER_TWO", 1.0)
CREATE_TOWN_CENTRE(900, "-4.0,41.0", "CELTIC_ABODE_TOWN_CENTRE", 0, 1000, 0)
CREATE_ABODE(900, "10.0,90.0", "CELTIC_ABODE_STORAGE_PIT", 6282, 1000, 3000, 3000)
CREATE_ABODE(900, "40.0,60.0", "CELTIC_ABODE_F", 4328, 1095, 0, 0)
CREATE_VILLAGER_POS("36.0,62.0", "40.0,60.0", "CELTIC_HOUSEWIFE", 20)
CREATE_VILLAGER_POS("36.0,62.0", "40.0,60.0", "CELTIC_FORESTER", 20)
CREATE_VILLAGER_POS("36.0,62.0", "40.0,60.0", "CELTIC_HOUSEWIFE", 20)
CREATE_VILLAGER_POS("36.0,62.0", "40.0,60.0", "CELTIC_FORESTER", 20)
CREATE_ABODE(900, "-40.0,60.0", "CELTIC_ABODE_F", 4328, 1095, 0, 0)
CREATE_VILLAGER_POS("-44.0,62.0", "-40.0,60.0", "CELTIC_HOUSEWIFE", 20)
CREATE_VILLAGER_POS("-44.0,62.0", "-40.0,60.0", "CELTIC_FORESTER", 20)
CREATE_VILLAGER_POS("-44.0,62.0", "-40.0,60.0", "CELTIC_HOUSEWIFE", 20)
CREATE_VILLAGER_POS("-44.0,62.0", "-40.0,60.0", "CELTIC_FORESTER", 20)
CREATE_TOWN(901, "170.0,40.0", "NEUTRAL", 0, "CELTIC")
CREATE_TOWN_CENTRE(901, "166.0,41.0", "CELTIC_ABODE_TOWN_CENTRE", 0, 1000, 0)
CREATE_ABODE(901, "180.0,90.0", "CELTIC_ABODE_STORAGE_PIT", 6282, 1000, 0, 0)
CREATE_ABODE(901, "200.0,60.0", "CELTIC_ABODE_F", 4328, 1095, 0, 0)
CREATE_VILLAGER_POS("196.0,62.0", "200.0,60.0", "CELTIC_HOUSEWIFE", 20)
CREATE_VILLAGER_POS("196.0,62.0", "200.0,60.0", "CELTIC_FORESTER", 20)
CREATE_VILLAGER_POS("196.0,62.0", "200.0,60.0", "CELTIC_HOUSEWIFE", 20)
CREATE_VILLAGER_POS("196.0,62.0", "200.0,60.0", "CELTIC_FORESTER", 20)
CREATE_ABODE(901, "140.0,60.0", "CELTIC_ABODE_F", 4328, 1095, 0, 0)
CREATE_VILLAGER_POS("136.0,62.0", "140.0,60.0", "CELTIC_HOUSEWIFE", 20)
CREATE_VILLAGER_POS("136.0,62.0", "140.0,60.0", "CELTIC_FORESTER", 20)
CREATE_VILLAGER_POS("136.0,62.0", "140.0,60.0", "CELTIC_HOUSEWIFE", 20)
CREATE_VILLAGER_POS("136.0,62.0", "140.0,60.0", "CELTIC_FORESTER", 20)
)";

/// The neutral town is the first thing of each forced action
constexpr ComputerActionThing k_NeutralTown {.kind = ComputerActionThing::Kind::Town, .townId = 901};
/// The game turns that the scenarios wait before the action, so that the worshippers come to the site first
constexpr uint32_t k_Wait = 300;

/// The scenarios show the temple, the god's town and the neutral town
Framing ConquestFraming()
{
	return {.shot = Shot::Overview, .include = {glm::vec2(-80.0f, -110.0f), glm::vec2(230.0f, 110.0f)}};
}

/// The tiger of player two, between the temple and the god's town
CreatureSetup GodTiger()
{
	return {.label = "player two's",
	        .species = CreatureType::Tiger,
	        .offset = {30.0f, -30.0f},
	        .owner = PlayerNames::PLAYER_TWO,
	        .size = 1.0f};
}

/// A cart and a barrel near the temple, things that the god can throw
std::vector<ObjectSetup> CartsByTheTemple()
{
	return {{.type = MobileObjectInfo::EgyptCart, .offset = {40.0f, -70.0f}},
	        {.type = MobileObjectInfo::EgyptBarrel, .offset = {-40.0f, -70.0f}}};
}

} // namespace

void testbed_scenarios::AddComputerGodConquestScenarios(std::vector<Scenario>& all)
{
	all.push_back({
	    .id = "mind.computer_god_take_over_food",
	    .name = "A computer god that feeds a neutral town to win it",
	    .facet = Facet::Mind,
	    .description = "Lines of a land switch on the computer mind of player two. Player two has a temple, and a town "
	                   "with a full storage pit that offers the food miracle. A neutral town to the east has an empty "
	                   "storage pit and hungry people. After 30 seconds, a testbed tool puts the action "
	                   "TakeOverTownByFulfillingFoodDesire in the queue of the god, with the neutral town.",
	    .expected = "The god finds the food need of the neutral town. The hand takes food from the storage pit of its own "
	                "town and drops it on the storage pit of the neutral town, or the worshippers chant for the food "
	                "miracle and the hand casts it on that storage pit. The neutral town shows belief in the colour of "
	                "player two. The players.list query shows the node and the steps of the hand.",
	    .framing = ConquestFraming(),
	    .landScript = k_NeutralTownScript,
	    .worshipShare = 0.5f,
	    .computerActions = {{.afterTurns = k_Wait, .node = "TakeOverTownByFulfillingFoodDesire", .first = k_NeutralTown}},
	});
	all.push_back({
	    .id = "mind.computer_god_fulfil_needs",
	    .name = "A computer god that casts the food miracle on the store of a neutral town",
	    .facet = Facet::Mind,
	    .description = "The land of the feed scenario. After 30 seconds, a testbed tool puts the action "
	                   "CastSpellToFulfilTownNeeds in the queue of the god, with the neutral town and the food icon.",
	    .expected = "The hand moves to the food icon of the site, and the worshippers chant for the miracle. When the "
	                "charge is full, the hand casts the food miracle on the storage pit of the neutral town. The "
	                "players.list query shows the steps of the hand.",
	    .framing = ConquestFraming(),
	    .landScript = k_NeutralTownScript,
	    .worshipShare = 0.5f,
	    .computerActions = {{.afterTurns = k_Wait,
	                         .node = "CastSpellToFulfilTownNeeds",
	                         .first = k_NeutralTown,
	                         .second = {.kind = ComputerActionThing::Kind::Icon, .type = MagicType::Food}}},
	});
	all.push_back({
	    .id = "mind.computer_god_creature_impress",
	    .name = "A computer god that sends its creature to impress a neutral town",
	    .facet = Facet::Mind,
	    .description = "The land of the feed scenario, with the tiger of player two between the temple and its town. "
	                   "After 30 seconds, a testbed tool puts the action UseCreatureToImpressTown in the queue of the "
	                   "god, with the neutral town and the tiger.",
	    .expected = "The hand puts the leash on the tiger and ties the leash to the storage pit of the neutral town. The "
	                "tiger wants to impress for two minutes, walks to the town and does impressive things there. The "
	                "players.list query shows the steps of the hand.",
	    .framing = ConquestFraming(),
	    .creatures = {GodTiger()},
	    .landScript = k_NeutralTownScript,
	    .worshipShare = 0.5f,
	    .computerActions = {{.afterTurns = k_Wait,
	                         .node = "UseCreatureToImpressTown",
	                         .first = k_NeutralTown,
	                         .second = {.kind = ComputerActionThing::Kind::Creature, .object = 0}}},
	});
	all.push_back({
	    .id = "mind.computer_god_creature_help",
	    .name = "A computer god that sends its creature to help a neutral town",
	    .facet = Facet::Mind,
	    .description = "The land of the creature impress scenario. After 30 seconds, a testbed tool puts the action "
	                   "GetCreaturetoHelp in the queue of the god, with the neutral town and the tiger.",
	    .expected = "The tiger walks to the storage pit of the neutral town, at the speed for a long or a short journey. "
	                "The hand puts the leash on the tiger and ties the leash to that storage pit. The players.list query "
	                "shows the steps of the hand.",
	    .framing = ConquestFraming(),
	    .creatures = {GodTiger()},
	    .landScript = k_NeutralTownScript,
	    .worshipShare = 0.5f,
	    .computerActions = {{.afterTurns = k_Wait,
	                         .node = "GetCreaturetoHelp",
	                         .first = k_NeutralTown,
	                         .second = {.kind = ComputerActionThing::Kind::Creature, .object = 0}}},
	});
	all.push_back({
	    .id = "mind.computer_god_throw_impress",
	    .name = "A computer god that throws a thing to impress a neutral town",
	    .facet = Facet::Mind,
	    .description = "The land of the feed scenario, with a cart and a barrel near the temple. After 30 seconds, a "
	                   "testbed tool puts the action TakeOverTown in the queue of the god, with the neutral town. The god "
	                   "then chooses the best of its four ways to impress the town.",
	    .expected = "The god finds the best place of the neutral town to land a thing, and the thing near its hand that "
	                "impresses most. The hand picks up the cart or the barrel and throws it at that place, or the god "
	                "chooses the food miracle for the store of the town. The neutral town shows belief in the colour of "
	                "player two. The players.list query shows the values of the four ways.",
	    .framing = ConquestFraming(),
	    .objects = CartsByTheTemple(),
	    .landScript = k_NeutralTownScript,
	    .worshipShare = 0.5f,
	    .objectsAfterLandScript = true,
	    .computerActions = {{.afterTurns = k_Wait, .node = "TakeOverTown", .first = k_NeutralTown}},
	});
}
