/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The testbed scenarios for the artefacts of a town. One shows the villagers that dance round a rock that the hand put
// down by their town, and the worth that each dance adds. One shows the glow and the symbol of the owner over charged
// artefacts. Two show a charged artefact that impresses another town: one thrown over its people, one put down in it.

#include <array>
#include <string>
#include <utility>
#include <vector>

#include "TestbedScenarioRegistry.h"

using namespace openblack;
using namespace openblack::testbed_scenarios;

namespace
{
using Kind = Command::Kind;

/// The town centre of the player's town
constexpr glm::vec2 k_Centre {0.0f, 40.0f};
/// Where the hand puts the rock down, between the town centre and the villagers. Nearer the centre, the rock would
/// come down on the centre and fall from it, and a thing that falls does not become an artefact.
constexpr glm::vec2 k_Drop {0.0f, 20.0f};
/// Where the rock lies before the hand takes it
constexpr glm::vec2 k_RockStart {-30.0f, 10.0f};
/// Where the charged artefacts of the player's town lie
constexpr glm::vec2 k_Charged {-6.0f, 26.0f};
constexpr glm::vec2 k_Glowing {6.0f, 26.0f};
/// A plain rock that is no artefact, to compare with the charged rocks
constexpr glm::vec2 k_Plain {18.0f, 26.0f};
/// The player's town of the impress scenarios, made by a land's script far to the west, and where its artefact lies
constexpr glm::vec2 k_FarCentre {-140.0f, 40.0f};
constexpr glm::vec2 k_FarRock {-140.0f, 20.0f};
/// The numbers of the scenario's own town and of the town that the land's script makes
constexpr uint32_t k_ScenarioTown = 900;
constexpr uint32_t k_FarTown = 901;
/// The worth of an artefact that shows its owner's symbol, and of one that only glows
constexpr float k_SymbolWorth = 2.5f;
constexpr float k_GlowWorth = 1.5f;
/// The town believes so much in the player: a dance there adds the most worth that a town can add
constexpr float k_TownBelief = 1000.0f;
/// The hour of the day at which the towns want to relax
constexpr float k_RelaxHour = 18.0f;
/// A new rock artefact is worth 0.01, read within the error of a float
constexpr double k_NewWorthLeast = 0.0099;
constexpr double k_NewWorthMost = 0.0101;
/// The game turns for a villager to wait and then dance the dance for small objects once (200 and 900 turns), and then
/// some more for the villagers to come
constexpr uint32_t k_FirstDanceByTurn = 3600;
constexpr uint32_t k_SecondDanceByTurn = 5400;
/// The search radius round a point for an artefact
constexpr float k_Near = 6.0f;

/// The scenario's own town in the middle: its centre, a full storage pit, two huts and twelve grown villagers who live in
/// the huts. The town is the player's unless the scenario gives it to another player. The town comes after this count
/// of other objects of the scenario.
std::vector<ObjectSetup> PlayersTown(size_t before)
{
	std::vector<ObjectSetup> objects {
	    {.type = AbodeInfo::CelticTownCentre, .offset = k_Centre},
	    {.type = AbodeInfo::CelticStoragePit,
	     .offset = k_Centre + glm::vec2(30.0f, 0.0f),
	     .storedFood = 5000,
	     .storedWood = 5000},
	    {.type = AbodeInfo::CelticHut, .offset = k_Centre + glm::vec2(-22.0f, 8.0f)},
	    {.type = AbodeInfo::CelticHut, .offset = k_Centre + glm::vec2(18.0f, 16.0f)},
	};
	const size_t firstHut = before + 2;
	constexpr std::array<VillagerInfo, 3> k_Kinds {VillagerInfo::CelticFarmerMale, VillagerInfo::CelticHousewifeFemale,
	                                               VillagerInfo::CelticForesterMale};
	for (size_t i = 0; i < 12; ++i)
	{
		const auto column = static_cast<float>(i % 6);
		const size_t rowIndex = i / 6;
		const auto row = static_cast<float>(rowIndex);
		objects.push_back({.type = k_Kinds.at(i % k_Kinds.size()),
		                   .offset = k_Centre + glm::vec2 {-15.0f + column * 6.0f, -32.0f - row * 5.0f},
		                   .joinTown = true,
		                   .homeIn = firstHut + i % 2});
	}
	return objects;
}

/// The lines of a land that make a Celtic town of the player far to the west. The scenario's own town, in the middle,
/// is the other town that the artefact impresses.
std::string FarTownScript()
{
	const std::string at = "\"" + std::to_string(k_FarCentre.x) + "," + std::to_string(k_FarCentre.y) + "\"";
	return "CREATE_TOWN(" + std::to_string(k_FarTown) + ", " + at + ", \"PLAYER_ONE\", 0, \"CELTIC\")\n" +
	       "CREATE_TOWN_CENTRE(" + std::to_string(k_FarTown) + ", " + at + ", \"CELTIC_ABODE_TOWN_CENTRE\", 0, 1000, 0)\n";
}

/// A land line that makes a rock at the point, and a line that makes it an artefact of the player in the town with the
/// number at the worth
std::string ArtefactScript(glm::vec2 at, float worth, uint32_t town)
{
	return "CREATE_MOBILE_STATIC(\"" + std::to_string(at.x) + "," + std::to_string(at.y) +
	       "\", 2, 0.0, 0.0, 0.0, 0.0, 1.0)\nMAKE_LAST_OBJECT_ARTIFACT(" + std::to_string(town) + ", \"PLAYER_ONE\", " +
	       std::to_string(worth) + ")\n";
}

/// The land of the scenarios that impress the neutral town: the player's town far to the west, with an artefact worth
/// enough to show the symbol
const std::string& ImpressScript()
{
	static const std::string k_Script = FarTownScript() + ArtefactScript(k_FarRock, k_SymbolWorth, k_FarTown);
	return k_Script;
}

/// The two charged artefacts of the player's town
const std::string& ChargedScript()
{
	static const std::string k_Script =
	    ArtefactScript(k_Charged, k_SymbolWorth, k_ScenarioTown) + ArtefactScript(k_Glowing, k_GlowWorth, k_ScenarioTown);
	return k_Script;
}

/// The camera looks at the place in front of the town centre where the hand lets the rock go. The hand is at the middle
/// of the screen.
Framing DropFraming()
{
	return {
	    .shot = Shot::Placed, .eye = glm::vec3(k_Drop.x, 26.0f, k_Drop.y - 40.0f), .look = glm::vec3(k_Drop.x, 0.0f, k_Drop.y)};
}
} // namespace

