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

/// The lines of a land that give player two a computer mind, a temple and a Celtic town that offers the fireball
/// miracle. Player one has a Celtic town 160 metres to the east of it, with a storage pit and four abodes of people.
constexpr std::string_view k_WarScript = R"(TOGGLE_COMPUTER_PLAYER("PLAYER_TWO", 1)
CREATE_CITADEL("0.0,-90.0", 0, "PLAYER_TWO", 5690, 1000)
CREATE_TOWN(900, "0.0,40.0", "PLAYER_TWO", 0, "CELTIC")
SET_TOWN_BELIEF_CAP(900, "PLAYER_TWO", 2.0)
CREATE_NEW_TOWN_SPELL(900, "FIRE")
SET_TOWN_BELIEF(900, "PLAYER_TWO", 1.0)
CREATE_TOWN_CENTRE(900, "-4.0,41.0", "CELTIC_ABODE_TOWN_CENTRE", 0, 1000, 0)
CREATE_ABODE(900, "10.0,90.0", "CELTIC_ABODE_STORAGE_PIT", 6282, 1000, 0, 3000)
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
CREATE_TOWN(901, "160.0,40.0", "PLAYER_ONE", 0, "CELTIC")
SET_TOWN_BELIEF(901, "PLAYER_ONE", 1.0)
CREATE_TOWN_CENTRE(901, "156.0,41.0", "CELTIC_ABODE_TOWN_CENTRE", 0, 1000, 0)
CREATE_ABODE(901, "170.0,90.0", "CELTIC_ABODE_STORAGE_PIT", 6282, 1000, 0, 3000)
CREATE_ABODE(901, "190.0,60.0", "CELTIC_ABODE_F", 4328, 1095, 0, 0)
CREATE_VILLAGER_POS("186.0,62.0", "190.0,60.0", "CELTIC_HOUSEWIFE", 20)
CREATE_VILLAGER_POS("186.0,62.0", "190.0,60.0", "CELTIC_FORESTER", 20)
CREATE_ABODE(901, "210.0,60.0", "CELTIC_ABODE_F", 4328, 1095, 0, 0)
CREATE_VILLAGER_POS("206.0,62.0", "210.0,60.0", "CELTIC_HOUSEWIFE", 20)
CREATE_VILLAGER_POS("206.0,62.0", "210.0,60.0", "CELTIC_FORESTER", 20)
CREATE_ABODE(901, "130.0,60.0", "CELTIC_ABODE_F", 4328, 1095, 0, 0)
CREATE_VILLAGER_POS("126.0,62.0", "130.0,60.0", "CELTIC_HOUSEWIFE", 20)
CREATE_VILLAGER_POS("126.0,62.0", "130.0,60.0", "CELTIC_FORESTER", 20)
CREATE_ABODE(901, "110.0,60.0", "CELTIC_ABODE_F", 4328, 1095, 0, 0)
CREATE_VILLAGER_POS("106.0,62.0", "110.0,60.0", "CELTIC_HOUSEWIFE", 20)
CREATE_VILLAGER_POS("106.0,62.0", "110.0,60.0", "CELTIC_FORESTER", 20)
)";

/// The id of the town of player one
constexpr uint32_t k_RivalTown = 901;
/// The game turns that the scenarios wait before the god gets its action: 30 seconds, so that the worshippers come to
/// the site first
constexpr uint32_t k_Wait = 300;

/// The view of both towns and the temple
Framing BothTowns()
{
	return {.shot = Shot::Overview, .include = {glm::vec2(-80.0f, -110.0f), glm::vec2(230.0f, 100.0f)}};
}

/// A grown up creature of a player at a place
CreatureSetup CreatureOf(std::string_view label, CreatureType species, PlayerNames owner, glm::vec2 offset)
{
	return {.label = label, .species = species, .offset = offset, .owner = owner, .size = 1.0f};
}

/// The town of player one
ComputerActionThing RivalTown()
{
	return {.kind = ComputerActionThing::Kind::Town, .townId = k_RivalTown};
}

/// The first creature of a player
ComputerActionThing CreatureThing(PlayerNames owner)
{
	return {.kind = ComputerActionThing::Kind::Creature, .owner = owner};
}

/// The fireball icon at the worship site of player two
ComputerActionThing FireballIcon()
{
	return {.kind = ComputerActionThing::Kind::Icon, .type = MagicType::Fireball};
}

/// The scenario's first object
ComputerActionThing FirstObject()
{
	return {.kind = ComputerActionThing::Kind::Object, .object = 0};
}

} // namespace

