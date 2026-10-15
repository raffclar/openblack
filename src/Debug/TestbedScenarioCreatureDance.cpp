/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The testbed's scenarios of a creature that dances with the villagers of a town, and that dances alone

#include <array>
#include <string_view>
#include <utility>
#include <vector>

#include "TestbedScenarioRegistry.h"

using namespace openblack;
using namespace openblack::testbed_scenarios;

namespace
{
/// The town's two huts, west of the open ground where its people stand
constexpr std::array<glm::vec2, 2> k_Huts {{{-45.0f, 20.0f}, {-45.0f, 50.0f}}};
/// Ten grown people of the town in two rows, five men and five women
constexpr size_t k_People = 10;
constexpr glm::vec2 k_FirstPerson {-10.0f, 25.0f};
constexpr float k_PersonSpacing = 5.0f;

/// The objects of a town scene: the two huts, and ten grown people who live in them
std::vector<openblack::testbed_scenarios::ObjectSetup> TownScene()
{
	using namespace openblack::testbed_scenarios;
	std::vector<ObjectSetup> objects;
	for (const auto hut : k_Huts)
	{
		objects.push_back({.type = AbodeInfo::CelticHut, .offset = hut, .yawDegrees = 90.0f});
	}
	for (size_t i = 0; i < k_People; ++i)
	{
		const auto row = static_cast<float>(i / 5);
		const auto column = static_cast<float>(i % 5);
		objects.push_back({.type = i % 2 == 0 ? VillagerInfo::CelticFarmerMale : VillagerInfo::CelticHousewifeFemale,
		                   .offset = k_FirstPerson + glm::vec2(column * k_PersonSpacing, row * k_PersonSpacing),
		                   .yawDegrees = 0.0f,
		                   .joinTown = true,
		                   .livesIn = i % 2});
	}
	return objects;
}

/// The villager a dance is for: the first of the people
constexpr size_t k_DanceVillager = k_Huts.size();

/// A scene of the town where the player's tiger is told to do a dance action with the first man of the rows
openblack::testbed_scenarios::Scenario TownDance(std::string_view id, std::string_view name, std::string_view action,
                                                 std::string_view description, std::string_view expected)
{
	using namespace openblack::testbed_scenarios;
	return {
	    .id = id,
	    .name = name,
	    .facet = Facet::Objects,
	    .description = description,
	    .expected = expected,
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-50.0f, 0.0f}, {50.0f, 70.0f}}, .distance = 0.8f},
	    .creatures = {{.species = CreatureType::Tiger, .offset = {40.0f, 30.0f}, .facingDegrees = 270.0f}},
	    .objects = TownScene(),
	    .commands = {{.kind = Command::Kind::DoAction, .delaySeconds = 1.0f, .object = k_DanceVillager, .action = action}},
	};
}
} // namespace

