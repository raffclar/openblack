/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <string_view>
#include <vector>

#include "TestbedScenarioRegistry.h"

using namespace openblack;
using namespace openblack::testbed_scenarios;

namespace
{

/// The lines of a skirmish land that give player two a computer mind and a creature from the computer's mind file. The
/// size of that creature comes from the local player's creature.
constexpr std::string_view k_ComputerPlayerScript = R"(TOGGLE_COMPUTER_PLAYER("PLAYER_TWO", 1)
CREATE_CREATURE_FROM_FILE("PLAYER_TWO", 3, "ComputerControlledCreature", "25.0,0.0")
SET_COMPUTER_PLAYER_CREATURE_LIKE("PLAYER_TWO", "PLAYER_ONE")
)";

/// The lines of a land that give player two a computer mind, a temple and a Celtic town that offers the food miracle,
/// with an empty storage pit and eight abodes of people
constexpr std::string_view k_ComputerGodTownScript = R"(TOGGLE_COMPUTER_PLAYER("PLAYER_TWO", 1)
CREATE_CITADEL("0.0,-90.0", 0, "PLAYER_TWO", 5690, 1000)
CREATE_TOWN(900, "0.0,40.0", "PLAYER_TWO", 0, "CELTIC")
SET_TOWN_BELIEF_CAP(900, "PLAYER_TWO", 2.0)
CREATE_NEW_TOWN_SPELL(900, "FOOD")
SET_TOWN_BELIEF(900, "PLAYER_TWO", 1.0)
CREATE_TOWN_CENTRE(900, "-4.0,41.0", "CELTIC_ABODE_TOWN_CENTRE", 0, 1000, 0)
CREATE_ABODE(900, "10.0,90.0", "CELTIC_ABODE_STORAGE_PIT", 6282, 1000, 0, 3000)
CREATE_ABODE(900, "40.0,60.0", "CELTIC_ABODE_F", 4328, 1095, 0, 0)
CREATE_VILLAGER_POS("36.0,62.0", "40.0,60.0", "CELTIC_HOUSEWIFE", 20)
CREATE_VILLAGER_POS("36.0,62.0", "40.0,60.0", "CELTIC_FORESTER", 20)
CREATE_VILLAGER_POS("36.0,62.0", "40.0,60.0", "CELTIC_HOUSEWIFE", 20)
CREATE_VILLAGER_POS("36.0,62.0", "40.0,60.0", "CELTIC_FORESTER", 20)
CREATE_ABODE(900, "60.0,60.0", "CELTIC_ABODE_F", 4328, 1095, 0, 0)
CREATE_VILLAGER_POS("56.0,62.0", "60.0,60.0", "CELTIC_HOUSEWIFE", 20)
CREATE_VILLAGER_POS("56.0,62.0", "60.0,60.0", "CELTIC_FORESTER", 20)
CREATE_VILLAGER_POS("56.0,62.0", "60.0,60.0", "CELTIC_HOUSEWIFE", 20)
CREATE_VILLAGER_POS("56.0,62.0", "60.0,60.0", "CELTIC_FORESTER", 20)
CREATE_ABODE(900, "40.0,80.0", "CELTIC_ABODE_F", 4328, 1095, 0, 0)
CREATE_VILLAGER_POS("36.0,82.0", "40.0,80.0", "CELTIC_HOUSEWIFE", 20)
CREATE_VILLAGER_POS("36.0,82.0", "40.0,80.0", "CELTIC_FORESTER", 20)
CREATE_VILLAGER_POS("36.0,82.0", "40.0,80.0", "CELTIC_HOUSEWIFE", 20)
CREATE_VILLAGER_POS("36.0,82.0", "40.0,80.0", "CELTIC_FORESTER", 20)
CREATE_ABODE(900, "60.0,80.0", "CELTIC_ABODE_F", 4328, 1095, 0, 0)
CREATE_VILLAGER_POS("56.0,82.0", "60.0,80.0", "CELTIC_HOUSEWIFE", 20)
CREATE_VILLAGER_POS("56.0,82.0", "60.0,80.0", "CELTIC_FORESTER", 20)
CREATE_VILLAGER_POS("56.0,82.0", "60.0,80.0", "CELTIC_HOUSEWIFE", 20)
CREATE_VILLAGER_POS("56.0,82.0", "60.0,80.0", "CELTIC_FORESTER", 20)
CREATE_ABODE(900, "-40.0,60.0", "CELTIC_ABODE_F", 4328, 1095, 0, 0)
CREATE_VILLAGER_POS("-44.0,62.0", "-40.0,60.0", "CELTIC_HOUSEWIFE", 20)
CREATE_VILLAGER_POS("-44.0,62.0", "-40.0,60.0", "CELTIC_FORESTER", 20)
CREATE_VILLAGER_POS("-44.0,62.0", "-40.0,60.0", "CELTIC_HOUSEWIFE", 20)
CREATE_VILLAGER_POS("-44.0,62.0", "-40.0,60.0", "CELTIC_FORESTER", 20)
CREATE_ABODE(900, "-60.0,60.0", "CELTIC_ABODE_F", 4328, 1095, 0, 0)
CREATE_VILLAGER_POS("-64.0,62.0", "-60.0,60.0", "CELTIC_HOUSEWIFE", 20)
CREATE_VILLAGER_POS("-64.0,62.0", "-60.0,60.0", "CELTIC_FORESTER", 20)
CREATE_VILLAGER_POS("-64.0,62.0", "-60.0,60.0", "CELTIC_HOUSEWIFE", 20)
CREATE_VILLAGER_POS("-64.0,62.0", "-60.0,60.0", "CELTIC_FORESTER", 20)
CREATE_ABODE(900, "-40.0,80.0", "CELTIC_ABODE_F", 4328, 1095, 0, 0)
CREATE_VILLAGER_POS("-44.0,82.0", "-40.0,80.0", "CELTIC_HOUSEWIFE", 20)
CREATE_VILLAGER_POS("-44.0,82.0", "-40.0,80.0", "CELTIC_FORESTER", 20)
CREATE_VILLAGER_POS("-44.0,82.0", "-40.0,80.0", "CELTIC_HOUSEWIFE", 20)
CREATE_VILLAGER_POS("-44.0,82.0", "-40.0,80.0", "CELTIC_FORESTER", 20)
CREATE_ABODE(900, "-60.0,80.0", "CELTIC_ABODE_F", 4328, 1095, 0, 0)
CREATE_VILLAGER_POS("-64.0,82.0", "-60.0,80.0", "CELTIC_HOUSEWIFE", 20)
CREATE_VILLAGER_POS("-64.0,82.0", "-60.0,80.0", "CELTIC_FORESTER", 20)
CREATE_VILLAGER_POS("-64.0,82.0", "-60.0,80.0", "CELTIC_HOUSEWIFE", 20)
CREATE_VILLAGER_POS("-64.0,82.0", "-60.0,80.0", "CELTIC_FORESTER", 20)
)";

