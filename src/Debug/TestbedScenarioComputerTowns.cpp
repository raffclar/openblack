/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <vector>

#include "TestbedScenarioComputerPlayers.h"
#include "TestbedScenarioRegistry.h"

using namespace openblack;
using namespace openblack::testbed_scenarios;

namespace
{

/// The lines of a land that give player two a computer mind, a temple and a Celtic town by the lake, with an empty
/// storage pit and two abodes of people. A fish farm of the town stands on the shore.
constexpr std::string_view k_FishFarmTownScript = R"(TOGGLE_COMPUTER_PLAYER("PLAYER_TWO", 1)
CREATE_CITADEL("0.0,-90.0", 0, "PLAYER_TWO", 5690, 1000)
CREATE_TOWN(900, "0.0,100.0", "PLAYER_TWO", 0, "CELTIC")
SET_TOWN_BELIEF_CAP(900, "PLAYER_TWO", 2.0)
SET_TOWN_BELIEF(900, "PLAYER_TWO", 1.0)
CREATE_TOWN_CENTRE(900, "-4.0,101.0", "CELTIC_ABODE_TOWN_CENTRE", 0, 1000, 0)
CREATE_ABODE(900, "30.0,115.0", "CELTIC_ABODE_STORAGE_PIT", 6282, 1000, 0, 3000)
CREATE_ABODE(900, "-30.0,90.0", "CELTIC_ABODE_F", 4328, 1095, 0, 0)
CREATE_VILLAGER_POS("-34.0,92.0", "-30.0,90.0", "CELTIC_HOUSEWIFE", 20)
CREATE_VILLAGER_POS("-34.0,92.0", "-30.0,90.0", "CELTIC_FORESTER", 20)
CREATE_VILLAGER_POS("-34.0,92.0", "-30.0,90.0", "CELTIC_HOUSEWIFE", 20)
CREATE_VILLAGER_POS("-34.0,92.0", "-30.0,90.0", "CELTIC_FORESTER", 20)
CREATE_ABODE(900, "-30.0,70.0", "CELTIC_ABODE_F", 4328, 1095, 0, 0)
CREATE_VILLAGER_POS("-34.0,72.0", "-30.0,70.0", "CELTIC_HOUSEWIFE", 20)
CREATE_VILLAGER_POS("-34.0,72.0", "-30.0,70.0", "CELTIC_FORESTER", 20)
CREATE_VILLAGER_POS("-34.0,72.0", "-30.0,70.0", "CELTIC_HOUSEWIFE", 20)
CREATE_VILLAGER_POS("-34.0,72.0", "-30.0,70.0", "CELTIC_FORESTER", 20)
)";

