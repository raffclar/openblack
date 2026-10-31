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

/// The same land, with a town that offers the physical shield miracle in place of the shield miracle
constexpr std::string_view k_ComputerGodPhysicalShieldScript = R"(TOGGLE_COMPUTER_PLAYER("PLAYER_TWO", 1)
CREATE_CITADEL("0.0,-90.0", 0, "PLAYER_TWO", 5690, 1000)
CREATE_TOWN(900, "0.0,40.0", "PLAYER_TWO", 0, "CELTIC")
SET_TOWN_BELIEF_CAP(900, "PLAYER_TWO", 2.0)
CREATE_NEW_TOWN_SPELL(900, "PHYSICAL_SHIELD")
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
	    .id = "mind.computer_god_defend_after_fire",
	    .name = "A computer god whose town burns, then defends it with the shield miracle",
	    .facet = Facet::Mind,
	    .description = "Lines of a land switch on the computer mind of player two. Player two has a temple to the south "
	                   "and a Celtic town to the north that offers the shield miracle. A hut of the town burns from the "
	                   "start, so that the town keeps a record of the attack. After 30 seconds, a testbed tool puts the "
	                   "action DefendtownWithSpells in the queue of the god, with the town and the shield icon, as a land "
	                   "script does.",
	    .expected = "The hut burns and the town keeps the record of the fire. The hand of the god moves to the "
	                "shield icon of the site, and the worshippers chant for the miracle. When the charge is full, the "
	                "hand flies to the middle of the town and casts the shield there. The record of the attack stays "
	                "in a saved and loaded game.",
	    .framing = {.shot = Shot::Overview, .include = {glm::vec2(-80.0f, -110.0f), glm::vec2(80.0f, 100.0f)}},
	    .objects = {{.type = AbodeInfo::CelticHut, .offset = {0.0f, 70.0f}, .life = 0.5f, .alight = true}},
	    .landScript = k_ComputerGodDefenceScript,
	    .worshipShare = 0.5f,
	    .objectsAfterLandScript = true,
	    .computerActions = {{.afterTurns = 300,
	                         .node = "DefendtownWithSpells",
	                         .first = {.kind = ComputerActionThing::Kind::Town, .townId = 900},
	                         .second = {.kind = ComputerActionThing::Kind::Icon, .type = MagicType::Shield}}},
	});
	all.push_back({
	    .id = "mind.computer_god_creature_repair",
	    .name = "A computer god that does not send its creature to a damaged building of its town",
	    .facet = Facet::Mind,
	    .description = "Lines of a land switch on the computer mind of player two. Player two has a temple to the south, "
	                   "a Celtic town to the north and a tiger between them. A hut at the middle of the town has half of "
	                   "its life and does not burn. No testbed tool tells the god what to do.",
	    .expected = "The god finds the hut as the most damaged building of its town: the players.computer_nodes query "
	                "shows the hut as the object of CheckTownForDamage and FindDamagedAbode. As in the original game, the "
	                "god never sends the tiger to the hut: GetCreaturetoHelp looks for a town above it and finds the "
	                "hut, so all three nodes stay at 0 (vanilla-bugs 789). The god does its other work.",
	    .framing = {.shot = Shot::Overview, .include = {glm::vec2(-80.0f, -110.0f), glm::vec2(80.0f, 100.0f)}},
	    .creatures = {{.label = "player two's",
	                   .species = CreatureType::Tiger,
	                   .offset = {30.0f, -30.0f},
	                   .owner = PlayerNames::PLAYER_TWO,
	                   .size = 1.0f}},
	    .objects = {{.type = AbodeInfo::CelticHut, .offset = {0.0f, 70.0f}, .life = 0.5f}},
	    .landScript = k_ComputerGodDefenceScript,
	    .worshipShare = 0.5f,
	    .objectsAfterLandScript = true,
	});
	all.push_back({
	    .id = "mind.computer_god_defend_town_with_creature",
	    .name = "A computer god that sends its creature to defend its town",
	    .facet = Facet::Mind,
	    .description = "The land of the shield defence scenario, with the tiger of player two to the east of the temple. "
	                   "After three seconds, a testbed tool puts the action DefendTown in the queue of the god, with the "
	                   "town and no second thing, as the land script of the big fight does. The town has no record of "
	                   "an attack, so the shield is not wanted.",
	    .expected = "The god chooses the creature defence. The tiger walks to the storage pit of the town, at the speed "
	                "for a long or a short journey. The hand puts the leash on the tiger and ties the leash to that "
	                "storage pit, as when the god sends its creature to help a town. The game log shows the action "
	                "DefendTownWithCreature, and the leash of the tiger goes to the storage pit.",
	    .framing = {.shot = Shot::Overview, .include = {glm::vec2(-80.0f, -110.0f), glm::vec2(120.0f, 100.0f)}},
	    .creatures = {{.label = "player two's",
	                   .species = CreatureType::Tiger,
	                   .offset = {90.0f, -60.0f},
	                   .owner = PlayerNames::PLAYER_TWO,
	                   .size = 1.0f}},
	    .landScript = k_ComputerGodDefenceScript,
	    .worshipShare = 0.5f,
	    .computerActions = {{.afterTurns = 30,
	                         .node = "DefendTown",
	                         .first = {.kind = ComputerActionThing::Kind::Town, .townId = 900}}},
	});
	all.push_back({
	    .id = "saves.computer_god_mind",
	    .name = "A computer god whose mind starts again after the load of a saved game",
	    .facet = Facet::Mind,
	    .description = "Lines of a land switch on the computer mind of player two, with a temple and a Celtic town. After "
	                   "30 seconds, a testbed tool puts the action DefendtownWithSpells in the queue of the god. Three "
	                   "seconds later the game saves into slot 0, and two seconds after that it loads slot 0.",
	    .expected = "Before the save, the players.list query shows the turns at which the nodes of the god were last "
	                "looked at and acted on. After the load these turns are 0, and the lowered nodes come back at the "
	                "first look. The weights and the suppressions stay. The hand keeps its steps and goes on with the "
	                "shield miracle.",
	    .framing = {.shot = Shot::Overview, .include = {glm::vec2(-80.0f, -110.0f), glm::vec2(80.0f, 100.0f)}},
	    .landScript = k_ComputerGodDefenceScript,
	    .commands = {{.kind = Command::Kind::SaveGame, .delaySeconds = 33.0f, .value = 0},
	                 {.kind = Command::Kind::LoadGame, .delaySeconds = 2.0f, .value = 0}},
	    .worshipShare = 0.5f,
	    .computerActions = {{.afterTurns = 300,
	                         .node = "DefendtownWithSpells",
	                         .first = {.kind = ComputerActionThing::Kind::Town, .townId = 900},
	                         .second = {.kind = ComputerActionThing::Kind::Icon, .type = MagicType::Shield}}},
	});
	all.push_back({
	    .id = "mind.computer_god_physical_shield_after_throw",
	    .name = "A computer god that protects its temple with the physical shield after a throw",
	    .facet = Facet::Mind,
	    .description = "Lines of a land switch on the computer mind of player two. Player two has a temple to the south "
	                   "and a Celtic town to the north that offers the physical shield miracle. Half of the people go to "
	                   "worship at the site. After 30 seconds, a testbed tool puts the action ReactToPlayerThrowingStuff "
	                   "in the queue of the god, with the heart of its temple, as when a player throws a thing at the "
	                   "temple. The god has no creature.",
	    .expected = "The god works out the two choices under the action: the physical shield and the creature as a "
	                "diversion. With no creature, it chooses the physical shield. The hand moves to the physical shield "
	                "icon of the site, and the worshippers chant for the miracle. When the charge is full, the hand "
	                "flies to the temple and casts the physical shield over the heart, a little wider than the heart. "
	                "The players.computer_nodes query shows CastPhysicalShieldToProtect with the icon as its object.",
	    .framing = {.shot = Shot::Overview, .include = {glm::vec2(-80.0f, -110.0f), glm::vec2(80.0f, 100.0f)}},
	    .landScript = k_ComputerGodPhysicalShieldScript,
	    .worshipShare = 0.5f,
	    .computerActions = {{.afterTurns = 300,
	                         .node = "ReactToPlayerThrowingStuff",
	                         .first = {.kind = ComputerActionThing::Kind::TempleHeart}}},
	});
	all.push_back({
	    .id = "mind.computer_god_camera_follows_hand",
	    .name = "A script camera that follows the hand of a computer god",
	    .facet = Facet::Mind,
	    .description = "The land of the shield defence scenario. After 30 seconds, a testbed tool puts the action "
	                   "DefendtownWithSpells in the queue of the god. One second later a script takes the camera, tells "
	                   "it to look at the hand of player two and to follow that hand, and gives the camera a distance of "
	                   "60 and a heading of 30 degrees.",
	    .expected = "The camera looks at the hand of the god and moves with it: first to the shield icon of the site, "
	                "then to the middle of the town. The camera stands 60 back from the hand and looks down on it. "
	                "When the camera only follows the hand and looks at no thing, it stands back from the point that it "
	                "looks at, as in the original game.",
	    .framing = {.shot = Shot::Overview, .include = {glm::vec2(-80.0f, -110.0f), glm::vec2(80.0f, 100.0f)}},
	    .landScript = k_ComputerGodDefenceScript,
	    .commands = {{.kind = Command::Kind::CallNative, .delaySeconds = 31.0f, .script = "START_CAMERA_CONTROL"},
	                 {.kind = Command::Kind::CallNative,
	                  .delaySeconds = 0.0f,
	                  .script = "SET_FOCUS_FOLLOW_COMPUTER_PLAYER",
	                  .scriptArguments = {{.number = 2.0f}}},
	                 {.kind = Command::Kind::CallNative,
	                  .delaySeconds = 0.0f,
	                  .script = "SET_POSITION_FOLLOW_COMPUTER_PLAYER",
	                  .scriptArguments = {{.number = 2.0f}}},
	                 {.kind = Command::Kind::CallNative,
	                  .delaySeconds = 0.0f,
	                  .script = "CAMERA_PROPERTIES",
	                  .scriptArguments = {{.number = 60.0f},
	                                      {.number = 0.2f},
	                                      {.number = 30.0f},
	                                      {.kind = ScriptArgument::Kind::Integer, .number = 0.0f}}}},
	    .worshipShare = 0.5f,
	    .computerActions = {{.afterTurns = 300,
	                         .node = "DefendtownWithSpells",
	                         .first = {.kind = ComputerActionThing::Kind::Town, .townId = 900},
	                         .second = {.kind = ComputerActionThing::Kind::Icon, .type = MagicType::Shield}}},
	});
}
