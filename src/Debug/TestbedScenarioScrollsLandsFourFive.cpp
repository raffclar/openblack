/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The testbed's scenarios of three silver scrolls of the fourth and fifth lands: the fish puzzle, the stranger who
// prays at the forest fire, and the explorers who come back. Each loads its land and starts the quest's own script.

#include <array>
#include <string_view>
#include <vector>

#include "3D/FlatLand.h"
#include "TestbedScenarioRegistry.h"

using namespace openblack;
using namespace openblack::testbed_scenarios;

namespace
{
using Kind = Command::Kind;
using Argument = ScriptArgument;

/// The middle of the fourth land's extent, from which the testbed takes the places of a scenario on that land
constexpr glm::vec2 k_Land4Middle {2720.0f, 2800.0f};
/// The middle of the fifth land's extent
constexpr glm::vec2 k_Land5Middle {2720.0f, 3040.0f};

/// A point of a land, as the scripts give it, from the middle of the land's extent
constexpr glm::vec2 FromMiddle(glm::vec2 middle, float x, float z)
{
	return glm::vec2(x, z) - middle;
}

/// A creature of the player that stands still and wants nothing, so that only the scripts move it
CreatureSetup PlayersCreature(CreatureType species, glm::vec2 offset, float facing, std::string_view label)
{
	return {.label = label,
	        .species = species,
	        .offset = offset,
	        .facingDegrees = facing,
	        .phase = 3,
	        .needs = {.energy = 1.0f, .exhaustion = 0.0f, .dehydration = 0.0f, .poo = 0.0f, .life = 1.0f, .warmth = 0.0f}};
}

/// The fifth land's control script builds Nemesis's Aztec, Greek and Tibetan Wonders to six tenths as the land starts.
/// The scenario does the same with the same natives, so that the stranger has his three Wonders to tell of.
void BuildNemesisWonders(std::vector<Command>& commands)
{
	constexpr std::array<glm::vec2, 3> k_Wonders {{{2663.767f, 3671.004f}, {2331.919f, 2581.005f}, {1758.438f, 2337.047f}}};
	for (const auto wonder : k_Wonders)
	{
		const Argument place {.kind = Argument::Kind::Position, .point = FromMiddle(k_Land5Middle, wonder.x, wonder.y)};
		commands.push_back({.kind = Kind::CallNative,
		                    .delaySeconds = 0.0f,
		                    .script = "BUILD_BUILDING",
		                    .scriptArguments = {place, {.kind = Argument::Kind::Number, .number = 0.6f}}});
	}
	for (const auto wonder : k_Wonders)
	{
		const Argument place {.kind = Argument::Kind::Position, .point = FromMiddle(k_Land5Middle, wonder.x, wonder.y)};
		// The Wonder that the script finds at the place, then how much of it is built
		commands.push_back({.kind = Kind::CallNative,
		                    .delaySeconds = 0.0f,
		                    .script = "CALL",
		                    .scriptArguments = {{.kind = Argument::Kind::Integer, .number = 2.0f},
		                                        {.kind = Argument::Kind::Integer, .number = 5000.0f},
		                                        place,
		                                        {.kind = Argument::Kind::Number, .number = 0.0f}}});
		commands.push_back({.kind = Kind::CallNative,
		                    .delaySeconds = 0.0f,
		                    .script = "SET_PROPERTY",
		                    .scriptArguments = {{.kind = Argument::Kind::Integer, .number = 22.0f},
		                                        {.kind = Argument::Kind::LastResult},
		                                        {.kind = Argument::Kind::Number, .number = 0.6f}}});
	}
}

/// The pointer at the middle of the screen, where the scenario's camera looks, presses and lets go of the Action
/// button a number of times, some seconds apart
void ClickMiddle(std::vector<Command>& commands, float firstDelay, int clicks, float apart)
{
	commands.push_back({.kind = Kind::PointerTo, .delaySeconds = firstDelay, .point = {0.5f, 0.5f}});
	for (int i = 0; i < clicks; ++i)
	{
		commands.push_back({.kind = Kind::PointerPress, .delaySeconds = i == 0 ? 0.5f : apart, .value = 3});
		commands.push_back({.kind = Kind::PointerRelease, .delaySeconds = 0.1f, .value = 3});
	}
}

/// A placed camera 20 metres south west of a point of a land and 20 metres above the land. It looks at the point, at a
/// height above the land, where a scroll floats.
Framing LookingAt(glm::vec2 middle, glm::vec2 point, float height)
{
	const auto at = FromMiddle(middle, point.x, point.y);
	return {.shot = Shot::Placed, .eye = {at.x - 20.0f, 20.0f, at.y - 20.0f}, .look = {at.x, height, at.y}};
}
} // namespace

