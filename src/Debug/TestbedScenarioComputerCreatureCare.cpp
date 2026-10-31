/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <string_view>
#include <utility>
#include <vector>

#include "Creature/CreatureDesires.h"
#include "TestbedScenarioComputerPlayers.h"
#include "TestbedScenarioRegistry.h"

using namespace openblack;
using namespace openblack::testbed_scenarios;
using creature_desires::Desire;

namespace
{

/// The lines of a land that give player two a computer mind, a temple and a Celtic town that offers the heal miracle,
/// with four abodes of people
constexpr std::string_view k_HealTownScript = R"(TOGGLE_COMPUTER_PLAYER("PLAYER_TWO", 1)
CREATE_CITADEL("0.0,-90.0", 0, "PLAYER_TWO", 5690, 1000)
CREATE_TOWN(900, "0.0,40.0", "PLAYER_TWO", 0, "CELTIC")
SET_TOWN_BELIEF_CAP(900, "PLAYER_TWO", 2.0)
CREATE_NEW_TOWN_SPELL(900, "HEAL")
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
)";

/// The lines of a land that give player two only a computer mind and a temple
constexpr std::string_view k_TempleScript = R"(TOGGLE_COMPUTER_PLAYER("PLAYER_TWO", 1)
CREATE_CITADEL("0.0,-90.0", 0, "PLAYER_TWO", 5690, 1000)
)";

/// The lines of a land that give player two only a temple and no computer mind
constexpr std::string_view k_TempleOnlyScript = R"(CREATE_CITADEL("0.0,-90.0", 0, "PLAYER_TWO", 5690, 1000)
)";

/// The creature of player two, a grown up tiger, at a place
CreatureSetup GodCreature(glm::vec2 offset, NeedOverrides needs, std::vector<DesireOverride> desires)
{
	return {.label = "player two's",
	        .species = CreatureType::Tiger,
	        .offset = offset,
	        .owner = PlayerNames::PLAYER_TWO,
	        .size = 1.0f,
	        .needs = needs,
	        .desires = std::move(desires)};
}

/// Two ripe wheat fields to the east of the town of the food miracle scenario
std::vector<ObjectSetup> FieldsByTheTown()
{
	return {{.type = FieldTypeInfo::Wheat, .offset = {100.0f, 20.0f}, .ripeCrop = true},
	        {.type = FieldTypeInfo::Wheat, .offset = {100.0f, 60.0f}, .ripeCrop = true}};
}

/// At the start, the god of player two gets the weight 0 for the need to look after its worshippers, so that the need
/// does not keep its hand busy
Command FreeFromWorshippers()
{
	return {.kind = Command::Kind::ComputerPersonality,
	        .player = PlayerNames::PLAYER_TWO,
	        .action = "AttendToWorshippersWorshippers",
	        .amount = 0.0f};
}

/// At the start, the god of player two gets the weight 0 for the need to look after the desires of its towns. Else it
/// can tie the compassion leash of its creature to the town, and that leash holds the hunger of the creature down.
Command FreeFromTownDesires()
{
	return {.kind = Command::Kind::ComputerPersonality,
	        .player = PlayerNames::PLAYER_TWO,
	        .action = "AttendTownDesires",
	        .amount = 0.0f};
}

} // namespace