void openblack::testbed_scenarios::AddCreatureDanceScenarios(std::vector<Scenario>& all)
{
	all.push_back(TownDance("creatures.dance_with_villagers", "A creature dancing with villagers", "DanceWithVillagers",
	                        "Two huts of the player's town, with ten grown people of the town who live in them, standing in "
	                        "two rows on open ground. The player's tiger stands forty metres to the east. One second in, it "
	                        "is told to dance with the villagers, with the first man of the rows as the villager it dances "
	                        "for.",
	                        "The tiger goes near the man and faces him, and plays its summon. The town's people stop what "
	                        "they do: eight of the ten come under its control (it calls an even number, and stops when it has "
	                        "more than two thirds of the town's adults). The tiger walks to a clear place by the man, without "
	                        "a turn of its head, and points at it with a smile. The eight people run to their places in a "
	                        "circle round it and stand still. Then the dance starts: the people dance and worship the tiger, "
	                        "and the tiger dances in the middle, playing its wave, impress, happy and summon actions and "
	                        "turning as the dance says. While the people worship, the town's belief in the player goes up, "
	                        "with the belief symbols above them. A camera near the dance hears the celtic chant. After 45 "
	                        "seconds the dance stops and the people go home."));
	all.push_back(TownDance("creatures.dance_story", "A creature telling villagers a story", "TellVillagersAStory",
	                        "Two huts of the player's town, with ten grown people of the town who live in them, standing in "
	                        "two rows on open ground. The player's tiger stands forty metres to the east. One second in, it "
	                        "is told to tell the villagers a story, for the first man of the rows.",
	                        "The tiger calls the people as for the dance to impress them, and they stand in their groups "
	                        "round it. Then it tells its story for 50 seconds: it faces the dance and turns, plays its happy "
	                        "and sad actions, talks with its mouth moving, sits down and gets up again, and turns to the "
	                        "angles of the story. At the end the people go home."));
	all.push_back(TownDance("creatures.dance_playful", "A creature dancing playfully with villagers",
	                        "DancePlayfullyWithVillagersParti",
	                        "Two huts of the player's town, with ten grown people of the town who live in them, standing in "
	                        "two rows on open ground. The player's tiger stands forty metres to the east. One second in, it "
	                        "is told to dance playfully with the villagers, for the first man of the rows.",
	                        "The tiger calls the people and they stand round it. Then it dances: it plays the start of its "
	                        "dance and then dance moves one after the other, each move as long as the others. Between the "
	                        "moves it pulls faces, plays its summon, wave and impress actions, and sits down once. At the end "
	                        "the people go home."));
	all.push_back(TownDance("creatures.dance_amorous", "A creature dancing amorously with villagers",
	                        "DanceAmorouslyWithVillagers",
	                        "Two huts of the player's town, with ten grown people of the town who live in them, standing in "
	                        "two rows on open ground. The player's tiger stands forty metres to the east. One second in, it "
	                        "is told to dance amorously with the villagers, with the first man of the rows.",
	                        "The tiger calls eight of the people, men and women in pairs. It turns, plays its summon and "
	                        "happy actions, and then dances its dance moves in the middle while the pairs dance round it. At "
	                        "the end the people go home."));
	all.push_back(TownDance("creatures.dance_game", "A creature playing a game with villagers", "PlayGameWithVillagers",
	                        "Two huts of the player's town, with ten grown people of the town who live in them, standing in "
	                        "two rows on open ground. The player's tiger stands forty metres to the east. One second in, it "
	                        "is told to play a game with the villagers, with the first man of the rows.",
	                        "The tiger calls the people and they stand in two groups. The tiger faces the dance, turns, plays "
	                        "its happy and summon actions and walks after its place as its group moves. At the end it dances "
	                        "its dance moves, and the people go home."));
	all.push_back(TownDance("creatures.dance_watched", "A creature dancing playfully while villagers watch",
	                        "DancePlayfullyWithVillagersWatch",
	                        "Two huts of the player's town, with ten grown people of the town who live in them, standing in "
	                        "two rows on open ground. The player's tiger stands forty metres to the east. One second in, it "
	                        "is told to dance playfully while the villagers watch, with the first man of the rows.",
	                        "The tiger calls the people and they stand round it. It faces the dance, and from 35 beats it "
	                        "dances its dance moves until the end. Then the people go home."));
	all.push_back(TownDance("creatures.practice_dance", "A creature dancing alone", "PracticeDance",
	                        "Two huts of the player's town, with ten grown people of the town who live in them, standing in "
	                        "two rows on open ground. The player's tiger stands forty metres to the east. One second in, it "
	                        "is told to practise its dance.",
	                        "The tiger dances where it stands with a smile: the start of its dance, then dance moves chosen "
	                        "at random, each one second long, for 20 seconds. Then it plays the finish of the dance."));

	// The dance around a thing and the dance outside the temple make no dance of their own: they start the creature's
	// last dance, if it has one
	{
		auto around =
		    TownDance("creatures.dance_around_thing", "A creature that has never danced, told to dance around a hut",
		              "DanceAroundArtefact",
		              "Two huts of the player's town, with ten grown people of the town who live in them, standing in "
		              "two rows on open ground. The player's tiger stands ninety metres to the east and has never "
		              "danced. One second in, it is told to dance around the first hut.",
		              "The tiger walks towards the hut and looks at it as it goes. It stops four of its heights from the "
		              "hut. No dance starts and nobody comes: the tiger gives up the action there, as in the original "
		              "game, and goes back to what it wants to do.");
		// It dances around the first hut, not the villager
		around.commands.front().object = 0;
		around.framing = {.shot = Shot::Placed, .eye = {50.0f, 16.0f, -15.0f}, .look = {25.0f, 6.0f, 28.0f}};
		// It starts far enough from the hut to walk a good way first
		around.creatures.front().offset = {90.0f, 30.0f};
		all.push_back(std::move(around));
	}
	{
		auto again = TownDance("creatures.dance_around_thing_again", "A creature that told a story, told to dance around a hut",
		                       "TellVillagersAStory",
		                       "The town of the story scene: two huts, ten grown people in two rows, the player's tiger forty "
		                       "metres to the east. One second in, the tiger is told to tell the villagers a story. Ninety "
		                       "seconds later, after the story, it is told to dance around the first hut.",
		                       "First the story, as in the story scene. Then the tiger walks towards the hut and stops four of "
		                       "its heights from it. As in the original game, the old story starts again where it was told, "
		                       "and the tiger goes back to its place in it and dances there for sixty seconds. The people have "
		                       "gone home and do not come back.");
		again.commands.push_back(
		    {.kind = Command::Kind::DoAction, .delaySeconds = 90.0f, .object = 0, .action = "DanceAroundArtefact"});
		all.push_back(std::move(again));
	}
	all.push_back({
	    .id = "creatures.dance_outside_temple",
	    .name = "A creature that has never danced, told to dance outside its temple",
	    .facet = Facet::Objects,
	    .description = "The player's temple, and the player's tiger standing 140 metres north of it. The tiger has never "
	                   "danced. One second in, it is told to dance outside the temple's worship site.",
	    .expected = "The tiger walks south to the temple and looks at it as it goes. It stops six of its heights from the "
	                "temple's heart (never more than fifty metres), turns to face the heart and holds still for two "
	                "seconds. No dance starts: the tiger gives up the action there, as in the original game.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {55.0f, 22.0f, 100.0f}, .look = {0.0f, 8.0f, 40.0f}},
	    .creatures = {{.species = CreatureType::Tiger, .offset = {0.0f, 110.0f}, .facingDegrees = 180.0f}},
	    .temples = {{.offset = {0.0f, -30.0f}}},
	    .commands =
	        {{.kind = Command::Kind::DoAction, .delaySeconds = 1.0f, .action = "DanceOutsideWorshipSite", .onTemple = true}},
	});
}