/// The lines of a land that give player two a computer mind, a temple and two Celtic towns. The east town has no food
/// in its storage pit. The west town, 160 metres away, has 5000 food and 5000 wood in its pit.
constexpr std::string_view k_TwoTownsFoodScript = R"(TOGGLE_COMPUTER_PLAYER("PLAYER_TWO", 1)
CREATE_CITADEL("0.0,-90.0", 0, "PLAYER_TWO", 5690, 1000)
CREATE_TOWN(900, "80.0,40.0", "PLAYER_TWO", 0, "CELTIC")
SET_TOWN_BELIEF_CAP(900, "PLAYER_TWO", 2.0)
SET_TOWN_BELIEF(900, "PLAYER_TWO", 1.0)
CREATE_TOWN_CENTRE(900, "76.0,41.0", "CELTIC_ABODE_TOWN_CENTRE", 0, 1000, 0)
CREATE_ABODE(900, "90.0,90.0", "CELTIC_ABODE_STORAGE_PIT", 6282, 1000, 0, 3000)
CREATE_ABODE(900, "110.0,60.0", "CELTIC_ABODE_F", 4328, 1095, 0, 0)
CREATE_VILLAGER_POS("106.0,62.0", "110.0,60.0", "CELTIC_HOUSEWIFE", 20)
CREATE_VILLAGER_POS("106.0,62.0", "110.0,60.0", "CELTIC_FORESTER", 20)
CREATE_VILLAGER_POS("106.0,62.0", "110.0,60.0", "CELTIC_HOUSEWIFE", 20)
CREATE_VILLAGER_POS("106.0,62.0", "110.0,60.0", "CELTIC_FORESTER", 20)
CREATE_ABODE(900, "110.0,80.0", "CELTIC_ABODE_F", 4328, 1095, 0, 0)
CREATE_VILLAGER_POS("106.0,82.0", "110.0,80.0", "CELTIC_HOUSEWIFE", 20)
CREATE_VILLAGER_POS("106.0,82.0", "110.0,80.0", "CELTIC_FORESTER", 20)
CREATE_VILLAGER_POS("106.0,82.0", "110.0,80.0", "CELTIC_HOUSEWIFE", 20)
CREATE_VILLAGER_POS("106.0,82.0", "110.0,80.0", "CELTIC_FORESTER", 20)
CREATE_TOWN(901, "-80.0,40.0", "PLAYER_TWO", 0, "CELTIC")
SET_TOWN_BELIEF_CAP(901, "PLAYER_TWO", 2.0)
SET_TOWN_BELIEF(901, "PLAYER_TWO", 1.0)
CREATE_TOWN_CENTRE(901, "-84.0,41.0", "CELTIC_ABODE_TOWN_CENTRE", 0, 1000, 0)
CREATE_ABODE(901, "-70.0,90.0", "CELTIC_ABODE_STORAGE_PIT", 6282, 1000, 5000, 5000)
CREATE_ABODE(901, "-110.0,60.0", "CELTIC_ABODE_F", 4328, 1095, 0, 0)
CREATE_VILLAGER_POS("-114.0,62.0", "-110.0,60.0", "CELTIC_HOUSEWIFE", 20)
CREATE_VILLAGER_POS("-114.0,62.0", "-110.0,60.0", "CELTIC_FORESTER", 20)
)";

/// The same two towns, but the east town has 5000 food and no wood in its storage pit
constexpr std::string_view k_TwoTownsWoodScript = R"(TOGGLE_COMPUTER_PLAYER("PLAYER_TWO", 1)
CREATE_CITADEL("0.0,-90.0", 0, "PLAYER_TWO", 5690, 1000)
CREATE_TOWN(900, "80.0,40.0", "PLAYER_TWO", 0, "CELTIC")
SET_TOWN_BELIEF_CAP(900, "PLAYER_TWO", 2.0)
SET_TOWN_BELIEF(900, "PLAYER_TWO", 1.0)
CREATE_TOWN_CENTRE(900, "76.0,41.0", "CELTIC_ABODE_TOWN_CENTRE", 0, 1000, 0)
CREATE_ABODE(900, "90.0,90.0", "CELTIC_ABODE_STORAGE_PIT", 6282, 1000, 5000, 0)
CREATE_ABODE(900, "110.0,60.0", "CELTIC_ABODE_F", 4328, 1095, 0, 0)
CREATE_VILLAGER_POS("106.0,62.0", "110.0,60.0", "CELTIC_HOUSEWIFE", 20)
CREATE_VILLAGER_POS("106.0,62.0", "110.0,60.0", "CELTIC_FORESTER", 20)
CREATE_VILLAGER_POS("106.0,62.0", "110.0,60.0", "CELTIC_HOUSEWIFE", 20)
CREATE_VILLAGER_POS("106.0,62.0", "110.0,60.0", "CELTIC_FORESTER", 20)
CREATE_ABODE(900, "110.0,80.0", "CELTIC_ABODE_F", 4328, 1095, 0, 0)
CREATE_VILLAGER_POS("106.0,82.0", "110.0,80.0", "CELTIC_HOUSEWIFE", 20)
CREATE_VILLAGER_POS("106.0,82.0", "110.0,80.0", "CELTIC_FORESTER", 20)
CREATE_VILLAGER_POS("106.0,82.0", "110.0,80.0", "CELTIC_HOUSEWIFE", 20)
CREATE_VILLAGER_POS("106.0,82.0", "110.0,80.0", "CELTIC_FORESTER", 20)
CREATE_TOWN(901, "-80.0,40.0", "PLAYER_TWO", 0, "CELTIC")
SET_TOWN_BELIEF_CAP(901, "PLAYER_TWO", 2.0)
SET_TOWN_BELIEF(901, "PLAYER_TWO", 1.0)
CREATE_TOWN_CENTRE(901, "-84.0,41.0", "CELTIC_ABODE_TOWN_CENTRE", 0, 1000, 0)
CREATE_ABODE(901, "-70.0,90.0", "CELTIC_ABODE_STORAGE_PIT", 6282, 1000, 5000, 5000)
CREATE_ABODE(901, "-110.0,60.0", "CELTIC_ABODE_F", 4328, 1095, 0, 0)
CREATE_VILLAGER_POS("-114.0,62.0", "-110.0,60.0", "CELTIC_HOUSEWIFE", 20)
CREATE_VILLAGER_POS("-114.0,62.0", "-110.0,60.0", "CELTIC_FORESTER", 20)
)";

