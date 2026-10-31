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

/// The lines of a land that give player two a computer mind, a temple and a Celtic town. The town has a storage pit
/// with wood, a workshop with no wood, and two full houses. More people of the town have no home, so the town wants
/// houses.
constexpr std::string_view k_WorkshopTownScript = R"(TOGGLE_COMPUTER_PLAYER("PLAYER_TWO", 1)
CREATE_CITADEL("0.0,-90.0", 0, "PLAYER_TWO", 5690, 1000)
CREATE_TOWN(900, "0.0,40.0", "PLAYER_TWO", 0, "CELTIC")
SET_TOWN_BELIEF_CAP(900, "PLAYER_TWO", 2.0)
SET_TOWN_BELIEF(900, "PLAYER_TWO", 1.0)
CREATE_TOWN_CENTRE(900, "-4.0,41.0", "CELTIC_ABODE_TOWN_CENTRE", 0, 1000, 0)
CREATE_ABODE(900, "10.0,90.0", "CELTIC_ABODE_STORAGE_PIT", 6282, 1000, 3000, 3000)
CREATE_ABODE(900, "-50.0,90.0", "CELTIC_ABODE_WORKSHOP", 0, 1000, 0, 0)
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
CREATE_VILLAGER_POS("0.0,30.0", "-4.0,41.0", "CELTIC_HOUSEWIFE", 20)
CREATE_VILLAGER_POS("2.0,30.0", "-4.0,41.0", "CELTIC_FORESTER", 20)
CREATE_VILLAGER_POS("4.0,30.0", "-4.0,41.0", "CELTIC_HOUSEWIFE", 20)
CREATE_VILLAGER_POS("6.0,30.0", "-4.0,41.0", "CELTIC_FORESTER", 20)
)";

/// The town of the workshop scenarios, but it offers the wood miracle, and its storage pit has no wood
constexpr std::string_view k_WorkshopWoodMiracleTownScript = R"(TOGGLE_COMPUTER_PLAYER("PLAYER_TWO", 1)
CREATE_CITADEL("0.0,-90.0", 0, "PLAYER_TWO", 5690, 1000)
CREATE_TOWN(900, "0.0,40.0", "PLAYER_TWO", 0, "CELTIC")
SET_TOWN_BELIEF_CAP(900, "PLAYER_TWO", 2.0)
CREATE_NEW_TOWN_SPELL(900, "WOOD")
SET_TOWN_BELIEF(900, "PLAYER_TWO", 1.0)
CREATE_TOWN_CENTRE(900, "-4.0,41.0", "CELTIC_ABODE_TOWN_CENTRE", 0, 1000, 0)
CREATE_ABODE(900, "10.0,90.0", "CELTIC_ABODE_STORAGE_PIT", 6282, 1000, 3000, 0)
CREATE_ABODE(900, "-50.0,90.0", "CELTIC_ABODE_WORKSHOP", 0, 1000, 0, 0)
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

/// The game turns that the scenarios wait before the action, so that the town settles first
constexpr uint32_t k_Wait = 50;
/// The share of the town's totem. The tool also builds the worship site at once, so that the god does not take wood to it.
constexpr float k_WorshipShare = 0.5f;
/// The scale of a scaffold of the value of one scaffold and of two scaffolds
constexpr float k_OneScaffold = 0.7f;
constexpr float k_TwoScaffolds = 0.9f;

/// The scenarios show the temple and the god's town
Framing ScaffoldFraming()
{
	return {.shot = Shot::Overview, .include = {glm::vec2(-80.0f, -110.0f), glm::vec2(80.0f, 110.0f)}};
}

} // namespace

