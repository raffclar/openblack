/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The testbed's scenarios of creature actions that Land 1 can start and that no other scenario stages. A script tells
// a tiger to do each action, so that the sweep of the creature's clips sees every action

#include <string_view>
#include <vector>

#include "TestbedScenarioRegistry.h"

using namespace openblack;
using namespace openblack::testbed_scenarios;

namespace
{
using Kind = Command::Kind;

/// A grown tiger whose needs stay full, so that only the told action shows
CreatureSetup Actor()
{
	return {.label = "actor",
	        .species = CreatureType::Tiger,
	        .facingDegrees = 0.0f,
	        .phase = 3,
	        .needs = {.energy = 1.0f, .exhaustion = 0.0f, .dehydration = 0.0f, .poo = 0.0f, .life = 1.0f}};
}

Command Do(std::string_view action, float delay, size_t object = 0)
{
	return {.kind = Kind::DoAction, .delaySeconds = delay, .object = object, .action = action};
}

/// The framing that keeps the tiger and the things near it in view
Framing Near()
{
	return {.shot = Shot::Overview, .include = {{-40.0f, -60.0f}, {40.0f, 20.0f}}};
}
} // namespace

void testbed_scenarios::AddCreatureActionScenarios(std::vector<Scenario>& all)
{
	const ObjectSetup rock {.type = MobileStaticInfo::Rock, .offset = {6.0f, 0.0f}};
	const ObjectSetup villager {.type = VillagerInfo::CelticFarmerMale, .offset = {0.0f, -40.0f}};

	all.push_back({
	    .id = "actions.go_to_hill_and_look",
	    .name = "Goes to a hill and looks about",
	    .facet = Facet::Movement,
	    .description = "A grown tiger is told to go to a hill and look about from it. The testbed is flat, so every "
	                   "region of 80 metres is a hill.",
	    .expected = "It walks to the highest cell of a region near it and looks about from there.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-160.0f, -120.0f}, {160.0f, 120.0f}}},
	    .creatures = {Actor()},
	    .objects = {rock},
	    .commands = {Do("GoToHillAndLook", 1.0f)},
	});
	all.push_back({
	    .id = "actions.go_to_hill_and_sit",
	    .name = "Goes to a hill to sit",
	    .facet = Facet::Movement,
	    .description = "A grown tiger is told to go to a hill and sit there.",
	    .expected = "It walks to the highest cell of a region near it. It does not sit down there.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-160.0f, -120.0f}, {160.0f, 120.0f}}},
	    .creatures = {Actor()},
	    .objects = {rock},
	    .commands = {Do("GoToHillAndSit", 1.0f)},
	});
	all.push_back({
	    .id = "actions.explore_coastline",
	    .name = "Explores the coast",
	    .facet = Facet::Movement,
	    .description = "A grown tiger is told to explore the coast. The testbed's lake gives it a coast to find.",
	    .expected = "It walks to a coast that its map of the land gives and looks about there.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-200.0f, -200.0f}, {200.0f, 200.0f}}},
	    .creatures = {Actor()},
	    .objects = {rock},
	    .commands = {Do("ExploreCoastline", 1.0f)},
	});
	all.push_back({
	    .id = "actions.explore_towns",
	    .name = "Explores the towns",
	    .facet = Facet::Movement,
	    .description = "A grown tiger is told to explore the towns. Two huts stand 120 metres north of it.",
	    .expected = "It walks to a town that its map of the land gives and looks about there.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-80.0f, -160.0f}, {80.0f, 20.0f}}},
	    .creatures = {Actor()},
	    .objects = {rock,
	                {.type = AbodeInfo::CelticHut, .offset = {-10.0f, -120.0f}},
	                {.type = AbodeInfo::CelticHut, .offset = {12.0f, -126.0f}}},
	    .commands = {Do("ExploreTowns", 1.0f)},
	});
	all.push_back({
	    .id = "actions.put_down",
	    .name = "Puts down what it holds",
	    .facet = Facet::Objects,
	    .description = "A grown tiger is told to hold a rock, and eight seconds later to put down what it holds.",
	    .expected = "It reaches for the rock and picks it up. Then it puts the rock down in front of it.",
	    .environment = {.dispenserGrid = false},
	    .framing = Near(),
	    .creatures = {Actor()},
	    .objects = {{.type = MobileStaticInfo::Rock, .offset = {0.0f, -12.0f}}},
	    .commands = {Do("HoldObject", 1.0f), Do("PutDown", 8.0f)},
	});
	all.push_back({
	    .id = "actions.run_away_from_player",
	    .name = "Runs away from the player",
	    .facet = Facet::Mind,
	    .description = "A grown tiger is told to run away from the player.",
	    .expected = "One time in two it starts, frightened, with wide eyes. It runs with a frightened face and looks back "
	                "in turns. One time in two it starts again at the end of the run.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Follow, .distance = 1.5f},
	    .creatures = {Actor()},
	    .objects = {rock},
	    .commands = {Do("RunAwayFromPlayer", 1.0f)},
	});
	all.push_back({
	    .id = "actions.scratch",
	    .name = "Scratches",
	    .facet = Facet::Expressions,
	    .description = "A grown tiger is told to scratch, as an itch makes it.",
	    .expected = "It plays the scratch clip once.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Follow, .distance = 1.2f},
	    .creatures = {Actor()},
	    .objects = {rock},
	    .commands = {Do("Scratch", 1.0f)},
	});
	all.push_back({
	    .id = "actions.sleep_by_tree",
	    .name = "Sleeps by a tree",
	    .facet = Facet::Needs,
	    .description = "A grown tiger is told to sleep by an oak 30 metres in front of it.",
	    .expected = "It walks to a clear place by the oak, yawns half the time, faces down the slope and sleeps until "
	                "rested. It gets up dazed, with a grimace.",
	    .environment = {.dispenserGrid = false},
	    .framing = Near(),
	    .creatures = {Actor()},
	    .objects = {{.type = TreeInfo::Oak, .offset = {0.0f, -30.0f}}},
	    .commands = {Do("SleepByObject", 1.0f)},
	});
	all.push_back({
	    .id = "actions.point_at_thing",
	    .name = "Points at a thing",
	    .facet = Facet::Expressions,
	    .description = "A grown tiger is told to point at a cow 40 metres in front of it.",
	    .expected = "It points at the cow for five seconds, amazed, and turns as the cow moves.",
	    .environment = {.dispenserGrid = false},
	    .framing = Near(),
	    .creatures = {Actor()},
	    .objects = {{.type = AnimalInfo::Cow, .offset = {0.0f, -40.0f}}},
	    .commands = {Do("PointAtObject", 1.0f)},
	});
	auto pooer = Actor();
	pooer.needs.poo = 0.9f;
	all.push_back({
	    .id = "actions.poo_at_home",
	    .name = "Poos at home",
	    .facet = Facet::Needs,
	    .description = "A grown tiger full of poo, with no temple, stands where it was made, which is its home. It is "
	                   "told to poo at home. A creature starts this action only within 20 metres of its home.",
	    .expected = "It walks until it is within two times its height of its home. Then it squats and poos there.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Follow, .distance = 1.5f},
	    .creatures = {pooer},
	    .objects = {rock},
	    .commands = {Do("PooAtHome", 1.0f)},
	});
	all.push_back({
	    .id = "actions.rest_on_the_spot",
	    .name = "Rests on the spot",
	    .facet = Facet::Needs,
	    .description = "A grown tiger is told to rest where it stands.",
	    .expected = "It waits a while where it stands, exhausted, and looks at its own feet.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Follow, .distance = 1.2f},
	    .creatures = {Actor()},
	    .objects = {rock},
	    .commands = {Do("RestOnTheSpot", 1.0f)},
	});
	all.push_back({
	    .id = "actions.look_forever",
	    .name = "Watches a thing",
	    .facet = Facet::Senses,
	    .description = "A grown tiger is told to watch a villager 40 metres in front of it.",
	    .expected = "It turns to face the villager and watches it.",
	    .environment = {.dispenserGrid = false},
	    .framing = Near(),
	    .creatures = {Actor()},
	    .objects = {villager},
	    .commands = {Do("LookForever", 1.0f)},
	});
	all.push_back({
	    .id = "actions.watch_player",
	    .name = "Watches the player",
	    .facet = Facet::Senses,
	    .description = "A grown tiger is told to watch the player.",
	    .expected = "It stands where it is and watches the player. The action plays no clip of its own.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Follow, .distance = 1.2f},
	    .creatures = {Actor()},
	    .objects = {rock},
	    .commands = {Do("WatchPlayer", 1.0f)},
	});
	all.push_back({
	    .id = "actions.frighten_villagers",
	    .name = "Frightens villagers in play",
	    .facet = Facet::Mind,
	    .description = "A grown tiger is told to frighten a villager in play.",
	    .expected = "It goes to three times its height from the villager, faces it for two seconds and plays the "
	                "frightened clip with wide eyes.",
	    .environment = {.dispenserGrid = false},
	    .framing = Near(),
	    .creatures = {Actor()},
	    .objects = {villager},
	    .commands = {Do("PlayfullyFrightenVillagers", 1.0f)},
	});
	all.push_back({
	    .id = "actions.point_out_highlight",
	    .name = "Points out a highlight",
	    .facet = Facet::Expressions,
	    .description = "A grown tiger is told to point out a villager to the player, as a lesson's highlight makes it.",
	    .expected = "When the camera is more than 30 metres away, it first looks at the camera, curious, and plays its "
	                "look-at-me. Then it points at the villager a second, amazed, and looks at the camera two seconds "
	                "more.",
	    .environment = {.dispenserGrid = false},
	    .framing = Near(),
	    .creatures = {Actor()},
	    .objects = {villager},
	    .commands = {Do("PointOutHighlight", 1.0f)},
	});
	all.push_back({
	    .id = "actions.make_breeder",
	    .name = "Makes a villager a breeder",
	    .facet = Facet::Mind,
	    .description = "A grown tiger is told to make a villager 40 metres in front of it a breeder.",
	    .expected = "It picks the villager up, strokes it with a look of compassion, makes it a breeder and puts it "
	                "down.",
	    .environment = {.dispenserGrid = false},
	    .framing = Near(),
	    .creatures = {Actor()},
	    .objects = {villager},
	    .commands = {Do("MakeBreederDisciple", 1.0f)},
	});
}