/// The two towns of the food scenario, but with no wood in either storage pit and no foresters in the west town. A small
/// forest of six grown pines stands beside the storage pit of the west town.
constexpr std::string_view k_ForestWoodScript = R"(TOGGLE_COMPUTER_PLAYER("PLAYER_TWO", 1)
CREATE_CITADEL("0.0,-90.0", 0, "PLAYER_TWO", 5690, 1000)
CREATE_TOWN(900, "80.0,40.0", "PLAYER_TWO", 0, "CELTIC")
SET_TOWN_BELIEF_CAP(900, "PLAYER_TWO", 2.0)
SET_TOWN_BELIEF(900, "PLAYER_TWO", 1.0)
CREATE_TOWN_CENTRE(900, "76.0,41.0", "CELTIC_ABODE_TOWN_CENTRE", 0, 1000, 0)
CREATE_ABODE(900, "90.0,90.0", "CELTIC_ABODE_STORAGE_PIT", 6282, 1000, 0, 0)
CREATE_ABODE(900, "110.0,60.0", "CELTIC_ABODE_F", 4328, 1095, 0, 0)
CREATE_VILLAGER_POS("106.0,62.0", "110.0,60.0", "CELTIC_HOUSEWIFE", 20)
CREATE_VILLAGER_POS("106.0,62.0", "110.0,60.0", "CELTIC_FORESTER", 20)
CREATE_VILLAGER_POS("106.0,62.0", "110.0,60.0", "CELTIC_HOUSEWIFE", 20)
CREATE_VILLAGER_POS("106.0,62.0", "110.0,60.0", "CELTIC_FORESTER", 20)
CREATE_ABODE(900, "110.0,80.0", "CELTIC_ABODE_F", 4328, 1095, 0, 0)
CREATE_VILLAGER_POS("106.0,82.0", "110.0,80.0", "CELTIC_HOUSEWIFE", 20)
CREATE_VILLAGER_POS("106.0,82.0", "110.0,80.0", "CELTIC_FORESTER", 20)
CREATE_TOWN(901, "-80.0,40.0", "PLAYER_TWO", 0, "CELTIC")
SET_TOWN_BELIEF_CAP(901, "PLAYER_TWO", 2.0)
SET_TOWN_BELIEF(901, "PLAYER_TWO", 1.0)
CREATE_TOWN_CENTRE(901, "-84.0,41.0", "CELTIC_ABODE_TOWN_CENTRE", 0, 1000, 0)
CREATE_ABODE(901, "-70.0,90.0", "CELTIC_ABODE_STORAGE_PIT", 6282, 1000, 5000, 0)
CREATE_ABODE(901, "-110.0,60.0", "CELTIC_ABODE_F", 4328, 1095, 0, 0)
CREATE_VILLAGER_POS("-114.0,62.0", "-110.0,60.0", "CELTIC_HOUSEWIFE", 20)
CREATE_VILLAGER_POS("-114.0,62.0", "-110.0,60.0", "CELTIC_HOUSEWIFE", 20)
CREATE_FOREST(950, "-45.0,105.0")
CREATE_NEW_TREE(950, "-51.0,101.0", 3, 1, 1.0, 1.0, 1.0)
CREATE_NEW_TREE(950, "-45.0,99.0", 3, 1, 2.0, 1.0, 1.0)
CREATE_NEW_TREE(950, "-39.0,102.0", 3, 1, 3.0, 1.0, 1.0)
CREATE_NEW_TREE(950, "-50.0,109.0", 3, 1, 4.0, 1.0, 1.0)
CREATE_NEW_TREE(950, "-44.0,106.0", 3, 1, 5.0, 1.0, 1.0)
CREATE_NEW_TREE(950, "-38.0,110.0", 3, 1, 6.0, 1.0, 1.0)
)";

} // namespace

