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

/// The lines of a land that give player two only a computer mind and a temple
constexpr std::string_view k_TempleScript = R"(TOGGLE_COMPUTER_PLAYER("PLAYER_TWO", 1)
CREATE_CITADEL("0.0,-90.0", 0, "PLAYER_TWO", 5690, 1000)
)";

/// The lines of a land that give player two a computer mind, a temple and a Celtic town that offers the fireball miracle,
/// with two abodes of people
constexpr std::string_view k_FireballTownScript = R"(TOGGLE_COMPUTER_PLAYER("PLAYER_TWO", 1)
CREATE_CITADEL("0.0,-90.0", 0, "PLAYER_TWO", 5690, 1000)
CREATE_TOWN(900, "0.0,40.0", "PLAYER_TWO", 0, "CELTIC")
SET_TOWN_BELIEF_CAP(900, "PLAYER_TWO", 2.0)
CREATE_NEW_TOWN_SPELL(900, "FIREBALL")
SET_TOWN_BELIEF(900, "PLAYER_TWO", 1.0)
CREATE_TOWN_CENTRE(900, "-4.0,41.0", "CELTIC_ABODE_TOWN_CENTRE", 0, 1000, 0)
CREATE_ABODE(900, "10.0,90.0", "CELTIC_ABODE_STORAGE_PIT", 6282, 1000, 0, 3000)
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

/// The creature of player two, a grown up tiger, at a place
CreatureSetup GodCreature(glm::vec2 offset)
{
	return {.label = "player two's",
	        .species = CreatureType::Tiger,
	        .offset = offset,
	        .owner = PlayerNames::PLAYER_TWO,
	        .size = 1.0f};
}

/// A personality that gives a node of the god of player two a weight at the start
Command Weight(std::string_view node, float weight)
{
	return {.kind = Command::Kind::ComputerPersonality, .player = PlayerNames::PLAYER_TWO, .action = node, .amount = weight};
}

/// At the start, the god of player two wants only to play with its creature, in one way:
/// - The need to play gets the weight 1.
/// - The needs to look after the worshippers, to look after the creature and to teach it get the weight 0.
/// - The way to play gets the weight 1. The throw of food gets the weight 0 when it is not the way.
std::vector<Command> OnlyPlay(std::string_view way)
{
	std::vector<Command> commands {Weight("AttendToWorshippersWorshippers", 0.0f), Weight("Lookaftercreature", 0.0f),
	                               Weight("TeachCreature", 0.0f), Weight("PlayWithCreature", 1.0f)};
	if (way != "ThrowFoodatCreature")
	{
		commands.push_back(Weight("ThrowFoodatCreature", 0.0f));
	}
	commands.push_back(Weight(way, 1.0f));
	return commands;
}

} // namespace