void testbed_scenarios::AddArtefactScenarios(std::vector<Scenario>& all)
{
	all.push_back({
	    .id = "town.artefact_dance",
	    .name = "Villagers that dance round a rock and charge it",
	    .facet = Facet::Objects,
	    .description = "A Celtic town of the player, which believes much in the player, with a full storage pit, two huts "
	                   "and twelve villagers. It is evening, the time at which the town wants to relax. After a second, "
	                   "the hand takes a rock from the west and puts it down gently in front of the town centre. Best "
	                   "watched at game speed 4.",
	    .expected =
	        "The rock becomes an artefact of the town, worth 0.01. The villagers come to it one by one and wait "
	        "round it. When the wait is over, they all dance round the rock for 90 seconds. At the end of each "
	        "dance its worth goes up by 0.012 for each dancer. While the town wants to relax, the villagers wait for the next "
	        "dance. The report of the checks passes.",
	    .environment = {.hour = k_RelaxHour, .dispenserGrid = false, .cursor = glm::vec2 {0.5f, 0.5f}},
	    .framing = DropFraming(),
	    .objects =
	        [] {
		        std::vector<ObjectSetup> objects {{.type = MobileStaticInfo::Rock, .offset = k_RockStart}};
		        auto town = PlayersTown(objects.size());
		        objects.insert(objects.end(), town.begin(), town.end());
		        return objects;
	        }(),
	    .commands = {{.kind = Kind::HandTake, .delaySeconds = 1.0f, .object = 0},
	                 {.kind = Kind::HandRelease, .delaySeconds = 2.0f}},
	    .townBeliefInPlayer = k_TownBelief,
	    .checks = {{.label = "the rock that the hand put down is a new artefact of the town",
	                .quantity = checks::Quantity::ArtefactWorth,
	                .point = k_Drop,
	                .radius = k_Near,
	                .afterTurns = 40,
	                .byTurn = 200,
	                .atLeast = k_NewWorthLeast,
	                .atMost = k_NewWorthMost},
	               {.label = "the villagers dance round the rock",
	                .quantity = checks::Quantity::ArtefactDancing,
	                .point = k_Drop,
	                .radius = k_Near,
	                .afterTurns = 200,
	                .byTurn = k_FirstDanceByTurn,
	                .atLeast = 1.0},
	               {.label = "the first dance adds to the worth of the rock",
	                .quantity = checks::Quantity::ArtefactWorth,
	                .point = k_Drop,
	                .radius = k_Near,
	                .afterTurns = 200,
	                .byTurn = k_FirstDanceByTurn,
	                .atLeast = 0.1},
	               {.label = "the second dance adds to the worth of the rock",
	                .quantity = checks::Quantity::ArtefactWorth,
	                .point = k_Drop,
	                .radius = k_Near,
	                .afterTurns = 200,
	                .byTurn = k_SecondDanceByTurn,
	                .atLeast = 0.25}},
	});

	all.push_back({
	    .id = "town.artefact_charged",
	    .name = "The glow and the symbol of charged artefacts",
	    .facet = Facet::Particles,
	    .description = "Two rocks in front of a Celtic town centre of the player are artefacts of the town, as a land's "
	                   "script makes them. The left rock is worth 2.5, the middle rock 1.5. The right rock is a plain "
	                   "rock to compare. The clock stands still at noon.",
	    .expected = "The left and middle rocks show the glow of a charged artefact: a thin shell of stars in the colour of the "
	                "player that "
	                "slides over the rock. It shows most where the side of the rock turns away from the camera, and it is "
	                "faint on a rock, as in the game. The plain rock has no glow. Only the left rock, worth more than 2, shows "
	                "the symbol of the player at half size over it, which turns and "
	                "glows.",
	    .environment = {.clockRuns = false, .dispenserGrid = false},
	    .framing = {.shot = Shot::Placed,
	                .eye = glm::vec3(0.0f, 9.0f, k_Charged.y - 16.0f),
	                .look = glm::vec3(0.0f, 1.0f, k_Charged.y)},
	    .objects = {{.type = AbodeInfo::CelticTownCentre, .offset = k_Centre},
	                {.type = MobileStaticInfo::Rock, .offset = k_Plain}},
	    .landScript = ChargedScript(),
	    .townBeliefInPlayer = k_TownBelief,
	    .checks = {{.label = "the left rock is an artefact worth 2.5",
	                .quantity = checks::Quantity::ArtefactWorth,
	                .point = k_Charged,
	                .radius = 2.0f,
	                .afterTurns = 5,
	                .atLeast = k_SymbolWorth,
	                .atMost = k_SymbolWorth},
	               {.label = "the middle rock is an artefact worth 1.5",
	                .quantity = checks::Quantity::ArtefactWorth,
	                .point = k_Glowing,
	                .radius = 2.0f,
	                .afterTurns = 5,
	                .atLeast = k_GlowWorth,
	                .atMost = k_GlowWorth}},
	});

	all.push_back({
	    .id = "town.artefact_thrown_over_town",
	    .name = "A charged artefact thrown over the people of another town",
	    .facet = Facet::Objects,
	    .description = "A neutral Celtic town with twelve villagers is in the middle, and the camera looks at it. A town of "
	                   "the player lies 140 m to the west, with a rock artefact worth 2.5, as a land's script makes it. "
	                   "After two seconds, the hand takes the artefact and, two seconds later, throws it slowly over the "
	                   "villagers of the neutral town.",
	    .expected = "The artefact glows in the hand. Its symbol goes while it is out of a town, as in the game. The villagers "
	                "of the neutral town "
	                "see it fly: they point at it, or run from it when it comes near. Each villager that sees it gives "
	                "the player belief: 1 + 2.5 times what the rock alone gives. The rock lands and stays an artefact of "
	                "the player's town, because a thrown thing does not join a town. The report of the checks passes.",
	    .environment = {.dispenserGrid = false, .cursor = glm::vec2 {0.5f, 0.5f}},
	    .framing = DropFraming(),
	    .objects = PlayersTown(0),
	    .landScript = ImpressScript(),
	    .commands = {{.kind = Kind::HandTakeNear, .delaySeconds = 2.0f, .point = k_FarRock},
	                 {.kind = Kind::HandRelease, .delaySeconds = 2.0f, .velocity = {0.0f, 4.0f, 6.0f}}},
	    .townOwner = PlayerNames::NEUTRAL,
	    .checks = {{.label = "the neutral town does not believe in the player before the throw",
	                .quantity = checks::Quantity::TownBeliefInPlayer,
	                .index = k_ScenarioTown,
	                .afterTurns = 20,
	                .atMost = 0.0},
	               {.label = "the neutral town believes in the player after the throw",
	                .quantity = checks::Quantity::TownBeliefInPlayer,
	                .index = k_ScenarioTown,
	                .afterTurns = 41,
	                .byTurn = 200,
	                .atLeast = 0.001},
	               {.label = "the thrown rock is still an artefact worth 2.5",
	                .quantity = checks::Quantity::ArtefactWorth,
	                .point = k_Drop,
	                .radius = 25.0f,
	                .afterTurns = 200,
	                .atLeast = k_SymbolWorth,
	                .atMost = k_SymbolWorth}},
	});

	all.push_back({
	    .id = "town.artefact_put_in_town",
	    .name = "A charged artefact put down in another town",
	    .facet = Facet::Objects,
	    .description = "The land of town.artefact_thrown_over_town. After two seconds, the hand takes the artefact and, two "
	                   "seconds later, puts it down gently in front of the centre of the neutral town. It is evening, the "
	                   "time at which the towns want to relax. Best watched at game speed 4.",
	    .expected = "The villagers of the neutral town come to look at the artefact, and the town gives the player belief: "
	                "1 + 2.5 times what the rock alone gives. The rock becomes an artefact of the neutral town. Later "
	                "the villagers of that town dance round it, and while they dance the town gives the player a little "
	                "belief about every ten seconds. Its worth does not change, because the town is not the player's. "
	                "The report of the checks passes.",
	    .environment = {.hour = k_RelaxHour, .dispenserGrid = false, .cursor = glm::vec2 {0.5f, 0.5f}},
	    .framing = DropFraming(),
	    .objects = PlayersTown(0),
	    .landScript = ImpressScript(),
	    .commands = {{.kind = Kind::HandTakeNear, .delaySeconds = 2.0f, .point = k_FarRock},
	                 {.kind = Kind::HandRelease, .delaySeconds = 2.0f}},
	    .townOwner = PlayerNames::NEUTRAL,
	    .checks = {{.label = "the neutral town does not believe in the player before the artefact comes",
	                .quantity = checks::Quantity::TownBeliefInPlayer,
	                .index = k_ScenarioTown,
	                .afterTurns = 20,
	                .atMost = 0.0},
	               {.label = "the neutral town believes in the player after the hand puts the artefact down",
	                .quantity = checks::Quantity::TownBeliefInPlayer,
	                .index = k_ScenarioTown,
	                .afterTurns = 41,
	                .byTurn = 300,
	                .atLeast = 0.001},
	               {.label = "the villagers of the neutral town dance round the artefact",
	                .quantity = checks::Quantity::ArtefactDancing,
	                .point = k_Drop,
	                .radius = k_Near,
	                .afterTurns = 300,
	                .byTurn = k_FirstDanceByTurn,
	                .atLeast = 1.0},
	               {.label = "the worth of the artefact does not change in the neutral town",
	                .quantity = checks::Quantity::ArtefactWorth,
	                .point = k_Drop,
	                .radius = k_Near,
	                .afterTurns = k_FirstDanceByTurn,
	                .atLeast = k_SymbolWorth,
	                .atMost = k_SymbolWorth}},
	});
}