void testbed_scenarios::AddComputerGodTownsScenarios(std::vector<Scenario>& all)
{
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
	all.push_back({
	    .id = "mind.computer_god_fish_farm_food",
	    .name = "A computer god that takes food from a fish farm for its town",
	    .facet = Facet::Mind,
	    .description = "Lines of a land switch on the computer mind of player two. Player two has a temple to the south "
	                   "and a Celtic town by the lake to the north, with an empty storage pit. A fish farm of the town "
	                   "stands on the shore.",
	    .expected = "The town wants food, and it has no field. The hand of the god moves to the fish farm, takes a pot "
	                "of fish food, and drops the pot in the storage pit of the town. The farm keeps all its fish. The "
	                "players.list query shows the need, the hand and its steps.",
	    .framing = {.shot = Shot::Overview, .include = {glm::vec2(-60.0f, -110.0f), glm::vec2(60.0f, 160.0f)}},
	    .objects = {{.type = FishFarmInfo::Normal, .offset = {5.0f, 145.0f}}},
	    .landScript = k_FishFarmTownScript,
	    .objectsAfterLandScript = true,
	});
	all.push_back({
	    .id = "mind.computer_god_food_from_other_town",
	    .name = "A computer god that takes food from one of its towns to another",
	    .facet = Facet::Mind,
	    .description = "Lines of a land switch on the computer mind of player two. Player two has a temple to the south "
	                   "and two Celtic towns. The east town has no food in its storage pit. The west town has 5000 food "
	                   "and 5000 wood in its storage pit.",
	    .expected = "The east town wants food. The hand of the god moves to the food pile of the west town's pit and "
	                "takes a pot of food from it, as much as the need of the east town asks. Then the hand carries the "
	                "pot to the pit of the east town and drops it in.",
	    .framing = {.shot = Shot::Overview, .include = {glm::vec2(-130.0f, -110.0f), glm::vec2(130.0f, 110.0f)}},
	    .landScript = k_TwoTownsFoodScript,
	});
	all.push_back({
	    .id = "mind.computer_god_wood_from_other_town",
	    .name = "A computer god that takes all the wood of one of its towns for another",
	    .facet = Facet::Mind,
	    .description = "The two towns of the food scenario, but the east town has 5000 food and no wood in its storage "
	                   "pit.",
	    .expected = "The east town wants wood. The hand of the god takes all the wood of the west town's pit in a pot. "
	                "It moves to the east town but the town does not take the pot, so the hand keeps it, as in the "
	                "original game. The hand holds the pot after the action.",
	    .framing = {.shot = Shot::Overview, .include = {glm::vec2(-130.0f, -110.0f), glm::vec2(130.0f, 110.0f)}},
	    .landScript = k_TwoTownsWoodScript,
	});
	all.push_back({
	    .id = "mind.computer_god_wood_from_forest",
	    .name = "A computer god that takes a tree from a forest that it saw for its town",
	    .facet = Facet::Mind,
	    .description = "The two towns of the food scenario, with no wood in either storage pit. The west town has no "
	                   "foresters, so its pit gets no wood. A small forest of six grown pines stands beside the storage "
	                   "pit of the west town. The god sees only the things near its hand.",
	    .expected = "The hand first takes food from the west town's pit to the east town's pit, and beside the west pit "
	                "the god sees the forest. When a town next wants wood most, the hand moves to the centre tree of the "
	                "forest, the grown tree nearest the middle of the forest, and pulls it out. Then the hand carries the "
	                "tree to the storage pit of that town and drops it in, and the wood of the pit goes up. The god takes "
	                "the trees one by one until the forest has no grown trees. The players.list query shows the need "
	                "GetWoodFromForest with the tree, the hand and its steps.",
	    .framing = {.shot = Shot::Overview, .include = {glm::vec2(-130.0f, -110.0f), glm::vec2(130.0f, 120.0f)}},
	    .landScript = k_ForestWoodScript,
	});
}