void testbed_scenarios::AddLandsFourFiveScrollScenarios(std::vector<Scenario>& all)
{
	// The fish puzzle of the fourth land: the boy's spot and the scroll beside him, as the quest's script gives them
	constexpr glm::vec3 k_FishermanSpot {2427.0f, 2.68f, 3612.758f};
	constexpr glm::vec3 k_FishScroll {2436.942f, 2.68f, 3598.528f};
	std::vector<Command> fishCommands {
	    {.kind = Kind::RunScript, .delaySeconds = 0.5f, .script = "SetupLand4"},
	    {.kind = Kind::RunScript,
	     .delaySeconds = 0.5f,
	     .script = "FishPuzzle",
	     .scriptGlobals = {{"MyCreature", {.kind = Argument::Kind::Creature, .index = 0}}}},
	};
	ClickMiddle(fishCommands, 12.0f, 3, 4.0f);
	all.push_back({
	    .id = "story.l4_fish_puzzle",
	    .name = "Story: the fish puzzle of the fourth land",
	    .facet = Facet::Story,
	    .description = "The fourth land loads with its own scripts and sky, and the player's Tiger stands on the shore. Its "
	                   "set-up script runs, then the script of the fish puzzle. The camera looks at the place "
	                   "of the silver scroll beside the boy. Twelve seconds in, the pointer at the middle of the screen "
	                   "clicks it three times, four seconds apart.",
	    .expected = "A Japanese boy is made a few paces from his spot on the shore and walks to it, with five tortoises "
	                "beside him and a silver scroll. On the click a film starts: the camera glides to the boy, who says "
	                "that he wants to be a fisherman like his dad. The fish puzzle is made in the bay, and the camera "
	                "cuts to look down on it while he tells how to tap the water. The challenge log then shows \"The "
	                "Fish Puzzle\" and the film ends.",
	    .environment = {.land = Land::Story, .storyLand = "Land4.txt", .dispenserGrid = false},
	    .framing = LookingAt(k_Land4Middle, {k_FishScroll.x, k_FishScroll.z}, 2.0f),
	    .creatures = {PlayersCreature(CreatureType::Tiger,
	                                  FromMiddle(k_Land4Middle, k_FishermanSpot.x - 20.0f, k_FishermanSpot.z - 10.0f), 45.0f,
	                                  "player's tiger")},
	    .commands = fishCommands,
	});

	// The stranger of the fifth land: his praying place at the fire, as the quest's script gives it
	constexpr glm::vec3 k_PrayingPlace {2197.82f, 26.609f, 3502.809f};
	std::vector<Command> traitorCommands {
	    {.kind = Kind::RunScript, .delaySeconds = 0.5f, .script = "SetupLand5"},
	};
	BuildNemesisWonders(traitorCommands);
	traitorCommands.push_back({.kind = Kind::CallNative,
	                           .delaySeconds = 0.0f,
	                           .script = "SET_GAME_TIME",
	                           .scriptArguments = {{.kind = Argument::Kind::Number, .number = 19.0f}}});
	traitorCommands.push_back({.kind = Kind::RunScript, .delaySeconds = 0.5f, .script = "JapaneseTraitor"});
	ClickMiddle(traitorCommands, 40.0f, 8, 5.0f);
	all.push_back({
	    .id = "story.l5_japanese_traitor",
	    .name = "Story: the stranger who prays at the forest fire of the fifth land",
	    .facet = Facet::Story,
	    .description = "The fifth land loads with its own scripts and sky. Its set-up script runs, the time of day jumps to "
	                   "19:00, and the script of the stranger starts. As the land's control script does, the Aztec, Greek "
	                   "and Tibetan Wonders are built to six tenths first. The camera looks at his praying place. Forty "
	                   "seconds in, the pointer at the middle of the screen clicks there eight times, five seconds apart.",
	    .expected = "A bonfire burns in the forest. A stranger dressed as a crusader comes out of the woods and walks to "
	                "it; the bonfire turns into a lantern, he faces it and prays, and a silver scroll appears. On the "
	                "click a film plays with the second generic script tune: he says that the player is the true god, "
	                "tells of the Aztec Wonder, which drains the creature's strength, while the camera pans across the "
	                "Wonder, and asks the player to come back tomorrow. The camera then goes back and he prays again.",
	    .environment = {.land = Land::Story, .storyLand = "Land5.txt", .dispenserGrid = false},
	    .framing = LookingAt(k_Land5Middle, {k_PrayingPlace.x, k_PrayingPlace.z}, 12.5f),
	    .creatures = {PlayersCreature(CreatureType::Tiger,
	                                  FromMiddle(k_Land5Middle, k_PrayingPlace.x - 30.0f, k_PrayingPlace.z - 30.0f), 45.0f,
	                                  "player's tiger")},
	    .commands = traitorCommands,
	});

	// The explorers of the fifth land: the scroll by their wreck, as the quest's script gives it
	constexpr glm::vec3 k_WreckScroll {1583.323f, 1.5f, 3920.859f};
	std::vector<Command> explorerCommands {
	    {.kind = Kind::RunScript, .delaySeconds = 0.5f, .script = "SetupLand5"},
	    {.kind = Kind::RunScript, .delaySeconds = 0.5f, .script = "MissionariesReturned"},
	};
	ClickMiddle(explorerCommands, 8.0f, 3, 4.0f);
	all.push_back({
	    .id = "story.l5_explorers_again",
	    .name = "Story: the explorers of the first land come back on the fifth land",
	    .facet = Facet::Story,
	    .description = "The fifth land loads with its own scripts and sky, and the player's Tiger stands on the shore. Its "
	                   "set-up script runs, then the script of the explorers who come back, as when they sailed on the "
	                   "first land. The camera looks at the scroll by their wreck. Eight "
	                   "seconds in, the pointer at the middle of the screen clicks it three times, four seconds apart.",
	    .expected = "The wreck of the explorers' boat lies on the shore, and the empty Norse town above it gets all its "
	                "eleven buildings and twenty villagers at once. On the click a film plays with the explorers' tune: "
	                "three sailors come out of the wreck and dance, the advisors talk, the sailors thank the player and "
	                "a polar bear walks up the beach. The challenge \"The Explorers Again\" is recorded as done, and a "
	                "silver scroll hangs over the bear, which the player's creature can swap with.",
	    .environment = {.land = Land::Story, .storyLand = "Land5.txt", .dispenserGrid = false},
	    .framing = LookingAt(k_Land5Middle, {k_WreckScroll.x, k_WreckScroll.z}, 3.0f),
	    .creatures = {PlayersCreature(CreatureType::Tiger,
	                                  FromMiddle(k_Land5Middle, k_WreckScroll.x - 15.0f, k_WreckScroll.z - 20.0f), 30.0f,
	                                  "player's tiger")},
	    .commands = explorerCommands,
	});
}
