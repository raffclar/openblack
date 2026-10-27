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

/// The lines of a land that give player two a computer mind, a temple and a Celtic town that offers the shield miracle,
/// with two abodes of people
constexpr std::string_view k_ComputerGodDefenceScript = R"(TOGGLE_COMPUTER_PLAYER("PLAYER_TWO", 1)
CREATE_CITADEL("0.0,-90.0", 0, "PLAYER_TWO", 5690, 1000)
CREATE_TOWN(900, "0.0,40.0", "PLAYER_TWO", 0, "CELTIC")
SET_TOWN_BELIEF_CAP(900, "PLAYER_TWO", 2.0)
CREATE_NEW_TOWN_SPELL(900, "SHIELD")
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

} // namespace

void testbed_scenarios::AddComputerGodDefenceScenarios(std::vector<Scenario>& all)
{
	all.push_back({
	    .id = "mind.computer_god_defend_town_with_shield",
	    .name = "A computer god that defends its town with the shield miracle",
	    .facet = Facet::Mind,
	    .description = "Lines of a land switch on the computer mind of player two. Player two has a temple to the south "
	                   "and a Celtic town to the north that offers the shield miracle. Half of the people go to worship "
	                   "at the site, which stands built. After 30 seconds, a testbed tool puts the action "
	                   "DefendtownWithSpells in the queue of the god, with the town and the shield icon, as a land script "
	                   "does.",
	    .expected = "The hand moves to the shield icon of the site, and the worshippers chant for the miracle. When the "
	                "charge is full, the hand flies to the middle of the town and casts the shield there, as wide as the "
	                "town. The players.list query shows the steps of the hand.",
	    .framing = {.shot = Shot::Overview, .include = {glm::vec2(-80.0f, -110.0f), glm::vec2(80.0f, 100.0f)}},
	    .landScript = k_ComputerGodDefenceScript,
	    .worshipShare = 0.5f,
	    .computerActions = {{.afterTurns = 300,
	                         .node = "DefendtownWithSpells",
	                         .first = {.kind = ComputerActionThing::Kind::Town, .townId = 900},
	                         .second = {.kind = ComputerActionThing::Kind::Icon, .type = MagicType::Shield}}},
	});
	all.push_back({
	    .id = "mind.computer_god_defend_belief",
	    .name = "A computer god that defends the belief of its town with the shield miracle",
	    .facet = Facet::Mind,
	    .description = "Lines of a land switch on the computer mind of player two. Player two has a temple to the south "
	                   "and a Celtic town to the north that offers the shield miracle. Half of the people go to worship "
	                   "at the site, which stands built. After 30 seconds, a testbed tool puts the action "
	                   "CastSpellToDefendAgainstBelief in the queue of the god, with the town and the shield icon, as a "
	                   "land script does.",
	    .expected = "The hand moves to the shield icon of the site, and the worshippers chant for the miracle. When the "
	                "charge is full, the hand flies to the middle of the town and casts the shield there, as wide as the "
	                "town. The players.list query shows the steps of the hand.",
	    .framing = {.shot = Shot::Overview, .include = {glm::vec2(-80.0f, -110.0f), glm::vec2(80.0f, 100.0f)}},
	    .landScript = k_ComputerGodDefenceScript,
	    .worshipShare = 0.5f,
	    .computerActions = {{.afterTurns = 300,
	                         .node = "CastSpellToDefendAgainstBelief",
	                         .first = {.kind = ComputerActionThing::Kind::Town, .townId = 900},
	                         .second = {.kind = ComputerActionThing::Kind::Icon, .type = MagicType::Shield}}},
	});
	all.push_back({
	    .id = "mind.computer_god_find_damaged_abode",
	    .name = "A computer god that goes to a damaged hut of its town",
	    .facet = Facet::Mind,
	    .description = "Lines of a land switch on the computer mind of player two. Player two has a temple to the south "
	                   "and a Celtic town to the north. A hut of the town at the middle of the town has half of its life. "
	                   "After 30 seconds, a testbed tool puts the action FindDamagedAbode in the queue of the god, with "
	                   "the hut, as a land script does.",
	    .expected = "The hand of the god flies to the damaged hut and stays near it. The players.list query shows the "
	                "steps of the hand.",
	    .framing = {.shot = Shot::Overview, .include = {glm::vec2(-80.0f, -110.0f), glm::vec2(80.0f, 100.0f)}},
	    .objects = {{.type = AbodeInfo::CelticHut, .offset = {0.0f, 70.0f}, .life = 0.5f}},
	    .landScript = k_ComputerGodDefenceScript,
	    .objectsAfterLandScript = true,
	    .computerActions = {{.afterTurns = 300,
	                         .node = "FindDamagedAbode",
	                         .first = {.kind = ComputerActionThing::Kind::Object, .object = 0}}},
	});
}
