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

#include "TestbedScenarioComputerPlayers.h"
#include "TestbedScenarioRegistry.h"

using namespace openblack;
using namespace openblack::testbed_scenarios;

namespace
{

/// The lines of a land that give player two a computer mind, a temple and a Celtic town that offers the food miracle,
/// with a full storage pit and two abodes of people
constexpr std::string_view k_FoodMiracleTownScript = R"(TOGGLE_COMPUTER_PLAYER("PLAYER_TWO", 1)
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
)";

/// The lines of a land that give player two a computer mind, a temple and a Celtic town that offers the heal miracle,
/// with a full storage pit and eight abodes of people, so that enough dancers charge the miracle
constexpr std::string_view k_HealMiracleTownScript = R"(TOGGLE_COMPUTER_PLAYER("PLAYER_TWO", 1)
CREATE_CITADEL("0.0,-90.0", 0, "PLAYER_TWO", 5690, 1000)
CREATE_TOWN(900, "0.0,40.0", "PLAYER_TWO", 0, "CELTIC")
SET_TOWN_BELIEF_CAP(900, "PLAYER_TWO", 2.0)
CREATE_NEW_TOWN_SPELL(900, "HEAL")
SET_TOWN_BELIEF(900, "PLAYER_TWO", 1.0)
CREATE_TOWN_CENTRE(900, "-4.0,41.0", "CELTIC_ABODE_TOWN_CENTRE", 0, 1000, 0)
CREATE_ABODE(900, "10.0,90.0", "CELTIC_ABODE_STORAGE_PIT", 6282, 1000, 3000, 3000)
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

/// The lines of a land that give player two a computer mind, a temple and a Celtic town that offers the wood miracle,
/// with a full storage pit and two abodes of people
constexpr std::string_view k_WoodMiracleTownScript = R"(TOGGLE_COMPUTER_PLAYER("PLAYER_TWO", 1)
CREATE_CITADEL("0.0,-90.0", 0, "PLAYER_TWO", 5690, 1000)
CREATE_TOWN(900, "0.0,40.0", "PLAYER_TWO", 0, "CELTIC")
SET_TOWN_BELIEF_CAP(900, "PLAYER_TWO", 2.0)
CREATE_NEW_TOWN_SPELL(900, "WOOD")
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
)";

/// The lines of a land that give player two a computer mind, a temple and a Celtic town with no miracle, so that the
/// worship site of the town is not built at the start. The storage pit of the town has wood, and two abodes have people.
constexpr std::string_view k_UnbuiltSiteTownScript = R"(TOGGLE_COMPUTER_PLAYER("PLAYER_TWO", 1)
CREATE_CITADEL("0.0,-90.0", 0, "PLAYER_TWO", 5690, 1000)
CREATE_TOWN(900, "0.0,40.0", "PLAYER_TWO", 0, "CELTIC")
SET_TOWN_BELIEF_CAP(900, "PLAYER_TWO", 2.0)
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
)";

/// The lines of a land that give player two a computer mind, a temple and two Celtic towns with no miracle. The east
/// town has food but no wood in its storage pit. The west town, 160 metres away, has 5000 food and 5000 wood in its pit.
constexpr std::string_view k_TwoTownsSiteWoodScript = R"(TOGGLE_COMPUTER_PLAYER("PLAYER_TWO", 1)
CREATE_CITADEL("0.0,-90.0", 0, "PLAYER_TWO", 5690, 1000)
CREATE_TOWN(900, "80.0,40.0", "PLAYER_TWO", 0, "CELTIC")
SET_TOWN_BELIEF_CAP(900, "PLAYER_TWO", 2.0)
SET_TOWN_BELIEF(900, "PLAYER_TWO", 1.0)
CREATE_TOWN_CENTRE(900, "76.0,41.0", "CELTIC_ABODE_TOWN_CENTRE", 0, 1000, 0)
CREATE_ABODE(900, "90.0,90.0", "CELTIC_ABODE_STORAGE_PIT", 6282, 1000, 3000, 0)
CREATE_ABODE(900, "110.0,60.0", "CELTIC_ABODE_F", 4328, 1095, 0, 0)
CREATE_VILLAGER_POS("106.0,62.0", "110.0,60.0", "CELTIC_HOUSEWIFE", 20)
CREATE_VILLAGER_POS("106.0,62.0", "110.0,60.0", "CELTIC_FORESTER", 20)
CREATE_VILLAGER_POS("106.0,62.0", "110.0,60.0", "CELTIC_HOUSEWIFE", 20)
CREATE_VILLAGER_POS("106.0,62.0", "110.0,60.0", "CELTIC_FORESTER", 20)
CREATE_TOWN(901, "-80.0,40.0", "PLAYER_TWO", 0, "CELTIC")
SET_TOWN_BELIEF_CAP(901, "PLAYER_TWO", 2.0)
SET_TOWN_BELIEF(901, "PLAYER_TWO", 1.0)
CREATE_TOWN_CENTRE(901, "-84.0,41.0", "CELTIC_ABODE_TOWN_CENTRE", 0, 1000, 0)
CREATE_ABODE(901, "-70.0,90.0", "CELTIC_ABODE_STORAGE_PIT", 6282, 1000, 5000, 5000)
CREATE_ABODE(901, "-110.0,60.0", "CELTIC_ABODE_F", 4328, 1095, 0, 0)
CREATE_VILLAGER_POS("-114.0,62.0", "-110.0,60.0", "CELTIC_HOUSEWIFE", 20)
CREATE_VILLAGER_POS("-114.0,62.0", "-110.0,60.0", "CELTIC_FORESTER", 20)
)";

