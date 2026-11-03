/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The testbed's scenarios of the puzzle quests of the fourth and fifth lands: the two Theseus mazes and their prize,
// the fish puzzle and the steps of a creature in it, the bronze totem puzzle, and the lion maze of Stanley the wolf

#include <glm/vec2.hpp>

#include "TestbedScenarioRegistry.h"

using namespace openblack;
using namespace openblack::testbed_scenarios;

namespace
{
using Argument = ScriptArgument;
using Kind = Command::Kind;

/// The middle of the blocks of the fourth land. The testbed measures the points of a scenario from it.
constexpr glm::vec2 k_Land4Middle {2720.0f, 2800.0f};
/// The place of the Theseus mazes, from the middle of the fourth land
constexpr glm::vec2 k_TheseusMazes = glm::vec2(1901.468f, 2451.211f) - k_Land4Middle;
/// The place of the fish puzzle in the bay, from the middle of the fourth land
constexpr glm::vec2 k_FishPuzzle = glm::vec2(2497.891f, 3628.35f) - k_Land4Middle;
/// The script's kind of object of a puzzle game, and the kind of the fish puzzle
constexpr float k_PuzzleObjectType = 32.0f;
constexpr float k_FishPuzzleKind = 14.0f;
/// A creature starts its walk on the shore west of the fish puzzle, and walks to the edge of the sea, where the land is
/// less than a metre high
constexpr glm::vec2 k_FishWalkStart = glm::vec2(2432.0f, 3592.0f) - k_Land4Middle;
constexpr glm::vec2 k_FishWalkEnd = glm::vec2(2447.0f, 3618.0f) - k_Land4Middle;
/// The place of the bronze totem puzzle north-west of the Japanese village, from the middle of the fourth land
constexpr glm::vec2 k_TotemPuzzle = glm::vec2(2286.4448f, 3588.0894f) - k_Land4Middle;
/// The middle of the blocks of the fifth land
constexpr glm::vec2 k_Land5Middle {2720.0f, 3040.0f};
/// The middle of the lion maze, from the middle of the fifth land
constexpr glm::vec2 k_LionMaze = glm::vec2(1634.0f, 2983.0f) - k_Land5Middle;

void AddTheseusQuest(std::vector<Scenario>& all)
{
	all.push_back({
	    .id = "story.l4_theseus_quest",
	    .name = "Story: the two Theseus mazes of the fourth land and their prize",
	    .facet = Facet::Story,
	    .description = "The fourth land loads with its own scripts and sky. The set-up script of the land runs, then the "
	                   "script of the Theseus mazes. Tap the land beside the villager to move him, until he walks out of "
	                   "the maze.",
	    .expected = "The first Theseus maze stands in a ring of influence, with its sign. When the villager walks out, "
	                "the maze goes and the second maze comes in its place. When the villager walks out of the second "
	                "maze, the sign goes, and the prize falls from the sky into the home village.",
	    .environment = {.land = Land::Story, .storyLand = "Land4.txt", .dispenserGrid = false},
	    .framing = {.shot = Shot::Placed,
	                .eye = {k_TheseusMazes.x + 14.0f, 45.0f, k_TheseusMazes.y - 30.0f},
	                .look = {k_TheseusMazes.x + 14.0f, 0.0f, k_TheseusMazes.y + 14.0f}},
	    .commands = {{.kind = Kind::RunScript, .delaySeconds = 0.5f, .script = "SetupLand4"},
	                 {.kind = Kind::RunScript, .delaySeconds = 0.5f, .script = "ThesiusPuzzleLand4"}},
	});
}

void AddFishHerding(std::vector<Scenario>& all)
{
	all.push_back({
	    .id = "story.l4_fish_herding",
	    .name = "Story: the fish puzzle of the fourth land, herding the fish into the ring of floats",
	    .facet = Facet::Story,
	    .description = "The fourth land loads with its own scripts and sky. The fish puzzle is made in the bay, as its "
	                   "script makes it. Grip the sea with the hand behind a shoal to scare its fish towards the ring.",
	    .expected = "Seven floats bob in a ring 22 metres across, seen under the sea. A shoal of fifteen fish swims on "
	                "each side of the ring, a little north of it. A grip of the sea scares the fish near it, which dart "
	                "away. When all thirty fish stay in the ring for half a second, each makes a ring on the water and "
	                "goes, and the floats close in to a small ring.",
	    .environment = {.land = Land::Story, .storyLand = "Land4.txt", .dispenserGrid = false},
	    .framing = {.shot = Shot::Placed,
	                .eye = {k_FishPuzzle.x, 40.0f, k_FishPuzzle.y - 30.0f},
	                .look = {k_FishPuzzle.x, 0.0f, k_FishPuzzle.y + 6.0f}},
	    .commands = {{.kind = Kind::CallNative,
	                  .delaySeconds = 0.5f,
	                  .script = "CREATE_WITH_ANGLE_AND_SCALE",
	                  .scriptArguments = {{.number = 0.0f},
	                                      {.number = 1.0f},
	                                      {.kind = Argument::Kind::Integer, .number = k_PuzzleObjectType},
	                                      {.kind = Argument::Kind::Integer, .number = k_FishPuzzleKind},
	                                      {.kind = Argument::Kind::Position, .point = k_FishPuzzle}}}},
	});
}

void AddFishCreatureSteps(std::vector<Scenario>& all)
{
	all.push_back({
	    .id = "story.l4_fish_creature_steps",
	    .name = "Story: a creature walks to the edge of the sea by the fish puzzle and its steps scare the fish",
	    .facet = Facet::Story,
	    .description = "The fourth land loads with its own scripts and sky. The fish puzzle is made in the bay, as its "
	                   "script makes it. A tiger comes on the shore west of the puzzle and walks to the edge of the sea.",
	    .expected = "The tiger walks to the edge of the sea. Each step of a foot lower than 1 m scares the fish: each "
	                "shoal goes to a new point near its centre, and fish near the foot dart away. The scare comes in the "
	                "game turn after the step, the same on each computer.",
	    .environment = {.land = Land::Story, .storyLand = "Land4.txt", .dispenserGrid = false},
	    .framing = {.shot = Shot::Placed,
	                .eye = {k_FishWalkEnd.x - 10.0f, 45.0f, k_FishWalkEnd.y - 45.0f},
	                .look = {(k_FishWalkEnd.x + k_FishPuzzle.x) * 0.5f, 0.0f, k_FishPuzzle.y}},
	    .creatures = {{.label = "player's tiger", .species = CreatureType::Tiger, .offset = k_FishWalkStart}},
	    .commands = {{.kind = Kind::CallNative,
	                  .delaySeconds = 0.5f,
	                  .script = "CREATE_WITH_ANGLE_AND_SCALE",
	                  .scriptArguments = {{.number = 0.0f},
	                                      {.number = 1.0f},
	                                      {.kind = Argument::Kind::Integer, .number = k_PuzzleObjectType},
	                                      {.kind = Argument::Kind::Integer, .number = k_FishPuzzleKind},
	                                      {.kind = Argument::Kind::Position, .point = k_FishPuzzle}}},
	                 {.kind = Kind::WalkTo, .creature = 0, .delaySeconds = 1.0f, .point = k_FishWalkEnd}},
	});
}

void AddBronzeTotemPuzzle(std::vector<Scenario>& all)
{
	all.push_back({
	    .id = "story.l4_bronze_totem_puzzle",
	    .name = "Story: the bronze totem puzzle of the fourth land",
	    .facet = Facet::Story,
	    .description = "The fourth land loads with its own scripts and sky. The script of the bronze totem puzzle starts as "
	                   "the land's script starts it. Take hold of a totem with the Action button and move the mouse up or "
	                   "down to change its height. Do this until all six totems are fully up.",
	    .expected = "Six bronze totems stand on a half circle in a ring of influence, all fully down, with a sign beside "
	                "them. When a totem changes height, some of the other totems slide to new heights. A sparkle stays "
	                "over each totem that is fully up. When all six are fully up, each totem sparkles, the sign and the "
	                "ring go, and a shield miracle dispenser comes in a new ring of influence.",
	    .environment = {.land = Land::Story, .storyLand = "Land4.txt", .dispenserGrid = false},
	    .framing = {.shot = Shot::Placed,
	                .eye = {k_TotemPuzzle.x + 10.0f, 40.0f, k_TotemPuzzle.y - 30.0f},
	                .look = {k_TotemPuzzle.x + 10.0f, 0.0f, k_TotemPuzzle.y + 5.0f}},
	    .commands = {{.kind = Kind::RunScript, .delaySeconds = 0.5f, .script = "JapaneseTotemPuzzle"}},
	});
}

void AddLionMazeQuest(std::vector<Scenario>& all)
{
	all.push_back({
	    .id = "story.l5_lion_maze_quest",
	    .name = "Story: Stanley the wolf and the lion maze of the fifth land",
	    .facet = Facet::Story,
	    .description = "The fifth land loads with its own scripts and sky. The script of the lion maze starts as the "
	                   "land's script starts it, with the player's Tiger. Click the silver scroll by the maze, then tap "
	                   "the bells round the maze to slide the wolf onto the sheep.",
	    .expected = "The lion maze stands with its five bells, and the wolf's owner sits by his camp fire. A click on the "
	                "scroll starts his scene. When the wolf stops on the sheep, both go in a sparkle, and the prize "
	                "comes: a lion to swap for and the fireball's higher levels in the home village.",
	    .environment = {.land = Land::Story, .storyLand = "Land5.txt", .dispenserGrid = false},
	    .framing = {.shot = Shot::Placed,
	                .eye = {k_LionMaze.x, 50.0f, k_LionMaze.y - 45.0f},
	                .look = {k_LionMaze.x, 0.0f, k_LionMaze.y}},
	    .creatures = {{.label = "player's tiger", .species = CreatureType::Tiger, .offset = {0.0f, 0.0f}}},
	    // A ring of the player's influence round the maze lets the hand reach the bells, as the player's towns of the
	    // fifth land do
	    .commands = {{.kind = Kind::CallNative,
	                  .delaySeconds = 0.2f,
	                  .script = "INFLUENCE_POSITION",
	                  .scriptArguments = {{.kind = Argument::Kind::Position, .point = k_LionMaze},
	                                      {.number = 60.0f},
	                                      {.kind = Argument::Kind::Integer, .number = 0.0f},
	                                      {.kind = Argument::Kind::Integer, .number = 0.0f}}},
	                 {.kind = Kind::RunScript,
	                  .delaySeconds = 0.5f,
	                  .script = "LionPuzzle",
	                  .scriptGlobals = {{"MyCreature", {.kind = Argument::Kind::Creature, .index = 0}}}}},
	});
}
} // namespace

void testbed_scenarios::AddPuzzleQuestScenarios(std::vector<Scenario>& all)
{
	AddTheseusQuest(all);
	AddFishHerding(all);
	AddFishCreatureSteps(all);
	AddBronzeTotemPuzzle(all);
	AddLionMazeQuest(all);
}
