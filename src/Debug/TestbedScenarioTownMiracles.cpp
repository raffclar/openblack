/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The testbed's scenarios of the debug tool that gives a town a miracle or takes it away

#include <vector>

#include "TestbedScenarioRegistry.h"

using namespace openblack;
using namespace openblack::testbed_scenarios;

namespace
{
/// Where the town centre stands
constexpr glm::vec2 k_Centre {0.0f, 40.0f};
/// Where the player's temple stands, south of the town centre
constexpr glm::vec2 k_Temple {0.0f, -60.0f};
/// The town believes in the player, so that it stays theirs
constexpr float k_TownBelief = 1000.0f;
/// The first land, which has no worship sites in a normal game
constexpr int32_t k_FirstLand = 1;

[[nodiscard]] std::vector<ObjectSetup> Town()
{
	return {{.type = AbodeInfo::NorseTownCentre, .offset = k_Centre},
	        {.type = AbodeInfo::NorseHut, .offset = k_Centre + glm::vec2(-20.0f, 5.0f)},
	        {.type = VillagerInfo::NorseHousewifeFemale, .offset = k_Centre + glm::vec2(10.0f, -10.0f), .joinTown = true},
	        {.type = VillagerInfo::NorseForesterMale, .offset = k_Centre + glm::vec2(-10.0f, -10.0f), .joinTown = true},
	        {.type = VillagerInfo::NorseFarmerMale, .offset = k_Centre + glm::vec2(5.0f, -15.0f), .joinTown = true}};
}

[[nodiscard]] Framing TownAndTemple()
{
	return {.shot = Shot::Overview,
	        .include = {k_Temple - glm::vec2(50.0f, 45.0f), k_Centre + glm::vec2(25.0f, 15.0f)},
	        .distance = 0.6f};
}
} // namespace

void openblack::testbed_scenarios::AddTownMiracleScenarios(std::vector<Scenario>& all)
{
	all.push_back({
	    .id = "town.miracle_debug",
	    .name = "Give a town a miracle with the debug tool",
	    .facet = Facet::Miracles,
	    .description = "A Norse town of the player with three villagers, its centre, and the player's temple built to "
	                   "the south, on the first land, which has no worship sites. The town has no miracles and no "
	                   "worship site. The Magic window opens: use its Town miracles tab.",
	    .expected = "Give the town a miracle (Heal, for example). A stone hand with the heal seed rises on the village "
	                "centre. The town's Norse worship site appears by the temple as a building site going up, with an "
	                "icon of the heal miracle. Take the miracle away: the hand and the site's icon go, the site stays.",
	    .environment = {.dispenserGrid = false, .landNumber = k_FirstLand},
	    .framing = TownAndTemple(),
	    .objects = Town(),
	    .temples = {{.offset = k_Temple}},
	    .townBeliefInPlayer = k_TownBelief,
	    .debugWindow = "Magic",
	});
	all.push_back({
	    .id = "town.miracle_debug_given",
	    .name = "A town given two miracles by the debug tool",
	    .facet = Facet::Miracles,
	    .description = "The same town and temple on the first land. The debug tool gives the town the heal and the water "
	                   "miracles, as a land file line gives them, then lets the town have its worship site.",
	    .expected = "The village centre shows two stone hands, heal and water, each seed spinning over the palm. The "
	                "town's Norse worship site goes up by the temple for the town's builders, with an icon of each "
	                "miracle. The site's water icon shows no power-up ring, as the player may not cast the water's "
	                "power-up.",
	    .environment = {.dispenserGrid = false, .landNumber = k_FirstLand},
	    .framing = TownAndTemple(),
	    .objects = Town(),
	    .temples = {{.offset = k_Temple}},
	    .townBeliefInPlayer = k_TownBelief,
	    .townMiracles = {MagicType::Heal, MagicType::Water},
	});
	all.push_back({
	    .id = "town.miracle_spares_stone_hands",
	    .name = "A fireball on a village centre with stone hands",
	    .facet = Facet::Miracles,
	    .description = "The same town and temple on the first land, given the heal and the water miracles. After four "
	                   "seconds a fireball falls on the village centre.",
	    .expected = "The blast and its fire hurt the centre and the villagers near it, and the centre can catch fire. "
	                "The two stone hands are not hurt and do not go: they are not in the land's cells, so the blast, "
	                "the fire and a heal do not find them. Each seed still spins over its palm, and a tap on a hand "
	                "still works.",
	    .environment = {.dispenserGrid = false, .landNumber = k_FirstLand},
	    .framing = TownAndTemple(),
	    .objects = Town(),
	    .temples = {{.offset = k_Temple}},
	    .miracles = {{.type = MagicType::Fireball,
	                  .point = k_Centre,
	                  .handOffset = k_Centre - glm::vec2(0.0f, 10.0f),
	                  .handHeight = 6.0f,
	                  .throwVelocity = {0.0f, 0.0f, 8.0f},
	                  .delaySeconds = 4.0f}},
	    .townBeliefInPlayer = k_TownBelief,
	    .townMiracles = {MagicType::Heal, MagicType::Water},
	});
	// The Help key on the things of a temple and a town that have their own texts
	using Kind = Command::Kind;
	all.push_back({
	    .id = "advisors.help_key_leashes_and_hands",
	    .name = "The Help key on the temple's leashes and a village centre's stone hands",
	    .facet = Facet::Advisors,
	    .description = "The town and temple of town.miracle_debug_given, its centre with the heal and water stone "
	                   "hands. The player's tiger is taught the three leashes, which then hang in a row in front of the "
	                   "camera. Put the hand over each leash and over a stone hand, and press F1.",
	    .expected = "Over each leash the advisors say its name in the dialogue box, as its tooltip says it: Leash Of "
	                "Aggression, Leash Of Learning, Leash Of Compassion. Over a stone hand they say the stone hand's own "
	                "text. Over the temple they say the temple's text.",
	    .environment = {.dispenserGrid = false, .landNumber = k_FirstLand},
	    .framing = {.shot = Shot::Placed, .eye = {6.0f, 8.0f, 0.0f}, .look = {6.0f, 4.0f, 30.0f}},
	    .creatures = {{.species = CreatureType::Tiger, .offset = {-30.0f, 10.0f}}},
	    .objects = Town(),
	    .temples = {{.offset = k_Temple}},
	    .commands = {{.kind = Kind::PutOnLeash, .creature = 0, .delaySeconds = 0.5f, .leash = LeashType::Rope},
	                 {.kind = Kind::PutOnLeash, .creature = 0, .delaySeconds = 0.5f, .leash = LeashType::Evil},
	                 {.kind = Kind::PutOnLeash, .creature = 0, .delaySeconds = 0.5f, .leash = LeashType::Good},
	                 {.kind = Kind::TakeOffLeash, .creature = 0, .delaySeconds = 0.5f},
	                 {.kind = Kind::HangLeashPosts, .creature = 0, .delaySeconds = 0.5f, .point = glm::vec2 {12.0f, 18.0f}}},
	    .townBeliefInPlayer = k_TownBelief,
	    .townMiracles = {MagicType::Heal, MagicType::Water},
	});
}