void testbed_scenarios::AddComputerGodCreaturePlayScenarios(std::vector<Scenario>& all)
{
	all.push_back({
	    .id = "mind.computer_god_throw_food",
	    .name = "A computer god that throws food at its creature",
	    .facet = Facet::Mind,
	    .description = "The land of the food miracle scenario, with two ripe wheat fields to the east of the town. The "
	                   "tiger of player two stands between the temple and the town. At the start, the god wants only to "
	                   "play with its creature, and to throw food at it.",
	    .expected = "The hand of the god takes food from a field. It moves to 20 metres from the tiger and throws the "
	                "food at the tiger in an arc. The food falls near the tiger.",
	    .framing = {.shot = Shot::Overview, .include = {glm::vec2(-80.0f, -110.0f), glm::vec2(140.0f, 100.0f)}},
	    .creatures = {GodCreature({30.0f, -30.0f})},
	    .objects = {{.type = FieldTypeInfo::Wheat, .offset = {100.0f, 20.0f}, .ripeCrop = true},
	                {.type = FieldTypeInfo::Wheat, .offset = {100.0f, 60.0f}, .ripeCrop = true}},
	    .landScript = k_ComputerGodTownScript,
	    .commands = OnlyPlay("ThrowFoodatCreature"),
	    .worshipShare = 0.5f,
	    .objectsAfterLandScript = true,
	});
	all.push_back({
	    .id = "mind.computer_god_slap",
	    .name = "A computer god that slaps its creature",
	    .facet = Facet::Mind,
	    .description = "Lines of a land give player two a temple to the south. The tiger of player two stands 60 metres to "
	                   "the east of the temple. At the start, the god wants only to play with its creature, and to slap "
	                   "it.",
	    .expected = "The hand of the god moves to the head of the tiger and slaps it. The hand stays on the tiger for "
	                "three to five seconds. The tiger feels the slap as a punishment.",
	    .framing = {.shot = Shot::Overview, .include = {glm::vec2(-40.0f, -130.0f), glm::vec2(80.0f, 0.0f)}},
	    .creatures = {GodCreature({60.0f, -60.0f})},
	    .landScript = k_TempleScript,
	    .commands = OnlyPlay("SlapCreature"),
	});
	all.push_back({
	    .id = "mind.computer_god_stroke",
	    .name = "A computer god that strokes its creature",
	    .facet = Facet::Mind,
	    .description = "Lines of a land give player two a temple to the south. The tiger of player two stands 60 metres to "
	                   "the east of the temple. At the start, the god wants only to play with its creature, and to stroke "
	                   "it.",
	    .expected = "The hand of the god moves to the head of the tiger and strokes it. The hand stays on the tiger for "
	                "three to five seconds. The tiger feels the stroke as a reward.",
	    .framing = {.shot = Shot::Overview, .include = {glm::vec2(-40.0f, -130.0f), glm::vec2(80.0f, 0.0f)}},
	    .creatures = {GodCreature({60.0f, -60.0f})},
	    .landScript = k_TempleScript,
	    .commands = OnlyPlay("StrokeCreature"),
	});
	all.push_back({
	    .id = "mind.computer_god_leash",
	    .name = "A computer god that ties the leash of its creature to a villager",
	    .facet = Facet::Mind,
	    .description = "Lines of a land give player two a temple to the south and a Celtic town to the north. The tiger of "
	                   "player two stands between the temple and the town. At the start, the god wants only to play with "
	                   "its creature, and to tie its leash to a thing that moves.",
	    .expected = "The hand of the god moves to a free villager near the hand. The god ties the leash of the tiger to "
	                "the villager, and the hand follows the villager for a time. The rope of the leash goes from the "
	                "collar of the tiger to the villager.",
	    .framing = {.shot = Shot::Overview, .include = {glm::vec2(-80.0f, -110.0f), glm::vec2(80.0f, 100.0f)}},
	    .creatures = {GodCreature({30.0f, -30.0f})},
	    .landScript = k_FireballTownScript,
	    .commands = OnlyPlay("AttachLeashtoMovingObject"),
	});
	auto laugh = OnlyPlay("CastSpellOnMyCreatureForALaugh");
	laugh.push_back({.kind = Command::Kind::FillSiteIcons, .player = PlayerNames::PLAYER_TWO, .amount = 1.0f});
	all.push_back({
	    .id = "mind.computer_god_laugh",
	    .name = "A computer god that casts a miracle on its creature for a laugh",
	    .facet = Facet::Mind,
	    .description = "Lines of a land give player two a temple to the south and a Celtic town to the north that offers "
	                   "the fireball miracle. Half of the people go to worship at its site. The tiger of player two stands "
	                   "between the temple and the town. At the start, the god wants only to play with its creature, and "
	                   "to cast a miracle on it for a laugh. Each icon of the site holds the full charge of its miracle.",
	    .expected = "The hand of the god moves to the fireball icon of the site and the worshippers chant. Then the hand "
	                "casts a random creature miracle on the tiger: freeze, small, big, weak or strong. The hand waits "
	                "until the miracle stops.",
	    .framing = {.shot = Shot::Overview, .include = {glm::vec2(-80.0f, -110.0f), glm::vec2(80.0f, 100.0f)}},
	    .creatures = {GodCreature({30.0f, -30.0f})},
	    .landScript = k_FireballTownScript,
	    .commands = laugh,
	    .worshipShare = 0.5f,
	});
}