void testbed_scenarios::AddComputerGodCreatureCareScenarios(std::vector<Scenario>& all)
{
	all.push_back({
	    .id = "mind.computer_god_creature_food",
	    .name = "A computer god that feeds its hungry creature",
	    .facet = Facet::Mind,
	    .description = "The land of the food miracle scenario, with two ripe wheat fields to the east of the town. The "
	                   "tiger of player two stands between the temple and the town. It is very hungry, but its energy "
	                   "is not low. At the start, the god gets the weight 0 for the need to look after its worshippers and the "
	                   "desires of its towns.",
	    .expected = "The god looks after its creature. Its hand takes food from a field and drops it on the tiger, or "
	                "the worshippers chant for the food miracle and the hand casts the miracle on the tiger. The "
	                "players.list query shows the need of the creature and the steps of the hand.",
	    .framing = {.shot = Shot::Overview, .include = {glm::vec2(-80.0f, -110.0f), glm::vec2(140.0f, 100.0f)}},
	    .creatures = {GodCreature({30.0f, -30.0f}, {.energy = 0.6f}, {{.desire = Desire::Hunger, .fraction = 1.0f}})},
	    .objects = FieldsByTheTown(),
	    .landScript = k_ComputerGodTownScript,
	    .commands = {FreeFromWorshippers(), FreeFromTownDesires()},
	    .worshipShare = 0.5f,
	    .objectsAfterLandScript = true,
	});
	all.push_back({
	    .id = "mind.computer_god_creature_starving",
	    .name = "A computer god that throws food to its starving creature",
	    .facet = Facet::Mind,
	    .description =
	        "The land of the food miracle scenario, with two ripe wheat fields to the east of the town. The "
	        "tiger of player two stands between the temple and the town. It is very hungry and its energy is "
	        "almost gone. At the start, the god gets the weight 0 for the need to look after its worshippers and the "
	        "desires of its towns.",
	    .expected = "The god reacts to the emergency of its creature. Its hand takes food from a field, moves to the "
	                "tiger and throws the food to it in an arc. The food lands at the tiger.",
	    .framing = {.shot = Shot::Overview, .include = {glm::vec2(-80.0f, -110.0f), glm::vec2(140.0f, 100.0f)}},
	    .creatures = {GodCreature({30.0f, -30.0f}, {.energy = 0.05f}, {{.desire = Desire::Hunger, .fraction = 1.0f}})},
	    .objects = FieldsByTheTown(),
	    .landScript = k_ComputerGodTownScript,
	    .commands = {FreeFromWorshippers(), FreeFromTownDesires()},
	    .worshipShare = 0.5f,
	    .objectsAfterLandScript = true,
	});
	all.push_back({
	    .id = "mind.computer_god_creature_heal",
	    .name = "A computer god that heals its hurt creature",
	    .facet = Facet::Mind,
	    .description = "Lines of a land give player two a temple to the south and a Celtic town to the north that offers "
	                   "the heal miracle. Half of the people go to worship at its site. The tiger of player two stands "
	                   "between the temple and the town with little life left. At the start, the god gets the weight 0 for "
	                   "the need to look after its worshippers, and each icon of the site holds the full charge of its "
	                   "miracle.",
	    .expected = "The god reacts to the emergency of its creature. Its hand moves to the heal icon of the site, the "
	                "worshippers chant for the miracle, and the hand casts the heal miracle on the tiger.",
	    .framing = {.shot = Shot::Overview, .include = {glm::vec2(-80.0f, -110.0f), glm::vec2(80.0f, 100.0f)}},
	    .creatures = {GodCreature({30.0f, -30.0f}, {.life = 0.02f}, {})},
	    .landScript = k_HealTownScript,
	    .commands = {FreeFromWorshippers(),
	                 {.kind = Command::Kind::FillSiteIcons, .player = PlayerNames::PLAYER_TWO, .amount = 1.0f}},
	    .worshipShare = 0.5f,
	});
	all.push_back({
	    .id = "mind.computer_god_creature_home",
	    .name = "A computer god that sends its creature home",
	    .facet = Facet::Mind,
	    .description = "Lines of a land give player two a temple to the south. The tiger of player two stands 200 metres "
	                   "to the east of the temple. It wants very much to stay near its home.",
	    .expected = "The hand of the god moves to the tiger and stays over it. The tiger runs home to the temple. When it "
	                "arrives, the hand is free again.",
	    .framing = {.shot = Shot::Overview, .include = {glm::vec2(-40.0f, -130.0f), glm::vec2(220.0f, 0.0f)}},
	    .creatures = {GodCreature({200.0f, -60.0f}, {}, {{.desire = Desire::StayNearHome, .fraction = 1.0f}})},
	    .landScript = k_TempleScript,
	});
	all.push_back({
	    .id = "mind.computer_god_creature_sleep",
	    .name = "A computer god that sends its tired creature home to sleep",
	    .facet = Facet::Mind,
	    .description = "Lines of a land give player two a temple to the south. The tiger of player two stands 60 metres "
	                   "to the east of the temple and is very tired. After half a second the god gets the weight 1 for "
	                   "the need to make its creature sleep, as a personality of the god gives it.",
	    .expected = "The hand of the god moves to the tiger and stays over it. The tiger walks home to the temple and "
	                "sleeps there.",
	    .framing = {.shot = Shot::Overview, .include = {glm::vec2(-40.0f, -130.0f), glm::vec2(80.0f, 0.0f)}},
	    .creatures = {GodCreature({60.0f, -60.0f}, {.exhaustion = 0.9f}, {{.desire = Desire::Tiredness, .fraction = 1.0f}})},
	    .landScript = k_TempleScript,
	    .commands = {{.kind = Command::Kind::ComputerPersonality,
	                  .player = PlayerNames::PLAYER_TWO,
	                  .delaySeconds = 0.5f,
	                  .action = "MakeCreatureSleep",
	                  .amount = 1.0f}},
	});
	all.push_back({
	    .id = "mind.computer_god_creature_drink",
	    .name = "A computer god that sends its thirsty creature to drink",
	    .facet = Facet::Mind,
	    .description = "Lines of a land give player two a temple to the south. The tiger of player two stands near the "
	                   "temple and is very thirsty. After half a second the god gets the weight 1 for the need to make "
	                   "its creature drink, as a personality of the god gives it.",
	    .expected = "The hand of the god moves to the tiger and stays over it. The tiger goes to the coast of the lake "
	                "nearest to it and drinks from it.",
	    .framing = {.shot = Shot::Overview, .include = {glm::vec2(-40.0f, -130.0f), glm::vec2(80.0f, 0.0f)}},
	    .creatures = {GodCreature({30.0f, -60.0f}, {.dehydration = 0.9f}, {{.desire = Desire::Water, .fraction = 1.0f}})},
	    .landScript = k_TempleScript,
	    .commands = {{.kind = Command::Kind::ComputerPersonality,
	                  .player = PlayerNames::PLAYER_TWO,
	                  .delaySeconds = 0.5f,
	                  .action = "MakeCreatureDrink",
	                  .amount = 1.0f}},
	});
	all.push_back({
	    .id = "mind.god_camera_looks_at_creature",
	    .name = "The camera of another god looks at the creature of that god",
	    .facet = Facet::Mind,
	    .description = "A game alone. Lines of a land give player two a temple to the south and no computer mind. The "
	                   "camera of that god starts 20 m above the temple and 20 m to the north of it, and looks at the "
	                   "temple. The tiger of player two stands 40 m to the south of the temple, in the view of that camera.",
	    .expected = "Each turn that the tiger stays in the view of the camera of its god, the count of the turns that its "
	                "player looked at it (turnsLookedAt of its mind in the inspector) goes up by one. Before this change, "
	                "the count stayed at 0, because only the first player looked at creatures in a game alone.",
	    .framing = {.shot = Shot::Overview, .include = {glm::vec2(-40.0f, -150.0f), glm::vec2(40.0f, -40.0f)}},
	    .creatures = {GodCreature({0.0f, -130.0f}, {}, {})},
	    .landScript = k_TempleOnlyScript,
	});
}