/// The two towns of the site wood scenario, but with no wood in either storage pit and no foresters in the west town.
/// A small forest of six grown pines stands beside the storage pit of the west town.
constexpr std::string_view k_TwoTownsSiteForestScript = R"(TOGGLE_COMPUTER_PLAYER("PLAYER_TWO", 1)
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

/// A hut of the east town of the two towns scenarios that stands a fifth built north of the town
std::vector<ObjectSetup> HutOfTheEastTown()
{
	return {{.type = AbodeInfo::CelticHut, .offset = {80.0f, 115.0f}, .built = 0.2f}};
}

/// The god gives the hut wood only: the needs of the towns for food and wood and of the hut for workers get the weight 0
std::vector<Command> OnlySiteWood()
{
	return {ComputerWeight("TownWithHighestFoodDesire", 0.0f), ComputerWeight("TownWithHighestWoodDesire", 0.0f),
	        ComputerWeight("GiveBuildingSiteWorkers", 0.0f)};
}

} // namespace

void testbed_scenarios::AddComputerGodWorshipScenarios(std::vector<Scenario>& all)
{
	all.push_back({
	    .id = "mind.computer_god_site_workers",
	    .name = "A computer god that carries a free villager to a building site of its town",
	    .facet = Facet::Mind,
	    .description = "Lines of a land switch on the computer mind of player two, with a temple to the south and a "
	                   "Celtic town to the north. A hut of the town stands a fifth built north of the town, with a site "
	                   "for its builders.",
	    .expected = "The god finds the free adult of the town that is best for the work, nearest its hand and with the "
	                "most life. The hand picks it up and puts it down beside the hut, a quarter more than the hut's "
	                "radius from its middle, at a point with wood on it if there is one. The players.list query shows "
	                "the GiveBuildingSiteWorkers need and the steps.",
	    .framing = {.shot = Shot::Overview, .include = {glm::vec2(-80.0f, -110.0f), glm::vec2(80.0f, 130.0f)}},
	    .objects = {{.type = AbodeInfo::CelticHut, .offset = {0.0f, 115.0f}, .built = 0.2f}},
	    .landScript = k_ComputerGodTownScript,
	    .objectsAfterLandScript = true,
	});
	all.push_back({
	    .id = "mind.computer_god_tired_worshippers",
	    .name = "A computer god that takes its tired worshippers home",
	    .facet = Facet::Mind,
	    .description = "The town of the food miracle scenario sends all of its people to worship at its built site. "
	                   "Each chant costs a dancer some of its life.",
	    .expected = "As the dancers chant they lose life. When the dancers want rest by more than a quarter, the hand "
	                "picks up the worshipper with the least life and puts it down outside its home, on the side of the "
	                "door, a quarter more than the home's radius from its middle. The players.list query shows the "
	                "GiveWorshippersRest need and the steps.",
	    .framing = {.shot = Shot::Overview, .include = {glm::vec2(-80.0f, -110.0f), glm::vec2(80.0f, 100.0f)}},
	    .landScript = k_ComputerGodTownScript,
	    .worshipShare = 1.0f,
	});
	all.push_back({
	    .id = "mind.computer_god_breeder",
	    .name = "A computer god that makes a breeder for its town",
	    .facet = Facet::Mind,
	    .description = "The town of the food miracle scenario, with no worship. The town has no breeders.",
	    .expected = "The god picks a town of its own with fewer than two breeders. It looks for an adult in the "
	                "homes of that town that is of an age to make love and that the hand cannot reach, such as one "
	                "inside its home, a man first. It makes that villager a breeder at once, picks it up and puts it "
	                "down where it stood. The villager glows as a disciple. The players.list query shows the "
	                "MakeDiscipleBreader need.",
	    .framing = {.shot = Shot::Overview, .include = {glm::vec2(-80.0f, -110.0f), glm::vec2(80.0f, 100.0f)}},
	    .landScript = k_ComputerGodTownScript,
	});
	all.push_back({
	    .id = "mind.computer_god_worshippers_food",
	    .name = "A computer god that carries food from a field to its hungry worshippers",
	    .facet = Facet::Mind,
	    .description = "The town of the field food scenario, 260 metres east of the temple, with two ripe wheat fields. "
	                   "Half of its people worship at the site, which stands built to the west.",
	    .expected = "When the worshippers get hungry and the site's pot has less food than they need, the hand takes the "
	                "food of a field in a pot and drops the pot on the worship site. The players.list query shows the "
	                "GiveWorshippersFood need, with the food that the site needs from its towns as the node's amount.",
	    .framing = {.shot = Shot::Overview, .include = {glm::vec2(-80.0f, -110.0f), glm::vec2(340.0f, 140.0f)}},
	    .objects = {{.type = FieldTypeInfo::Wheat, .offset = {240.0f, 125.0f}, .ripeCrop = true},
	                {.type = FieldTypeInfo::Wheat, .offset = {300.0f, 125.0f}, .ripeCrop = true}},
	    .landScript = k_ComputerGodFarTownScript,
	    .worshipShare = 0.5f,
	    .objectsAfterLandScript = true,
	});
	all.push_back({
	    .id = "mind.computer_god_food_miracle_at_site",
	    .name = "A computer god that casts the food miracle where its worship site keeps its food",
	    .facet = Facet::Mind,
	    .description = "Lines of a land switch on the computer mind of player two. Player two has a temple to the south "
	                   "and a Celtic town to the north that offers the food miracle. The storage pit of the town is full. "
	                   "All of the people go to worship at the site, which stands built. No field is near.",
	    .expected = "When the worshippers get hungry, the god wants to give them food. The hand moves to the food icon "
	                "of the site, and the worshippers chant for the miracle. When the charge is full, the hand casts the "
	                "food miracle at the place where the site keeps its food. Then the hand moves to that place and casts "
	                "the food miracle again, as a shield cast (see docs/vanilla-bugs.md). The players.list query shows "
	                "the GiveWorshippersFood need and the steps.",
	    .framing = {.shot = Shot::Overview, .include = {glm::vec2(-80.0f, -110.0f), glm::vec2(80.0f, 100.0f)}},
	    .landScript = k_FoodMiracleTownScript,
	    .worshipShare = 1.0f,
	});
	all.push_back({
	    .id = "mind.computer_god_heal_miracle_at_site",
	    .name = "A computer god that casts the heal miracle on its weakest worshipper",
	    .facet = Facet::Mind,
	    .description = "Lines of a land switch on the computer mind of player two. Player two has a temple to the south "
	                   "and a Celtic town to the north that offers the heal miracle. All of the people go to worship at the "
	                   "site, which stands built. Each chant costs a dancer some of its life.",
	    .expected = "When the dancers want rest, the god wants to heal them. The hand moves to the heal icon of the "
	                "site, and the worshippers chant for the miracle. When the charge is full, the hand casts the heal "
	                "miracle on the worshipper with the least life. Then the hand moves there and casts the heal miracle "
	                "again, as a shield cast (see docs/vanilla-bugs.md). The players.list query shows the "
	                "GiveWorshippersRest need and the steps.",
	    .framing = {.shot = Shot::Overview, .include = {glm::vec2(-80.0f, -110.0f), glm::vec2(80.0f, 100.0f)}},
	    .landScript = k_HealMiracleTownScript,
	    .worshipShare = 1.0f,
	});
	all.push_back({
	    .id = "mind.computer_god_wood_miracle_at_site",
	    .name = "A computer god that casts the wood miracle at a building site of its town",
	    .facet = Facet::Mind,
	    .description = "Lines of a land switch on the computer mind of player two. Player two has a temple to the south "
	                   "and a Celtic town to the north that offers the wood miracle. The storage pit of the town is full. A "
	                   "hut of the town stands a fifth built north of the town, with a site for its builders. Half of the "
	                   "people go to worship at the site, which stands built.",
	    .expected = "The god wants wood for the hut. The hand moves to the wood icon of the site, and the worshippers "
	                "chant for the miracle. When the charge is full, the hand casts the wood miracle at the hut. Then the "
	                "hand moves there and casts the wood miracle again, as a shield cast (see docs/vanilla-bugs.md). The "
	                "players.list query shows the GiveBuildingSiteWood need and the steps.",
	    .framing = {.shot = Shot::Overview, .include = {glm::vec2(-80.0f, -110.0f), glm::vec2(80.0f, 130.0f)}},
	    .objects = {{.type = AbodeInfo::CelticHut, .offset = {0.0f, 115.0f}, .built = 0.2f}},
	    .landScript = k_WoodMiracleTownScript,
	    .worshipShare = 0.5f,
	    .objectsAfterLandScript = true,
	});
	all.push_back({
	    .id = "mind.computer_god_unbuilt_worship_site",
	    .name = "A computer god that holds wood over a worship site that it built at once",
	    .facet = Facet::Mind,
	    .description = "Lines of a land switch on the computer mind of player two. Player two has a temple to the south "
	                   "and a Celtic town to the north with no miracle, so the worship site of the town is not built at "
	                   "the start. The storage pit of the town has wood.",
	    .expected = "At the first turn the god wants wood for the worship site. In the same turn the god builds the site "
	                "at once, because the site is not damaged. The hand takes all the wood of the storage pit, moves "
	                "over the site and holds the wood there. The built site does not take wood, so the hand keeps it and "
	                "no step follows (see docs/vanilla-bugs.md). The players.list query shows the needs and the steps.",
	    .framing = {.shot = Shot::Overview, .include = {glm::vec2(-80.0f, -110.0f), glm::vec2(80.0f, 110.0f)}},
	    .landScript = k_UnbuiltSiteTownScript,
	});
	all.push_back({
	    .id = "mind.computer_god_builders_stay_at_site",
	    .name = "A computer god whose carried villagers become builders and stay at the site",
	    .facet = Facet::Mind,
	    .description = "Lines of a land switch on the computer mind of player two. Player two has a temple to the south "
	                   "and a Celtic town to the north with no miracle, so the worship site of the town is not built at "
	                   "the start. The storage pit of the town has wood. A hut of the town stands a fifth built north of "
	                   "the town, with a site for its builders.",
	    .expected = "The god takes the wood of the storage pit to the hut. Then the god takes free adults of the town to "
	                "the hut, one at a time. In the hand of the god, each adult becomes a builder, because the hand of "
	                "the god is the hand of its own player. Each builder stays at the hut and builds it. The god does not "
	                "take the same adult again. The players.list query shows the needs and the steps.",
	    .framing = {.shot = Shot::Overview, .include = {glm::vec2(-80.0f, -110.0f), glm::vec2(80.0f, 130.0f)}},
	    .objects = {{.type = AbodeInfo::CelticHut, .offset = {0.0f, 115.0f}, .built = 0.2f}},
	    .landScript = k_UnbuiltSiteTownScript,
	    .objectsAfterLandScript = true,
	});
	all.push_back({
	    .id = "mind.computer_god_worshippers_fish_farm",
	    .name = "A computer god that takes fish food to its hungry worshippers",
	    .facet = Facet::Mind,
	    .description = "The land of the fish farm scenario: a town by the lake with a fish farm on the shore and no field. "
	                   "Half of its people worship at the site, which stands built. At the start, the god gets the weight 0 "
	                   "for the food need of its towns.",
	    .expected = "When the worshippers get hungry and the pot of the site has less food than they need, the hand takes "
	                "a pot of fish food from the fish farm and drops it on the worship site. The farm keeps all its fish. "
	                "The players.list query shows the GiveWorshippersFood need with the leaf GetFoodFromFishFarm.",
	    .framing = {.shot = Shot::Overview, .include = {glm::vec2(-60.0f, -110.0f), glm::vec2(60.0f, 160.0f)}},
	    .objects = {{.type = FishFarmInfo::Normal, .offset = {5.0f, 145.0f}}},
	    .landScript = k_ComputerGodFishFarmTownScript,
	    .commands = {ComputerWeight("TownWithHighestFoodDesire", 0.0f)},
	    .worshipShare = 0.5f,
	    .objectsAfterLandScript = true,
	});
	all.push_back({
	    .id = "mind.computer_god_worshippers_food_from_other_town",
	    .name = "A computer god that takes food from a town to its hungry worshippers",
	    .facet = Facet::Mind,
	    .description = "The land of the scenario of food from another town: the east town has no food in its storage pit, "
	                   "and the west town has 5000 food. No field is near. Half of the people of each town worship at its "
	                   "site, which stands built. At the start, the god gets the weight 0 for the food and wood needs of its "
	                   "towns.",
	    .expected = "When the worshippers of a site get hungry and the pot of the site has less food than they need, the "
	                "hand moves to the food pile of a storage pit that has food, takes a pot of food from it, as much as "
	                "the site needs from its towns, and drops the pot on the worship site. The players.list query shows "
	                "the GiveWorshippersFood need with the leaf GetFoodFromOtherTownForObject.",
	    .framing = {.shot = Shot::Overview, .include = {glm::vec2(-130.0f, -110.0f), glm::vec2(130.0f, 110.0f)}},
	    .landScript = k_ComputerGodTwoTownsFoodScript,
	    .commands = {ComputerWeight("TownWithHighestFoodDesire", 0.0f), ComputerWeight("TownWithHighestWoodDesire", 0.0f)},
	    .worshipShare = 0.5f,
	});
	all.push_back({
	    .id = "mind.computer_god_site_wood_from_other_town",
	    .name = "A computer god that takes the wood of one town to a building site of another",
	    .facet = Facet::Mind,
	    .description = "Lines of a land switch on the computer mind of player two, with two Celtic towns and no miracle. "
	                   "The east town has no wood in its storage pit, and the west town has 5000 wood. A hut of the east "
	                   "town stands a fifth built north of the town, with a site for its builders. At the start, the god "
	                   "gets the weight 0 for the food and wood needs of its towns and for the workers of a site.",
	    .expected = "The god wants wood for the hut. The hand takes all the wood of the storage pit of the west town in a "
	                "pot, moves to the hut and drops the pot on it. The players.list query shows the GiveBuildingSiteWood "
	                "need with the leaf GetWoodFromOtherTown.",
	    .framing = {.shot = Shot::Overview, .include = {glm::vec2(-130.0f, -110.0f), glm::vec2(130.0f, 130.0f)}},
	    .objects = HutOfTheEastTown(),
	    .landScript = k_TwoTownsSiteWoodScript,
	    .commands = OnlySiteWood(),
	    .objectsAfterLandScript = true,
	});
	all.push_back({
	    .id = "mind.computer_god_site_wood_from_forest",
	    .name = "A computer god that takes a tree from a forest that it saw to a building site",
	    .facet = Facet::Mind,
	    .description = "Lines of a land switch on the computer mind of player two, with two Celtic towns and no miracle. "
	                   "Neither storage pit has wood. The west town has 5000 food, and a small forest of six grown pines "
	                   "stands beside its storage pit. A hut of the east town stands a fifth built north of the town, with "
	                   "a site for its builders. The god sees only the things near its hand. At the start, the god gets the "
	                   "weight 0 for the wood need of its towns and for the workers of a site.",
	    .expected = "The god wants wood for the hut, but at first it knows no wood. The hand first takes food from the "
	                "storage pit of the west town to the pit of the east town, and beside the west pit the god sees the "
	                "forest. Then the hand moves to the centre tree of the forest, pulls it out, carries "
	                "it to the door of the hut and drops it there. The players.list query shows the GiveBuildingSiteWood "
	                "need with the leaf GetWoodFromForest.",
	    .framing = {.shot = Shot::Overview, .include = {glm::vec2(-130.0f, -110.0f), glm::vec2(130.0f, 130.0f)}},
	    .objects = HutOfTheEastTown(),
	    .landScript = k_TwoTownsSiteForestScript,
	    .commands = {ComputerWeight("TownWithHighestWoodDesire", 0.0f), ComputerWeight("GiveBuildingSiteWorkers", 0.0f)},
	    .objectsAfterLandScript = true,
	});
}