/// The same town 260 metres further east, so that its storage pit is far from the temple and its worship site
constexpr std::string_view k_ComputerGodFarTownScript = R"(TOGGLE_COMPUTER_PLAYER("PLAYER_TWO", 1)
CREATE_CITADEL("0.0,-90.0", 0, "PLAYER_TWO", 5690, 1000)
CREATE_TOWN(900, "260.0,40.0", "PLAYER_TWO", 0, "CELTIC")
SET_TOWN_BELIEF_CAP(900, "PLAYER_TWO", 2.0)
CREATE_NEW_TOWN_SPELL(900, "FOOD")
SET_TOWN_BELIEF(900, "PLAYER_TWO", 1.0)
CREATE_TOWN_CENTRE(900, "256.0,41.0", "CELTIC_ABODE_TOWN_CENTRE", 0, 1000, 0)
CREATE_ABODE(900, "270.0,90.0", "CELTIC_ABODE_STORAGE_PIT", 6282, 1000, 0, 3000)
CREATE_ABODE(900, "300.0,60.0", "CELTIC_ABODE_F", 4328, 1095, 0, 0)
CREATE_VILLAGER_POS("296.0,62.0", "300.0,60.0", "CELTIC_HOUSEWIFE", 20)
CREATE_VILLAGER_POS("296.0,62.0", "300.0,60.0", "CELTIC_FORESTER", 20)
CREATE_VILLAGER_POS("296.0,62.0", "300.0,60.0", "CELTIC_HOUSEWIFE", 20)
CREATE_VILLAGER_POS("296.0,62.0", "300.0,60.0", "CELTIC_FORESTER", 20)
CREATE_ABODE(900, "320.0,60.0", "CELTIC_ABODE_F", 4328, 1095, 0, 0)
CREATE_VILLAGER_POS("316.0,62.0", "320.0,60.0", "CELTIC_HOUSEWIFE", 20)
CREATE_VILLAGER_POS("316.0,62.0", "320.0,60.0", "CELTIC_FORESTER", 20)
CREATE_VILLAGER_POS("316.0,62.0", "320.0,60.0", "CELTIC_HOUSEWIFE", 20)
CREATE_VILLAGER_POS("316.0,62.0", "320.0,60.0", "CELTIC_FORESTER", 20)
CREATE_ABODE(900, "300.0,80.0", "CELTIC_ABODE_F", 4328, 1095, 0, 0)
CREATE_VILLAGER_POS("296.0,82.0", "300.0,80.0", "CELTIC_HOUSEWIFE", 20)
CREATE_VILLAGER_POS("296.0,82.0", "300.0,80.0", "CELTIC_FORESTER", 20)
CREATE_VILLAGER_POS("296.0,82.0", "300.0,80.0", "CELTIC_HOUSEWIFE", 20)
CREATE_VILLAGER_POS("296.0,82.0", "300.0,80.0", "CELTIC_FORESTER", 20)
CREATE_ABODE(900, "320.0,80.0", "CELTIC_ABODE_F", 4328, 1095, 0, 0)
CREATE_VILLAGER_POS("316.0,82.0", "320.0,80.0", "CELTIC_HOUSEWIFE", 20)
CREATE_VILLAGER_POS("316.0,82.0", "320.0,80.0", "CELTIC_FORESTER", 20)
CREATE_VILLAGER_POS("316.0,82.0", "320.0,80.0", "CELTIC_HOUSEWIFE", 20)
CREATE_VILLAGER_POS("316.0,82.0", "320.0,80.0", "CELTIC_FORESTER", 20)
CREATE_ABODE(900, "220.0,60.0", "CELTIC_ABODE_F", 4328, 1095, 0, 0)
CREATE_VILLAGER_POS("216.0,62.0", "220.0,60.0", "CELTIC_HOUSEWIFE", 20)
CREATE_VILLAGER_POS("216.0,62.0", "220.0,60.0", "CELTIC_FORESTER", 20)
CREATE_VILLAGER_POS("216.0,62.0", "220.0,60.0", "CELTIC_HOUSEWIFE", 20)
CREATE_VILLAGER_POS("216.0,62.0", "220.0,60.0", "CELTIC_FORESTER", 20)
CREATE_ABODE(900, "200.0,60.0", "CELTIC_ABODE_F", 4328, 1095, 0, 0)
CREATE_VILLAGER_POS("196.0,62.0", "200.0,60.0", "CELTIC_HOUSEWIFE", 20)
CREATE_VILLAGER_POS("196.0,62.0", "200.0,60.0", "CELTIC_FORESTER", 20)
CREATE_VILLAGER_POS("196.0,62.0", "200.0,60.0", "CELTIC_HOUSEWIFE", 20)
CREATE_VILLAGER_POS("196.0,62.0", "200.0,60.0", "CELTIC_FORESTER", 20)
CREATE_ABODE(900, "220.0,80.0", "CELTIC_ABODE_F", 4328, 1095, 0, 0)
CREATE_VILLAGER_POS("216.0,82.0", "220.0,80.0", "CELTIC_HOUSEWIFE", 20)
CREATE_VILLAGER_POS("216.0,82.0", "220.0,80.0", "CELTIC_FORESTER", 20)
CREATE_VILLAGER_POS("216.0,82.0", "220.0,80.0", "CELTIC_HOUSEWIFE", 20)
CREATE_VILLAGER_POS("216.0,82.0", "220.0,80.0", "CELTIC_FORESTER", 20)
CREATE_ABODE(900, "200.0,80.0", "CELTIC_ABODE_F", 4328, 1095, 0, 0)
CREATE_VILLAGER_POS("196.0,82.0", "200.0,80.0", "CELTIC_HOUSEWIFE", 20)
CREATE_VILLAGER_POS("196.0,82.0", "200.0,80.0", "CELTIC_FORESTER", 20)
CREATE_VILLAGER_POS("196.0,82.0", "200.0,80.0", "CELTIC_HOUSEWIFE", 20)
CREATE_VILLAGER_POS("196.0,82.0", "200.0,80.0", "CELTIC_FORESTER", 20)
)";

} // namespace

void testbed_scenarios::AddComputerPlayerScenarios(std::vector<Scenario>& all)
{
	all.push_back({
	    .id = "mind.computer_player_start",
	    .name = "A computer player's creature at the start of its mind",
	    .facet = Facet::Mind,
	    .description = "The player's tiger stands to the west at size 1. A temple of player two stands to the north. Lines "
	                   "of a skirmish land switch on the computer mind of player two. They make a leopard for it from the "
	                   "computer's mind file to the east, and name player one for the size of the leopard.",
	    .expected = "At the first turn the leopard grows at once to 0.9 to 1.2 times the size of the tiger. The leopard "
	                "knows heal, food, wood and water, and each miracle that player two can cast. The players.list query "
	                "shows the computer mind of player two as switched on, marked and started.",
	    .creatures = {{.label = "the player's", .species = CreatureType::Tiger, .offset = {-25.0f, 0.0f}, .size = 1.0f}},
	    .temples = {{.offset = {0.0f, 90.0f}, .owner = PlayerNames::PLAYER_TWO}},
	    .landScript = k_ComputerPlayerScript,
	});
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