void testbed_scenarios::AddComputerGodScaffoldScenarios(std::vector<Scenario>& all)
{
	all.push_back({
	    .id = "mind.computer_god_combine_scaffolds",
	    .name = "A computer god that joins two scaffolds",
	    .facet = Facet::Mind,
	    .description = "Lines of a land switch on the computer mind of player two, which has a temple and a Celtic town. "
	                   "Two loose scaffolds lie between the temple and the town: one of the value of one scaffold to the "
	                   "west, and one of the value of two scaffolds to the east. After five seconds, a testbed tool puts "
	                   "the action CombineScaffolds in the queue of the god, with the two scaffolds.",
	    .expected = "The hand of the god picks up the west scaffold and gives it to the east scaffold. The two scaffolds "
	                "join: the west scaffold goes, and the east scaffold becomes larger, with the value of three "
	                "scaffolds. A puff shows at the joined scaffold, and a sound plays at the hand. The players.list "
	                "query shows the node and the steps of the hand.",
	    .framing = ScaffoldFraming(),
	    .objects = {{.type = ScaffoldInfo::Normal, .offset = {-30.0f, -30.0f}, .scale = k_OneScaffold},
	                {.type = ScaffoldInfo::Normal, .offset = {30.0f, -30.0f}, .scale = k_TwoScaffolds}},
	    .landScript = k_WorkshopTownScript,
	    .worshipShare = k_WorshipShare,
	    .objectsAfterLandScript = true,
	    .computerActions = {{.afterTurns = k_Wait,
	                         .node = "CombineScaffolds",
	                         .first = {.kind = ComputerActionThing::Kind::Object, .object = 0},
	                         .second = {.kind = ComputerActionThing::Kind::Object, .object = 1}}},
	});
	all.push_back({
	    .id = "mind.computer_god_scaffold_build",
	    .name = "A computer god that takes a scaffold to its town to build",
	    .facet = Facet::Mind,
	    .description = "The land of the join scenario, with one loose scaffold of the value of one scaffold between the "
	                   "temple and the town. The town has people with no home. After five seconds, a testbed tool puts "
	                   "the action FindPlaceToUseScaffoldForBuilding in the queue of the god, with the scaffold.",
	    .expected = "The god finds a place round its town where the town offers a building for the scaffold. The hand "
	                "picks up the scaffold, and the phantom of the building shows below it. The hand puts the scaffold "
	                "down at that place, and a building site of a house opens there. The players.list query shows the "
	                "node, its place and the steps of the hand.",
	    .framing = ScaffoldFraming(),
	    .objects = {{.type = ScaffoldInfo::Normal, .offset = {30.0f, -30.0f}, .scale = k_OneScaffold}},
	    .landScript = k_WorkshopTownScript,
	    .worshipShare = k_WorshipShare,
	    .objectsAfterLandScript = true,
	    .computerActions = {{.afterTurns = k_Wait,
	                         .node = "FindPlaceToUseScaffoldForBuilding",
	                         .first = {.kind = ComputerActionThing::Kind::Object, .object = 0}}},
	});
	all.push_back({
	    .id = "mind.computer_god_fill_workshop",
	    .name = "A computer god that gives wood to its workshop",
	    .facet = Facet::Mind,
	    .description = "The land of the join scenario: the workshop of the god's town has no wood, and the storage pit "
	                   "of the town has 3000 wood. After five seconds, a testbed tool puts the action FillWorkshop in the "
	                   "queue of the god, with the workshop.",
	    .expected = "The hand of the god takes wood from the storage pit and gives it to the workshop. The pile of logs "
	                "of the workshop becomes larger, and the workshop starts to make a scaffold when it has the wood for "
	                "one. The players.list query shows the node and the steps of the hand.",
	    .framing = ScaffoldFraming(),
	    .landScript = k_WorkshopTownScript,
	    .worshipShare = k_WorshipShare,
	    .computerActions = {{.afterTurns = k_Wait,
	                         .node = "FillWorkshop",
	                         .first = {.kind = ComputerActionThing::Kind::Workshop, .townId = 900}}},
	});
	all.push_back({
	    .id = "mind.computer_god_workshop_wood_miracle",
	    .name = "A computer god that casts the wood miracle on its workshop",
	    .facet = Facet::Mind,
	    .description = "The land of the workshop scenarios, but the town offers the wood miracle and its storage pit has "
	                   "no wood. Half of the people worship at the site, and each icon of the site holds the full charge "
	                   "of its miracle. At the start, the god gets the weight 0 for the wood need of its towns, so that it "
	                   "keeps the charge. After five seconds, a testbed tool puts the action FillWorkshop in the queue of "
	                   "the god, with the workshop.",
	    .expected = "The god gives its workshop wood with the wood miracle. The hand moves to the wood icon of the site "
	                "and the worshippers chant for the miracle. Then the hand casts the wood miracle on the workshop and "
	                "stays over it for eight seconds. The players.list query shows the FillWorkshop node with the leaf "
	                "CastWoodSpellOn and the steps of the hand.",
	    .framing = ScaffoldFraming(),
	    .landScript = k_WorkshopWoodMiracleTownScript,
	    .commands = {{.kind = Command::Kind::FillSiteIcons, .player = PlayerNames::PLAYER_TWO, .amount = 1.0f},
	                 ComputerWeight("TownWithHighestWoodDesire", 0.0f)},
	    .worshipShare = k_WorshipShare,
	    .computerActions = {{.afterTurns = k_Wait,
	                         .node = "FillWorkshop",
	                         .first = {.kind = ComputerActionThing::Kind::Workshop, .townId = 900}}},
	});
}