void testbed_scenarios::AddComputerGodWarScenarios(std::vector<Scenario>& all)
{
	all.push_back({
	    .id = "mind.computer_god_destroy_town_with_spell",
	    .name = "A computer god that casts a miracle at the town of another player",
	    .facet = Facet::Mind,
	    .description = "Lines of a land switch on the computer mind of player two. Player two has a temple to the south "
	                   "and a town that offers the fireball miracle. Player one has a town to the east. After 30 seconds, "
	                   "a testbed tool gives the god the action DestroytownWithSpell, with the town of player one and the "
	                   "fireball icon, as a land script does.",
	    .expected = "The hand moves to the fireball icon, and the worshippers chant for the miracle. When the charge is "
	                "full, the god throws the fireball at the building or the field of the town of player one that it "
	                "wants most. A building that stands complete, near the hand, comes first.",
	    .framing = BothTowns(),
	    .landScript = k_WarScript,
	    .worshipShare = 0.5f,
	    .computerActions =
	        {{.afterTurns = k_Wait, .node = "DestroytownWithSpell", .first = RivalTown(), .second = FireballIcon()}},
	});
	all.push_back({
	    .id = "mind.computer_god_destroy_town_with_creature",
	    .name = "A computer god that sends its creature to the town of another player",
	    .facet = Facet::Mind,
	    .description = "The land of the scenario with the miracle at the town. The tiger of player two stands between the "
	                   "temple and the towns. After 30 seconds, a testbed tool gives the god the action "
	                   "DestroytownWithCreature, with the town of player one and the tiger, as a land script does.",
	    .expected = "The hand puts the evil leash on the tiger and ties the leash to the storage pit of the town of player "
	                "one. The tiger walks to the town to attack it. The tiger knows no angry miracle, so the god counts "
	                "one more time that it sent the creature without the lesson. The players.list query shows the count.",
	    .framing = BothTowns(),
	    .creatures = {CreatureOf("player two's", CreatureType::Tiger, PlayerNames::PLAYER_TWO, {60.0f, -20.0f})},
	    .landScript = k_WarScript,
	    .computerActions = {{.afterTurns = k_Wait,
	                         .node = "DestroytownWithCreature",
	                         .first = RivalTown(),
	                         .second = CreatureThing(PlayerNames::PLAYER_TWO)}},
	});
	all.push_back({
	    .id = "mind.computer_god_destroy_town_with_rock",
	    .name = "A computer god that throws a rock at the town of another player",
	    .facet = Facet::Mind,
	    .description = "The land of the scenario with the miracle at the town, with a rock to the east of the temple. After "
	                   "30 seconds, a testbed tool gives the god the action DestroytownWithObject, with the town of player "
	                   "one and the rock, as a land script does.",
	    .expected = "The hand moves to the rock and picks it up. Then the hand moves toward the town of player one and "
	                "throws the rock at the middle of the town.",
	    .framing = BothTowns(),
	    .objects = {{.type = MobileStaticInfo::Rock, .offset = {60.0f, -40.0f}}},
	    .landScript = k_WarScript,
	    .objectsAfterLandScript = true,
	    .computerActions =
	        {{.afterTurns = k_Wait, .node = "DestroytownWithObject", .first = RivalTown(), .second = FirstObject()}},
	});
	all.push_back({
	    .id = "mind.computer_god_attack_creature_with_spells",
	    .name = "A computer god that casts a miracle at the creature of another player",
	    .facet = Facet::Mind,
	    .description = "The land of the scenario with the miracle at the town. The cow of player one stands near the town "
	                   "of player one. After 30 seconds, a testbed tool gives the god the action AttackCreatureWithSpells, "
	                   "with the cow and the fireball icon, as a land script does.",
	    .expected = "The hand moves near the cow, then to the fireball icon, and the worshippers chant for the miracle. "
	                "The god throws the fireball at the cow. It does this two or three times, by a random number.",
	    .framing = BothTowns(),
	    .creatures = {CreatureOf("player one's", CreatureType::Cow, PlayerNames::PLAYER_ONE, {140.0f, 0.0f})},
	    .landScript = k_WarScript,
	    .worshipShare = 0.5f,
	    .computerActions = {{.afterTurns = k_Wait,
	                         .node = "AttackCreatureWithSpells",
	                         .first = CreatureThing(PlayerNames::PLAYER_ONE),
	                         .second = FireballIcon()}},
	});
	all.push_back({
	    .id = "mind.computer_god_attack_creature_with_creature",
	    .name = "A computer god that sends its creature at the creature of another player",
	    .facet = Facet::Mind,
	    .description = "The land of the scenario with the miracle at the town. The tiger of player two stands near the "
	                   "temple, and the cow of player one stands near the town of player one. After 30 seconds, a testbed "
	                   "tool gives the god the action AttackCreatureWithCreature, with the cow and the tiger, as a land "
	                   "script does.",
	    .expected = "The hand puts the evil leash on the tiger and ties the leash to the cow. The tiger walks to the cow "
	                "to attack it.",
	    .framing = BothTowns(),
	    .creatures = {CreatureOf("player two's", CreatureType::Tiger, PlayerNames::PLAYER_TWO, {40.0f, -40.0f}),
	                  CreatureOf("player one's", CreatureType::Cow, PlayerNames::PLAYER_ONE, {140.0f, 0.0f})},
	    .landScript = k_WarScript,
	    .computerActions = {{.afterTurns = k_Wait,
	                         .node = "AttackCreatureWithCreature",
	                         .first = CreatureThing(PlayerNames::PLAYER_ONE),
	                         .second = CreatureThing(PlayerNames::PLAYER_TWO)}},
	});
	all.push_back({
	    .id = "mind.computer_god_throw_rock_at_creature",
	    .name = "A computer god that throws a rock at the creature of another player",
	    .facet = Facet::Mind,
	    .description = "The land of the scenario with the miracle at the town, with a rock to the east of the temple. The "
	                   "cow of player one stands near the town of player one. After 30 seconds, a testbed tool gives the "
	                   "god the action ThrowrockatCreature, with the cow and the rock, as a land script does.",
	    .expected = "The hand moves to the rock and picks it up. Then the hand moves toward the cow and throws the rock at "
	                "it.",
	    .framing = BothTowns(),
	    .creatures = {CreatureOf("player one's", CreatureType::Cow, PlayerNames::PLAYER_ONE, {140.0f, 0.0f})},
	    .objects = {{.type = MobileStaticInfo::Rock, .offset = {60.0f, -40.0f}}},
	    .landScript = k_WarScript,
	    .objectsAfterLandScript = true,
	    .computerActions = {{.afterTurns = k_Wait,
	                         .node = "ThrowrockatCreature",
	                         .first = CreatureThing(PlayerNames::PLAYER_ONE),
	                         .second = FirstObject()}},
	});
}
