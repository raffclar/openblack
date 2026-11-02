/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The testbed's scenarios of four silver scrolls of the fourth and fifth lands: the fish puzzle, the creature breeder,
// the stranger who prays at the forest fire, and the explorers who come back. Each loads its land and starts the quest's
// own script.

#include <cstddef>

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

/// A placed camera at an offset from a point of a land, looking at the point at a height above the land
Framing LookingFrom(glm::vec2 middle, glm::vec2 point, glm::vec3 eyeOffset, float height)
{
	const auto at = FromMiddle(middle, point.x, point.y);
	return {.shot = Shot::Placed, .eye = {at.x + eyeOffset.x, eyeOffset.y, at.y + eyeOffset.z}, .look = {at.x, height, at.y}};
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

	// The creature breeder of the fourth land: the kennels and the place of the creatures on show, as the land's launcher
	// of the breeder gives them
	constexpr glm::vec3 k_Kennels {3760.273f, 52.26f, 3412.651f};
	constexpr glm::vec3 k_ShowPlace {3767.373f, 52.26f, 3417.875f};
	std::vector<Command> breederCommands {
	    {.kind = Kind::RunScript,
	     .delaySeconds = 0.5f,
	     .script = "RunCreatureBreederLand",
	     .scriptArguments = {{.kind = Argument::Kind::Number, .number = 4.0f}},
	     // As the land control scripts set them up: the player's creature, and no swap at the breeder yet
	     .scriptGlobals = {{"MyCreature", {.kind = Argument::Kind::Creature, .index = 0}},
	                       {"GlobalCurrentCreature", {.number = -1.0f}},
	                       {"GlobalOldCreature", {.number = -1.0f}},
	                       {"BreederRunning", {.number = 0.0f}},
	                       {"NrOfChoosenCreature", {.number = 0.0f}},
	                       {"GlobalReset", {.number = 0.0f}}}},
	};
	ClickMiddle(breederCommands, 11.0f, 1, 0.0f);
	creature_swap::Unlocked leopardAndHorse;
	leopardAndHorse.set(static_cast<size_t>(creature_swap::Bonus::Leopard));
	leopardAndHorse.set(static_cast<size_t>(creature_swap::Bonus::Horse));
	all.push_back({
	    .id = "story.lfour_breeder",
	    .name = "Story: the creature breeder of the fourth land",
	    .facet = Facet::Story,
	    .description = "The fourth land loads with its own scripts and sky. The Leopard and the Horse are taken as unlocked. "
	                   "The land's own launcher of the breeder runs, which finds the kennels as a house. The camera looks "
	                   "at the kennels. Eleven seconds in, the pointer at the middle of the screen clicks the scroll.",
	    .expected = "The breeder, an ordinary man, stands by a Norse house of the neutral Norse village in the east of the "
	                "land. Ten seconds in, a silver scroll stands over the house. On the click the breeder walks to his "
	                "soap box, the camera glides to him, and the Leopard and the Horse appear side by side in sparkles, "
	                "each with a scroll. Click the scroll of one with the Tiger within 50 metres to choose it, then click "
	                "that creature to swap: the two creatures walk to two spots 30 to 50 metres east of the kennels, on "
	                "ground of the same height, and change bodies.",
	    .environment =
	        {.land = Land::Story, .storyLand = "Land4.txt", .dispenserGrid = false, .unlockedBonusCreatures = leopardAndHorse},
	    .framing = LookingFrom(k_Land4Middle, {k_Kennels.x, k_Kennels.z}, {-20.0f, 10.0f, 22.0f}, 5.0f),
	    .creatures = {PlayersCreature(CreatureType::Tiger,
	                                  FromMiddle(k_Land4Middle, k_ShowPlace.x + 10.0f, k_ShowPlace.z - 12.0f), 0.0f,
	                                  "player's tiger")},
	    .commands = breederCommands,
	});

	// The stranger of the fifth land: his praying place at the fire, as the quest's script gives it
	constexpr glm::vec3 k_PrayingPlace {2197.82f, 26.609f, 3502.809f};
	// The set-up of the fifth land, the Wonders, a time of day and then the stranger's script
	const auto traitorStart = [](float hour) {
		std::vector<Command> commands {
		    {.kind = Kind::RunScript, .delaySeconds = 0.5f, .script = "SetupLand5"},
		};
		BuildNemesisWonders(commands);
		commands.push_back({.kind = Kind::CallNative,
		                    .delaySeconds = 0.0f,
		                    .script = "SET_GAME_TIME",
		                    .scriptArguments = {{.kind = Argument::Kind::Number, .number = hour}}});
		commands.push_back({.kind = Kind::RunScript, .delaySeconds = 0.5f, .script = "JapaneseTraitor"});
		return commands;
	};
	auto traitorCommands = traitorStart(19.0f);
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
	all.push_back({
	    .id = "story.l5_traitor_fire_remarks",
	    .name = "Story: the good advisor remarks on the forest fire of the fifth land",
	    .facet = Facet::Story,
	    .description = "As story.l5_japanese_traitor, but the time of day jumps to 15:45 and nothing clicks. The camera "
	                   "stays on the praying place into the evening.",
	    .expected = "The good advisor comes out, points at the fire and says \"I sense something special about this "
	                "place.\" and \"Don't forget it. We should come back this way later.\" one time. About a minute "
	                "later the clock passes 16:00, the stranger comes and the bonfire turns into a lantern. After that "
	                "she says nothing more about the fire, also after 18:00, because her remarks watch the first "
	                "bonfire, which is gone.",
	    .environment = {.land = Land::Story, .storyLand = "Land5.txt", .dispenserGrid = false},
	    .framing = LookingAt(k_Land5Middle, {k_PrayingPlace.x, k_PrayingPlace.z}, 12.5f),
	    .creatures = {PlayersCreature(CreatureType::Tiger,
	                                  FromMiddle(k_Land5Middle, k_PrayingPlace.x - 30.0f, k_PrayingPlace.z - 30.0f), 45.0f,
	                                  "player's tiger")},
	    .commands = traitorStart(15.75f),
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
	                   "first land. The camera looks at the scroll by their wreck from the east. Eight "
	                   "seconds in, the pointer at the middle of the screen clicks it three times, four seconds apart.",
	    .expected = "The wreck of the explorers' boat lies on the shore, and the empty Norse town above it gets all its "
	                "eleven buildings and twenty villagers at once. On the click a film plays with the explorers' tune: "
	                "three sailors come out of the wreck and dance, the advisors talk, the sailors thank the player and "
	                "a polar bear walks up the beach. The challenge \"The Explorers Again\" is recorded as done, and a "
	                "silver scroll hangs over the bear, which the player's creature can swap with.",
	    .environment = {.land = Land::Story, .storyLand = "Land5.txt", .dispenserGrid = false},
	    // The wreck hides the scroll from the west and from above, so the camera looks at it from the east
	    .framing = LookingFrom(k_Land5Middle, {k_WreckScroll.x, k_WreckScroll.z}, {17.0f, 8.0f, -6.0f}, 4.0f),
	    .creatures = {PlayersCreature(CreatureType::Tiger,
	                                  FromMiddle(k_Land5Middle, k_WreckScroll.x - 15.0f, k_WreckScroll.z - 20.0f), 30.0f,
	                                  "player's tiger")},
	    .commands = explorerCommands,
	});
}
