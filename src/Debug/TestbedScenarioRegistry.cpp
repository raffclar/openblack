/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "TestbedScenarioRegistry.h"

#include <cmath>

#include <algorithm>
#include <array>
#include <iterator>

#include <fmt/format.h>
#include <glm/geometric.hpp>

#include "3D/FlatLand.h"
#include "3D/TempleInteriorInterface.h"
#include "Common/PortableMath.h"
#include "Creature/CreatureCave.h"
#include "Creature/CreatureFace.h"
#include "Creature/CreatureFeedback.h"
#include "Creature/CreatureFight.h"
#include "Creature/CreatureIdleMind.h"
#include "Creature/CreatureLayers.h"
#include "Creature/CreatureObjectActions.h"
#include "ECS/IncomingMessageRules.h"
#include "Particles/ParticleTypes.h"
#include "ScriptHeaders/ScriptEnums.h"
#include "Sound/GameMusic.h"
#include "TestbedDispenserGrid.h"
#include "TestbedScenarioBuilders.h"

using namespace openblack;
using namespace openblack::testbed_scenarios;

// How to add a scenario: append one to the list Build() returns, in the section of its facet. Give it a new id
// ("facet.what", never reused or renamed, as tests and the command line pick scenarios by it), a name, its facet, a
// description of what it sets up and what to look for, and then its data: the environment (land, hour, weather, body
// time), the framing, the creatures with the bodies, needs and desires they start with, the objects on the land, and
// the commands, which play in turn (commands on objects name them by their place in the scenario's objects). Offsets are from
// the middle of the map, x east and y north; the testbed's camera looks north from 120 units south of the middle, and the lake
// lies beyond the middle (k_Lake, its open water 100 units a side, ringed by 20 units of shallows and a bank). Keep other
// scenarios clear of it. A new facet goes into the Facet enum and its Name. test_testbed_scenarios
// checks the data of every scenario; anything a scenario needs that the runner can't yet do goes into these types and
// TestbedScenarioRunner together.

namespace
{
/// A task of a land's script that stops gives back the objects that its variables hold. The objects stay in a script
/// until the scripts' turn ends, and then they are in no script
void AddScriptTaskRelease(std::vector<Scenario>& all)
{
	using Argument = ScriptArgument;
	const auto stop = [](float delaySeconds) {
		return Command {.kind = Kind::StopScript, .delaySeconds = delaySeconds, .script = "CheckForCommentOnFoodItems"};
	};
	all.push_back({
	    .id = "story.stopped_task_lets_go",
	    .name = "A stopped script task lets go of the object that it holds",
	    .facet = Facet::Story,
	    .description = "The game's CheckForCommentOnFoodItems script runs with a food pot as its parameter, which it "
	                   "keeps in a variable. One second later the scenario stops the script, and one second after that "
	                   "it stops the script again.",
	    .expected = "The log tells that the first stop stopped one task and that the pot is still in a script for "
	                "the rest of that turn. The second stop stopped no task, and the pot is in no script.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview},
	    .objects = {{.type = PotInfo::FoodPot, .offset = {0.0f, -20.0f}}},
	    .commands = {{.kind = Kind::RunScript,
	                  .delaySeconds = 0.5f,
	                  .script = "CheckForCommentOnFoodItems",
	                  .scriptArguments = {{.kind = Argument::Kind::Object, .index = 0}}},
	                 stop(1.0f),
	                 stop(1.0f)},
	});

	all.push_back({
	    .id = "temple.room_help_without_sound",
	    .name = "The temple's room help when no sound can play",
	    .facet = Facet::Story,
	    .description = "Start the game with no sound device (for example with the environment ALSOFT_DRIVERS=none). "
	                   "The player has a temple, and is put in its main room for the first time.",
	    .expected = "The advisors come out and the help shows its lines one at a time, with no voice. Each line stays for "
	                "its reading time: 5 game turns for each word and 8 more. The help shows all nine lines, then the "
	                "advisors go home. With a sound device, each line stays until the advisor stops talking.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview},
	    .temples = {{.offset = {-300.0f, 100.0f}, .angle = 36.0f}},
	    .commands = {{.kind = Kind::EnterTemple, .delaySeconds = 2.0f, .value = 3}},
	});

	all.push_back({
	    .id = "temple.main_room_lean",
	    .name = "The temple's main room leans at the screen's edge and as the floor is dragged",
	    .facet = Facet::Story,
	    .description = "The player has a temple and is put in its main room. Move the pointer near the top or the bottom "
	                   "of the screen, then drag the floor up and down.",
	    .expected = "The camera starts to lean when the pointer is more than 0.8 of the way from the middle of the "
	                "screen to the top or bottom edge, and leans faster nearer the edge. Dragging the floor leans the "
	                "camera by 0.06 for each pixel and second, less the nearer the pointer is to the sides.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview},
	    .temples = {{.offset = {-300.0f, 100.0f}, .angle = 36.0f}},
	    .commands = {{.kind = Kind::EnterTemple, .delaySeconds = 5.0f, .value = 3}},
	});

	all.push_back({
	    .id = "temple.low_detail_rooms",
	    .name = "At the lowest detail the temple's rooms have no light maps",
	    .facet = Facet::Story,
	    .description = "Start the game with --detail-level 0. The player has a temple and is put in its main room. Walk "
	                   "through the doors into the other rooms.",
	    .expected = "Each room, its floor and its pools are the rooms' low models: plainer walls with no light from "
	                "light maps, each wall hazed as one piece. The domes are as at other levels. The doors, the "
	                "scrolls and the clicks on them work as at other levels. With no --detail-level, or above 0, the "
	                "rooms have their light maps.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview},
	    .temples = {{.offset = {-300.0f, 100.0f}, .angle = 36.0f}},
	    .commands = {{.kind = Kind::EnterTemple, .delaySeconds = 2.0f, .value = 3}},
	});

	all.push_back({
	    .id = "temple.low_detail_scroll_text",
	    .name = "At the lowest detail levels the scrolls have textures of half the size",
	    .facet = Facet::Appearance,
	    .description = "Start the game with --detail-level 1 (Low) or 0 (Minimum). The player has a temple and is put in "
	                   "its main room. Look at the scrolls on the walls from a distance, then start again with "
	                   "--detail-level 4 to compare.",
	    .expected = "At Low and Minimum the text and the parchment of each scroll are softer, with half the texels "
	                "along each side. Each texel is the mean of four texels of the full text. When the camera goes "
	                "close to a scroll, the text in front of it is as sharp as at other levels.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview},
	    .temples = {{.offset = {-300.0f, 100.0f}, .angle = 36.0f}},
	    .commands = {{.kind = Kind::EnterTemple, .delaySeconds = 2.0f, .value = 3}},
	});

	all.push_back({
	    .id = "temple.low_detail_map",
	    .name = "At the lowest detail levels the temple's map has a texture of half the size",
	    .facet = Facet::Appearance,
	    .description = "Start the game with --detail-level 1 (Low) or 0 (Minimum). The player has a temple and is put in "
	                   "its main room. Look at the map of the island over the pool, then start again with "
	                   "--detail-level 4 to compare.",
	    .expected = "At Low and Minimum the land on the map has half the texels along each side: 4 for each block of the "
	                "land, not 8. Each texel has the colour of one texel of the full map, not a mean of four. The "
	                "shape of the map, its markers and its fade at the coast do not change.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview},
	    .temples = {{.offset = {-300.0f, 100.0f}, .angle = 36.0f}},
	    .commands = {{.kind = Kind::EnterTemple, .delaySeconds = 2.0f, .value = 3}},
	});

	all.push_back({
	    .id = "temple.way_out_glow",
	    .name = "A white light grows at the temple's way out as the player leaves",
	    .facet = Facet::Story,
	    .description = "The player has a temple and is put in its main room. After ten seconds the camera walks to the "
	                   "door out of the temple and through it, as a click on that door does.",
	    .expected = "The camera turns to the door out. 2.2 seconds after the walk starts, a white sheet of light grows "
	                "on the floor from the door towards the middle of the room, added over all of the room, its texture "
	                "slowly drifting. It is brightest and widest at the door and fades to nothing towards the middle. "
	                "Then the player leaves the temple.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview},
	    .temples = {{.offset = {-300.0f, 100.0f}, .angle = 36.0f}},
	    .commands = {{.kind = Kind::EnterTemple, .delaySeconds = 2.0f, .value = 3},
	                 {.kind = Kind::EnterTemple, .delaySeconds = 10.0f, .value = static_cast<size_t>(TempleRoom::Unknown)}},
	});
	// The second visit uses the draws and the scroll textures of the first visit again
	all.push_back({
	    .id = "temple.visit_again",
	    .name = "The player goes into the temple, leaves it and goes in again",
	    .facet = Facet::Story,
	    .description = "The player has a temple and goes into its main room after two seconds. After six seconds the "
	                   "camera walks out through the door. After sixteen seconds the player goes into the main room "
	                   "again.",
	    .expected = "Each time, the camera comes into the main room along its path and the room, its scrolls and the "
	                "map of the land show as on the first visit. Outside, the land and its things show as before the "
	                "visit. No frame of a visit, of the walk out or of the land after it takes 10 ms or more.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview},
	    .temples = {{.offset = {-300.0f, 100.0f}, .angle = 36.0f}},
	    .commands = {{.kind = Kind::EnterTemple, .delaySeconds = 2.0f, .value = static_cast<size_t>(TempleRoom::Main)},
	                 {.kind = Kind::EnterTemple, .delaySeconds = 6.0f, .value = static_cast<size_t>(TempleRoom::Unknown)},
	                 {.kind = Kind::EnterTemple, .delaySeconds = 16.0f, .value = static_cast<size_t>(TempleRoom::Main)}},
	});
}

/// Land 1's script natives and land script commands that the lane land1-script-stubs completed
void AddLandOneScriptStubScenarios(std::vector<Scenario>& all)
{
	using Argument = ScriptArgument;
	// A script's dance about a person, as the Pied Piper's dance of Land 1, which wants its camera at once
	all.push_back({
	    .id = "story.dance_camera",
	    .name = "The camera of a dance holds the view until a grip of the land",
	    .facet = Facet::Story,
	    .description = "A leader stands in the middle with six villagers round him. After one second, a script makes the "
	                   "dance about a person (the dance of the Pied Piper on Land 1) about the leader, and the six "
	                   "villagers join it. The scenario says that the hand was near the dance for more than ten seconds, "
	                   "so the dance wants its camera.",
	    .expected = "The villagers walk to their places and dance. At the first key frame of the dance that asks for "
	                "the camera, the camera holds its view. The keys, the wheel and the mouse do not move it. A grip of "
	                "the land with the left button gives the camera back in the next game turn, and the drag of the land "
	                "starts. The dance does not take the camera again. The log tells the number of dancers.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-15.0f, -15.0f}, {15.0f, 15.0f}}},
	    .objects = {{.type = VillagerInfo::CelticLeaderMale, .offset = {0.0f, 0.0f}},
	                {.type = VillagerInfo::CelticHousewifeFemale, .offset = {6.0f, 0.0f}},
	                {.type = VillagerInfo::CelticFarmerMale, .offset = {-6.0f, 0.0f}},
	                {.type = VillagerInfo::CelticHousewifeFemale, .offset = {0.0f, 6.0f}},
	                {.type = VillagerInfo::CelticForesterMale, .offset = {0.0f, -6.0f}},
	                {.type = VillagerInfo::CelticHousewifeFemale, .offset = {4.0f, 4.0f}},
	                {.type = VillagerInfo::CelticFishermanMale, .offset = {-4.0f, -4.0f}}},
	    .commands = {{.kind = Kind::MakeDance,
	                  .delaySeconds = 1.0f,
	                  .value = static_cast<size_t>(DanceInfo::DanceAroundPerson),
	                  .object = 0,
	                  .ctrl = true}},
	});

	all.push_back({
	    .id = "script.set_target_throw",
	    .name = "A script throws a villager to get to a place in a time",
	    .facet = Facet::Story,
	    .description = "A villager stands in the middle. After one second, SET_TARGET throws the villager to a point "
	                   "20 metres east and 10 metres north, 5 metres above the land, in 3 seconds. The magic mushroom "
	                   "challenge of Land 1 throws its actor at the camera in this way.",
	    .expected = "The villager tumbles up in an arc with no air drag and gets to the point after 3 seconds. Then it "
	                "falls to the land. The log tells that SET_TARGET ran and gave back no values.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-10.0f, -10.0f}, {30.0f, 20.0f}}},
	    .objects = {{.type = VillagerInfo::CelticHousewifeFemale, .offset = {0.0f, 0.0f}}},
	    .commands = {{.kind = Kind::CallNative,
	                  .delaySeconds = 1.0f,
	                  .script = "SET_TARGET",
	                  .scriptArguments = {{.kind = Argument::Kind::Object, .index = 0},
	                                      {.kind = Argument::Kind::Position, .point = {20.0f, 10.0f}, .height = 5.0f},
	                                      {.kind = Argument::Kind::Number, .number = 3.0f}}}},
	});

	// A script opens a window of the screen and closes it again, as the end of the fifth land does
	all.push_back({
	    .id = "script.clipping_window",
	    .name = "A script opens a window of the screen and closes it again",
	    .facet = Facet::Story,
	    .description = "After one second, SET_CLIPPING_WINDOW opens a window 40% wide and 60% high, 10% from the left "
	                   "and 20% from the top, in half a second. Four seconds later, CLEAR_CLIPPING_WINDOW closes it in "
	                   "half a second. The waving villagers at the end of the fifth land are shown in this window.",
	    .expected = "Four black boxes fade in around the window in half a second and cover all of the screen out of it. "
	                "The world shows in the window. After four seconds the boxes fade out in half a second.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-20.0f, -20.0f}, {20.0f, 20.0f}}},
	    .commands = {{.kind = Kind::CallNative,
	                  .delaySeconds = 1.0f,
	                  .script = "SET_CLIPPING_WINDOW",
	                  .scriptArguments = {{.kind = Argument::Kind::Number, .number = 0.1f},
	                                      {.kind = Argument::Kind::Number, .number = 0.2f},
	                                      {.kind = Argument::Kind::Number, .number = 0.4f},
	                                      {.kind = Argument::Kind::Number, .number = 0.6f},
	                                      {.kind = Argument::Kind::Number, .number = 0.5f}}},
	                 {.kind = Kind::CallNative,
	                  .delaySeconds = 4.0f,
	                  .script = "CLEAR_CLIPPING_WINDOW",
	                  .scriptArguments = {{.kind = Argument::Kind::Number, .number = 0.5f}}}},
	});

	// A script gives a clip to a flock of villagers, as Idol Pyre does to its extra worshippers
	all.push_back({
	    .id = "script.flock_plays_clip",
	    .name = "A script gives a flock of villagers a clip to play",
	    .facet = Facet::Story,
	    .description = "Three villagers stand in a row. After one second, a script makes a flock of them. SET_SCRIPT_ULONG "
	                   "gives the flock the prayer clip once, and SET_SCRIPT_STATE tells the flock to play it. Idol Pyre "
	                   "on Land 2 gives its extra worshippers the clip in this way.",
	    .expected = "Each villager of the flock plays the prayer clip one time. The script then controls each villager. "
	                "The log tells that the two natives ran and gave back no values.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 4.0f, -12.0f}, .look = {0.0f, 1.0f, 0.0f}},
	    .objects = {{.type = VillagerInfo::CelticHousewifeFemale, .offset = {-4.0f, 0.0f}},
	                {.type = VillagerInfo::CelticFarmerMale, .offset = {0.0f, 0.0f}},
	                {.type = VillagerInfo::CelticHousewifeFemale, .offset = {4.0f, 0.0f}}},
	    .commands = {{.kind = Kind::CallNative,
	                  .delaySeconds = 1.0f,
	                  .script = "SET_SCRIPT_ULONG",
	                  .scriptArguments = {{.kind = Argument::Kind::Flock},
	                                      {.kind = Argument::Kind::Integer, .number = 343.0f},
	                                      {.kind = Argument::Kind::Integer, .number = 1.0f}}},
	                 {.kind = Kind::CallNative,
	                  .delaySeconds = 1.0f,
	                  .script = "SET_SCRIPT_STATE",
	                  .scriptArguments = {{.kind = Argument::Kind::Flock},
	                                      {.kind = Argument::Kind::Integer, .number = 200.0f}}}},
	});

	// A script's camera follows a flock of villagers and looks at it, as at the place of its leader
	all.push_back({
	    .id = "script.camera_follow_flock",
	    .name = "A script's camera follows a flock of villagers",
	    .facet = Facet::Story,
	    .description = "Three villagers stand in a row. After one second, a script makes a flock of them and takes the "
	                   "camera. SET_POSITION_FOLLOW and SET_FOCUS_FOLLOW give the camera the flock. After three seconds, "
	                   "MOVE_GAME_THING sends each villager 30 to the north.",
	    .expected = "The camera goes to the leader of the flock at once and looks down at the middle of its body from "
	                "about 2 away. It then follows the leader as the villagers walk. The log tells that the natives ran.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 12.0f, -24.0f}, .look = {0.0f, 1.0f, 0.0f}},
	    .objects = {{.type = VillagerInfo::CelticHousewifeFemale, .offset = {-4.0f, 0.0f}},
	                {.type = VillagerInfo::CelticFarmerMale, .offset = {0.0f, 0.0f}},
	                {.type = VillagerInfo::CelticHousewifeFemale, .offset = {4.0f, 0.0f}}},
	    .commands = {{.kind = Kind::CallNative, .delaySeconds = 1.0f, .script = "START_CAMERA_CONTROL"},
	                 {.kind = Kind::CallNative,
	                  .delaySeconds = 0.0f,
	                  .script = "SET_POSITION_FOLLOW",
	                  .scriptArguments = {{.kind = Argument::Kind::Flock}}},
	                 {.kind = Kind::CallNative,
	                  .delaySeconds = 0.0f,
	                  .script = "SET_FOCUS_FOLLOW",
	                  .scriptArguments = {{.kind = Argument::Kind::Flock}}},
	                 {.kind = Kind::CallNative,
	                  .delaySeconds = 3.0f,
	                  .script = "MOVE_GAME_THING",
	                  .scriptArguments = {{.kind = Argument::Kind::Object, .index = 0, .kept = true},
	                                      {.kind = Argument::Kind::Position, .point = {-4.0f, 30.0f}},
	                                      {.kind = Argument::Kind::Number, .number = 0.0f}}},
	                 {.kind = Kind::CallNative,
	                  .delaySeconds = 0.0f,
	                  .script = "MOVE_GAME_THING",
	                  .scriptArguments = {{.kind = Argument::Kind::Object, .index = 1, .kept = true},
	                                      {.kind = Argument::Kind::Position, .point = {0.0f, 30.0f}},
	                                      {.kind = Argument::Kind::Number, .number = 0.0f}}},
	                 {.kind = Kind::CallNative,
	                  .delaySeconds = 0.0f,
	                  .script = "MOVE_GAME_THING",
	                  .scriptArguments = {{.kind = Argument::Kind::Object, .index = 2, .kept = true},
	                                      {.kind = Argument::Kind::Position, .point = {4.0f, 30.0f}},
	                                      {.kind = Argument::Kind::Number, .number = 0.0f}}}},
	});

	// The two-thing camera of a script keeps a flock of villagers and a tree in view. It reads the flock at the own place
	// of the flock, not at its leader.
	all.push_back({
	    .id = "script.camera_dual_flock",
	    .name = "The two-thing camera of a script on a flock and a tree",
	    .facet = Facet::Story,
	    .description = "Three villagers stand in a row, and an oak stands 30 to the north. After one second, a script "
	                   "makes a flock of the villagers at the first villager and takes the camera. START_DUAL_CAMERA "
	                   "gives the camera the flock and the oak. After four seconds, MOVE_GAME_THING sends each villager "
	                   "20 to the east.",
	    .expected = "The camera glides in two seconds to the middle between the place of the flock and the oak. It looks "
	                "from the side of the line between them, from about 83 away. The flock counts as 30 wide and has no "
	                "height. The camera does not move when the villagers walk to the east, because the place of the flock "
	                "does not change.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 12.0f, -24.0f}, .look = {0.0f, 1.0f, 0.0f}},
	    .objects = {{.type = VillagerInfo::CelticHousewifeFemale, .offset = {-4.0f, 0.0f}},
	                {.type = VillagerInfo::CelticFarmerMale, .offset = {0.0f, 0.0f}},
	                {.type = VillagerInfo::CelticHousewifeFemale, .offset = {4.0f, 0.0f}},
	                {.type = TreeInfo::Oak, .offset = {0.0f, 30.0f}}},
	    .commands = {{.kind = Kind::CallNative, .delaySeconds = 1.0f, .script = "START_CAMERA_CONTROL"},
	                 {.kind = Kind::CallNative,
	                  .delaySeconds = 0.0f,
	                  .script = "START_DUAL_CAMERA",
	                  .scriptArguments = {{.kind = Argument::Kind::Flock}, {.kind = Argument::Kind::Object, .index = 3}}},
	                 {.kind = Kind::CallNative,
	                  .delaySeconds = 4.0f,
	                  .script = "MOVE_GAME_THING",
	                  .scriptArguments = {{.kind = Argument::Kind::Object, .index = 0, .kept = true},
	                                      {.kind = Argument::Kind::Position, .point = {16.0f, 0.0f}},
	                                      {.kind = Argument::Kind::Number, .number = 0.0f}}},
	                 {.kind = Kind::CallNative,
	                  .delaySeconds = 0.0f,
	                  .script = "MOVE_GAME_THING",
	                  .scriptArguments = {{.kind = Argument::Kind::Object, .index = 1, .kept = true},
	                                      {.kind = Argument::Kind::Position, .point = {20.0f, 0.0f}},
	                                      {.kind = Argument::Kind::Number, .number = 0.0f}}},
	                 {.kind = Kind::CallNative,
	                  .delaySeconds = 0.0f,
	                  .script = "MOVE_GAME_THING",
	                  .scriptArguments = {{.kind = Argument::Kind::Object, .index = 2, .kept = true},
	                                      {.kind = Argument::Kind::Position, .point = {24.0f, 0.0f}},
	                                      {.kind = Argument::Kind::Number, .number = 0.0f}}}},
	});

	// The leans of the three dead trees of Land 1, in degrees about x and z, and one tree with a large lean
	all.push_back({
	    .id = "nature.dead_tree_lean",
	    .name = "Dead trees from a land script lean about x and z",
	    .facet = Facet::Nature,
	    .description = "Four dead trees in a row from the west. The first three have the kinds and the leans of the three "
	                   "dead trees on Land 1. The last tree leans 20 degrees about x and 10 degrees about z, so that the "
	                   "direction of the lean is clear.",
	    .expected = "The first three trees lean by a few degrees, each in its own direction. The last tree leans "
	                "clearly. The turn about the vertical comes first, then the lean about x, then the lean about z.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-25.0f, -8.0f}, {25.0f, 8.0f}}},
	    .objects = {{.type = TreeInfo::Pine, .offset = {-18.0f, 0.0f}, .dead = true, .leanDegrees = {-0.4544f, -3.1648f}},
	                {.type = TreeInfo::ConiferA, .offset = {-6.0f, 0.0f}, .dead = true, .leanDegrees = {2.2700f, 0.0694f}},
	                {.type = TreeInfo::ConiferA, .offset = {6.0f, 0.0f}, .dead = true, .leanDegrees = {-0.4841f, 1.8931f}},
	                {.type = TreeInfo::ConiferA, .offset = {18.0f, 0.0f}, .dead = true, .leanDegrees = {20.0f, 10.0f}}},
	});

	// Bonfires from a land script: the turn is the third number, and the second number is not used
	all.push_back({
	    .id = "nature.bonfire_land_script_turn",
	    .name = "Bonfires from a land script take their turn from the third number",
	    .facet = Facet::Nature,
	    .description = "Two bonfires in a row from the west, made by land script lines. The first has the numbers of the "
	                   "two bonfires on Land 1: 24.7, then 0. The second has 0, then a quarter turn.",
	    .expected = "The first bonfire stands with no turn, as on Land 1, because the second number is not used. The "
	                "second bonfire is turned by a quarter turn about the vertical.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-12.0f, -6.0f}, {12.0f, 6.0f}}},
	    .landScript = "CREATE_BONFIRE(\"-6.0,0.0\", 24.700001, 0.000000, 1.000000)\n"
	                  "CREATE_BONFIRE(\"6.0,0.0\", 0.000000, 1.570796, 1.000000)\n",
	});

	// Mobile statics that a land script turns and leans: the lean follows the way each one faces
	all.push_back({
	    .id = "nature.mobile_static_lean",
	    .name = "Mobile statics from a land script lean and turn in the game's order",
	    .facet = Facet::Nature,
	    .description = "Four mobile statics in a row from the west, each turned 45 degrees about the vertical. The first "
	                   "fence leans 30 degrees about x. The second fence leans 30 degrees about z. The long rock and the "
	                   "totem lean about x and z.",
	    .expected = "Each thing leans in the direction that it faces after its turn, not along the axes of the map. The "
	                "first fence tips forward along its facing. The second fence tips to its side.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-25.0f, -8.0f}, {25.0f, 8.0f}}},
	    .objects = {{.type = MobileStaticInfo::CeltFenceTall,
	                 .offset = {-18.0f, 0.0f},
	                 .yawDegrees = 45.0f,
	                 .leanDegrees = {30.0f, 0.0f}},
	                {.type = MobileStaticInfo::CeltFenceTall,
	                 .offset = {-6.0f, 0.0f},
	                 .yawDegrees = 45.0f,
	                 .leanDegrees = {0.0f, 30.0f}},
	                {.type = MobileStaticInfo::LongrockSandstone,
	                 .offset = {6.0f, 0.0f},
	                 .yawDegrees = 45.0f,
	                 .leanDegrees = {20.0f, 20.0f}},
	                {.type = MobileStaticInfo::GateTotemApe,
	                 .offset = {18.0f, 0.0f},
	                 .yawDegrees = 45.0f,
	                 .leanDegrees = {25.0f, -15.0f}}},
	});

	// Two rows of small pines along the path of a low throw. The camera stands as for the hand's throw.
	std::vector<ObjectSetup> bendTrees {{.type = MobileStaticInfo::Rock, .offset = {0.0f, 0.0f}}};
	for (int row = 0; row < 10; ++row)
	{
		const float along = 6.0f + static_cast<float>(row) * 3.0f;
		bendTrees.push_back({.type = TreeInfo::Pine, .offset = {along, -2.0f}, .scale = 0.5f});
		bendTrees.push_back({.type = TreeInfo::Pine, .offset = {along, 2.0f}, .scale = 0.5f});
	}
	all.push_back({
	    .id = "nature.trees_bend_round_a_thrown_rock",
	    .name = "Trees bend away from a rock that flies between them",
	    .facet = Facet::Nature,
	    .description = "The hand takes a rock and throws it low between two rows of small pines. Then the rock comes to "
	                   "rest.",
	    .expected = "While the rock flies, the pines near it lean away from it, more when the rock is near. A pine that "
	                "the rock passes stands straight again when the rock is out of its reach. When the rock comes to "
	                "rest among the pines, the pines in its reach stay bent away from it.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {-34.5f, 9.0f, 4.5f}, .look = {0.0f, 0.0f, 0.0f}},
	    .objects = bendTrees,
	    .commands =
	        {// Takes the rock under the middle of the screen
	         {.kind = Kind::PointerTo, .delaySeconds = 1.0f, .point = {0.5f, 0.5f}},
	         {.kind = Kind::PointerPress, .delaySeconds = 0.5f, .value = 3},
	         {.kind = Kind::PointerRelease, .delaySeconds = 0.5f, .value = 3},
	         // Makes ready low on the screen and sweeps a little up, so that the throw stays low
	         {.kind = Kind::PointerTo, .delaySeconds = 0.5f, .point = {0.45f, 0.7f}},
	         {.kind = Kind::PointerPress, .delaySeconds = 0.5f, .value = 3},
	         {.kind = Kind::PointerSweep, .delaySeconds = 0.3f, .point = {0.03f, -0.15f}, .amount = 0.3f},
	         {.kind = Kind::PointerRelease, .delaySeconds = 0.5f, .value = 3}},
	});

	// Two rows of pines with a lane between them. The player's creature walks along the lane and back.
	std::vector<ObjectSetup> lanePines;
	for (int row = 0; row < 10; ++row)
	{
		const float along = static_cast<float>(row) * 3.0f;
		lanePines.push_back({.type = TreeInfo::Pine, .offset = {along, -4.5f}});
		lanePines.push_back({.type = TreeInfo::Pine, .offset = {along, 4.5f}});
	}
	all.push_back({
	    .id = "nature.trees_bend_round_the_creature",
	    .name = "Trees bend away from the player's creature",
	    .facet = Facet::Nature,
	    .description = "The player's tiger walks along a lane between two rows of pines, then walks back.",
	    .expected = "The pines near the tiger lean away from it, more when the tiger is near, and crash about. When the "
	                "tiger has gone by, they stand straight again.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-12.0f, -12.0f}, {40.0f, 12.0f}}},
	    .creatures = {CreatureSetup {
	        .label = "yours", .species = CreatureType::Tiger, .offset = {-10.0f, 0.0f}, .owner = PlayerNames::PLAYER_ONE}},
	    .objects = lanePines,
	    .commands = {{.kind = Kind::WalkTo, .creature = 0, .delaySeconds = 1.0f, .point = {40.0f, 0.0f}},
	                 {.kind = Kind::WalkTo, .creature = 0, .delaySeconds = 8.0f, .point = {-10.0f, 0.0f}}},
	});
}

void AddVillagerSizeAndReachScenarios(std::vector<Scenario>& all)
{
	// Children of each age from 1 to 12 in a row, then a grown up, all held still by a script
	std::vector<ObjectSetup> ages;
	constexpr uint32_t k_OldestChild = 12;
	for (uint32_t age = 1; age <= k_OldestChild + 1; ++age)
	{
		ages.push_back({.type = VillagerInfo::CelticHousewifeFemale,
		                .offset = {static_cast<float>(age) * 4.0f - 28.0f, 0.0f},
		                .villagerAge = age <= k_OldestChild ? age : 30u,
		                .inScript = true});
	}
	all.push_back({
	    .id = "growth.villager_size_by_age",
	    .name = "Villagers of each age side by side",
	    .facet = Facet::Growth,
	    .description = "Twelve children of the ages 1 to 12 and a grown up stand in a row, from the west, held still by a "
	                   "script.",
	    .expected = "Each child is a little larger than the one to its west. A child starts at the size of the year before "
	                "its age and grows towards the size of the year after it. The grown up is between 0.95 and 1.05 of "
	                "full size.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-30.0f, -6.0f}, {30.0f, 6.0f}}},
	    .objects = ages,
	});

	// Villagers of the player's town walking there and back
	std::vector<ObjectSetup> walkers;
	for (int i = 0; i < 3; ++i)
	{
		const float y = static_cast<float>(i) * 6.0f - 6.0f;
		walkers.push_back({.type = VillagerInfo::CelticHousewifeFemale,
		                   .offset = {-20.0f, y},
		                   .walkTo = glm::vec2(20.0f, y),
		                   .walkAfterSeconds = 1.0f,
		                   .walkRepeatSeconds = 20.0f,
		                   .joinTown = true});
	}
	all.push_back({
	    .id = "movement.villager_speed_town_belief",
	    .name = "The walk speed of villagers in a player's town",
	    .facet = Facet::Movement,
	    .description = "Three villagers of the player's town walk 40 metres east and back every twenty seconds. Their speed "
	                   "comes from the player's most Indian tribal power and from the town's belief in the player against "
	                   "the town's own neutral belief.",
	    .expected = "The villagers walk at the speed of their state, scaled by the town's belief in the player beyond the "
	                "town's own neutral belief. The inspector shows the speed of each walker's WallHug.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-24.0f, -10.0f}, {24.0f, 10.0f}}},
	    .objects = walkers,
	});
	// A villager that sits at the end of each walk stands up before the next walk starts
	all.push_back({
	    .id = "movement.villager_stands_up_then_walks",
	    .name = "A villager stands up before it walks",
	    .facet = Facet::Movement,
	    .description = "A villager walks 20 metres east and sits down there. Every twenty seconds it walks to the other "
	                   "point and sits down again.",
	    .expected = "At the start of each walk, the villager plays its clip to stand up where it sits. It does not move "
	                "until the clip ends. Then it walks. The villager walks in its own turn, and it does not walk while "
	                "a clip into or out of a state plays.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {10.0f, 2.5f, -7.0f}, .look = {9.0f, 1.0f, 0.0f}},
	    .objects = {{.type = VillagerInfo::CelticHousewifeFemale,
	                 .offset = {-10.0f, 0.0f},
	                 .walkTo = glm::vec2(10.0f, 0.0f),
	                 .walkAfterSeconds = 1.0f,
	                 .walkRepeatSeconds = 20.0f,
	                 .walkFinal = VillagerStates::SitAndChillout}},
	});
	// Dancers that walk to their places in a dance, each step at its point in the dance's state
	all.push_back({
	    .id = "movement.dancers_step_in_their_dance",
	    .name = "Dancers take their steps in the dance",
	    .facet = Facet::Movement,
	    .description = "A leader stands in the middle with four villagers round him, 8 metres away. After one second, a "
	                   "script makes the dance about a person about the leader, and the four villagers join it.",
	    .expected = "Each dancer does the move of the dance first. In the same turn, it starts its walk to its place and "
	                "takes the first step. When the group moves, each dancer takes its step to the new place in the "
	                "turn that the group moves. A dancer on its place takes no step.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 14.0f, -18.0f}, .look = {0.0f, 0.0f, 0.0f}},
	    .objects = {{.type = VillagerInfo::CelticLeaderMale, .offset = {0.0f, 0.0f}},
	                {.type = VillagerInfo::CelticHousewifeFemale, .offset = {8.0f, 0.0f}},
	                {.type = VillagerInfo::CelticFarmerMale, .offset = {-8.0f, 0.0f}},
	                {.type = VillagerInfo::CelticHousewifeFemale, .offset = {0.0f, 8.0f}},
	                {.type = VillagerInfo::CelticForesterMale, .offset = {0.0f, -8.0f}}},
	    .commands = {{.kind = Kind::MakeDance,
	                  .delaySeconds = 1.0f,
	                  .value = static_cast<size_t>(DanceInfo::DanceAroundPerson),
	                  .object = 0}},
	});
	// A dancer that must go round a hut to get to its place, while the dance turns its place away
	all.push_back({
	    .id = "movement.dancer_leaves_circle_when_goal_moves",
	    .name = "A dancer round a hut leaves it when the dance moves its place",
	    .facet = Facet::Movement,
	    .description = "A leader stands in the middle. A Celtic hut stands 9 metres west of him, and a villager stands "
	                   "behind the hut, 16 metres west of the leader. After one second, a script makes the dance about "
	                   "a person about the leader, and the villager joins it.",
	    .expected = "The villager walks round the hut to its place. Each time the dance moves its place, it notes a new "
	                "distance at which it can leave the hut. It leaves the hut when it is nearer its place than at "
	                "the last move, and it gets to the dance. It does not go round the hut for ever.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-22.0f, -12.0f}, {12.0f, 12.0f}}},
	    .objects = {{.type = VillagerInfo::CelticLeaderMale, .offset = {0.0f, 0.0f}},
	                {.type = AbodeInfo::CelticHut, .offset = {-9.0f, 0.0f}},
	                {.type = VillagerInfo::CelticFarmerMale, .offset = {-16.0f, 0.0f}}},
	    .commands = {{.kind = Kind::MakeDance,
	                  .delaySeconds = 1.0f,
	                  .value = static_cast<size_t>(DanceInfo::DanceAroundPerson),
	                  .object = 0}},
	});
	// Lethys's worship site on the second land dances among the spokes of his temple
	all.push_back({
	    .id = "movement.lethys_dancers_leave_temple_spokes",
	    .name = "Lethys's worshippers do not go round a spoke of his temple for ever",
	    .facet = Facet::Movement,
	    .description = "The second land of the story, as the story plays it, with the camera on Lethys's worship site. "
	                   "The dance of the site stands next to the spokes of his temple, and the dance moves the places of "
	                   "its dancers. Best watched at a high game speed, or run with --sim-fast to turn 60000.",
	    .expected = "A worshipper that goes round a temple spoke to get to its place notes a new distance to leave the "
	                "spoke each time the dance moves its place. It leaves the spoke and dances again. No worshipper "
	                "of the site goes round a spoke for ever, and none dies of hunger because of it.",
	    .environment = {.land = Land::Story, .storyLand = "Land2.txt", .dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-1609.0f, 1086.0f}, {-1549.0f, 1146.0f}}},
	});
	// Two creatures, each with its own turn in the list of the living
	all.push_back({
	    .id = "mind.two_creatures_own_turns",
	    .name = "Two creatures each take their turn at their own place",
	    .facet = Facet::Mind,
	    .description = "A tiger and a cow stand 30 metres apart with nothing to do, and a villager walks between them. "
	                   "With the game's log at the debug level, each creature writes the synced random numbers of its "
	                   "turn.",
	    .expected = "Each creature thinks, walks and acts as it does alone. In the log, the numbers that each creature "
	                "draws come together in its own turn, the newer creature first. No numbers of the other creature come "
	                "between them.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview},
	    .creatures = {Content(CreatureType::Tiger, {-15.0f, 0.0f}, 90.0f, "older"),
	                  Content(CreatureType::Cow, {15.0f, 0.0f}, 270.0f, "newer")},
	    .objects = {{.type = VillagerInfo::CelticHousewifeFemale,
	                 .offset = {0.0f, -10.0f},
	                 .walkTo = glm::vec2(0.0f, 10.0f),
	                 .walkAfterSeconds = 1.0f,
	                 .walkRepeatSeconds = 20.0f}},
	});

	// The tutorial's interface levels, as a lesson's script steps through them
	std::vector<Command> levels;
	for (const size_t level : {1, 2, 1, 5, 1, 11, 1, 15, 8})
	{
		levels.push_back({.kind = Kind::SetInterfaceLevel, .delaySeconds = 5.0f, .value = level});
	}
	all.push_back({
	    .id = "tutorial.interface_level_reach",
	    .name = "The hand's reach at each interface level",
	    .facet = Facet::Tutorial,
	    .description = "The interface level changes every five seconds, as a lesson's script changes it: level 1, then 2, 5, "
	                   "11 and 15, each after level 1 again, and at the end level 8. The scenario log gives the hand's reach "
	                   "at each level.",
	    .expected = "Level 1 gives the hand a short reach of 75 metres. Levels 2, 5, 11 and 15 give the full reach of "
	                "1800 metres. Level 8 keeps the reach that level 15 gave.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Testbed},
	    .commands = levels,
	});
	// A lesson's interface level limits the Action button, and the normal level removes the limits
	all.push_back({
	    .id = "tutorial.interface_level_hand_limits",
	    .name = "The limits of the Action button at a lesson's interface level",
	    .facet = Facet::Tutorial,
	    .description = "The interface level becomes 1, the level of the opening's lesson. After 40 seconds it becomes "
	                   "the normal level 0. A villager and a scroll stand in front of the camera.",
	    .expected = "At level 1 the Action button does not pick up the villager, and a press on the land does nothing. "
	                "A click on the scroll still reaches the scripts. At level 0 the hand picks up the villager again.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 12.0f, -20.0f}, .look = {0.0f, 2.0f, 6.0f}},
	    .objects = {{.type = VillagerInfo::CelticFarmerMale, .offset = {-4.0f, 6.0f}}},
	    .highlights = {{.kind = 1, .offset = {4.0f, 6.0f}, .challenge = 0}},
	    .commands = {{.kind = Kind::SetInterfaceLevel, .delaySeconds = 0.5f, .value = 1},
	                 {.kind = Kind::SetInterfaceLevel, .delaySeconds = 40.0f, .value = 0}},
	});
	all.push_back({
	    .id = "tutorial.bookmark_flight",
	    .name = "A long flight to a bookmark",
	    .facet = Facet::Tutorial,
	    .description = "A bookmark is put on the land 900 metres from the middle of the map, and the camera flies to it, as "
	                   "the number key 1 does. Five seconds later a second bookmark is put back at the middle, and the "
	                   "camera flies back to it.",
	    .expected = "Each flight goes by a high point half way. The high point is 0.3 metres higher for each metre that the "
	                "flight goes across, so these flights go up by about 270 metres. The camera makes a woosh at the start "
	                "of each flight.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Testbed},
	    .commands = {{.kind = Kind::FlyToBookmark, .delaySeconds = 2.0f, .point = {900.0f, 0.0f}, .value = 0},
	                 {.kind = Kind::FlyToBookmark, .delaySeconds = 5.0f, .point = {0.0f, 0.0f}, .value = 1}},
	});
	all.push_back({
	    .id = "tutorial.tab_town_flights",
	    .name = "The Tab key flies to the player's towns",
	    .facet = Facet::Tutorial,
	    .description = "Lines of a land script make three towns of the player in this order: a town with a storage pit to "
	                   "the west, a town with a village centre to the north, and a town with no building to the east. A "
	                   "neutral town with a storage pit stands to the south. The player presses the Tab key four times, "
	                   "eight seconds apart.",
	    .expected = "The first press flies the camera to the village centre to the north. The second press flies it to the "
	                "storage pit to the west. The town to the east has no place to fly to, so the third press flies to "
	                "the north again, and the fourth to the west. The camera never goes to the neutral town. Each flight "
	                "ends 24 metres west, 28 metres up and 47 metres north of the place, looking at it.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview},
	    .landScript = R"(CREATE_TOWN(60, "-70.0,0.0", "PLAYER_ONE", 0, "CELTIC")
SET_TOWN_BELIEF(60, "PLAYER_ONE", 10.0)
CREATE_ABODE(60, "-70.0,10.0", "CELTIC_ABODE_STORAGE_PIT", 0, 1000, 100, 100)
CREATE_TOWN(61, "0.0,80.0", "PLAYER_ONE", 0, "NORSE")
SET_TOWN_BELIEF(61, "PLAYER_ONE", 10.0)
CREATE_TOWN_CENTRE(61, "0.0,90.0", "NORSE_ABODE_TOWN_CENTRE", 0, 990, 0)
CREATE_TOWN(62, "70.0,0.0", "PLAYER_ONE", 0, "CELTIC")
SET_TOWN_BELIEF(62, "PLAYER_ONE", 10.0)
CREATE_TOWN(63, "0.0,-80.0", "NEUTRAL", 0, "CELTIC")
CREATE_ABODE(63, "0.0,-70.0", "CELTIC_ABODE_STORAGE_PIT", 0, 1000, 100, 100)
)",
	    .commands = {{.kind = Kind::TabToTown, .delaySeconds = 3.0f},
	                 {.kind = Kind::TabToTown, .delaySeconds = 8.0f},
	                 {.kind = Kind::TabToTown, .delaySeconds = 8.0f},
	                 {.kind = Kind::TabToTown, .delaySeconds = 8.0f}},
	});
	all.push_back({
	    .id = "tutorial.bookmark_markers_follow",
	    .name = "Bookmark markers and a followed creature",
	    .facet = Facet::Tutorial,
	    .description = "Three bookmarks go on the land near the middle of the map, as Ctrl and the keys 1, 2 and 3 do. Then "
	                   "the player keeps the tiger in bookmark 4, as Ctrl and the key 4 do with the hand over it, and the "
	                   "camera follows it, as Shift and the key 4 do. The tiger walks away to the north.",
	    .expected = "Each bookmark shows its yellow number in a ring, two metres above its place. The marker of bookmark 4 "
	                "stays two metres above the tiger and moves with it. All the markers throb together: the width and "
	                "the height each swing once a second, the height a fifth of a second behind. The camera flies after "
	                "the tiger each turn, until the player moves the camera.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview},
	    .creatures = {Posed(CreatureType::Tiger, {0.0f, -10.0f}, 0.0f)},
	    .commands = {{.kind = Kind::FlyToBookmark, .delaySeconds = 0.5f, .point = {25.0f, 0.0f}, .value = 0},
	                 {.kind = Kind::FlyToBookmark, .delaySeconds = 0.5f, .point = {-25.0f, 0.0f}, .value = 1},
	                 {.kind = Kind::FlyToBookmark, .delaySeconds = 0.5f, .point = {0.0f, 25.0f}, .value = 2},
	                 {.kind = Kind::BookmarkCreature, .creature = 0, .delaySeconds = 1.0f, .value = 3},
	                 Go(Kind::WalkTo, 0, {0.0f, 160.0f}, 0.5f, false)},
	});
	{
		// The first land's turning lesson, as the game's own script runs it after the turning demonstration. The lesson
		// keeps the camera near the temple, and the temple's place is where the camera goes back to.
		using Argument = ScriptArgument;
		constexpr glm::vec2 k_LessonTemple {0.0f, 40.0f};
		constexpr glm::vec2 k_ViewPlace {0.0f, 0.0f};
		all.push_back({
		    .id = "tutorial.teach_rotate",
		    .name = "The turning lesson and the hand's turning state",
		    .facet = Facet::Tutorial,
		    .description = "The player's temple stands north of the middle. Half a second in, the game's TeachRotate "
		                   "script runs, as the first land's lesson starts it. Hold the land with both buttons and move "
		                   "the mouse across to turn the camera, or turn it with the keys while both buttons hold the land.",
		    .expected = "The advisors tell the player to turn the camera. While both buttons hold the land and the camera "
		                "turns, the hand's state that the scripts read is the turning state (21). When the lesson's minute "
		                "runs out while the player turns the land in this way, the turning demonstration does not play "
		                "again. With no turn, it plays again after each minute. Turning enough ends the lesson.",
		    .environment = {.dispenserGrid = false},
		    .framing = {.shot = Shot::Placed,
		                .eye = {k_ViewPlace.x, 30.0f, k_ViewPlace.y - 40.0f},
		                .look = {k_LessonTemple.x, 0.0f, k_LessonTemple.y}},
		    .temples = {{.offset = k_LessonTemple}},
		    .commands = {{.kind = Kind::RunScript,
		                  .delaySeconds = 0.5f,
		                  .script = "TeachRotate",
		                  .scriptArguments = {{.kind = Argument::Kind::Temple, .index = 0},
		                                      {.kind = Argument::Kind::Marker, .point = k_ViewPlace, .height = 30.0f}}}},
		});
	}
}

/// A blow that sways the creature, and a wolf that frightens it
void AddSwayAndFrightScenarios(std::vector<Scenario>& all)
{
	all.push_back({
	    .id = "objects.sway_from_a_thrown_rock",
	    .name = "A thrown rock sways the creature",
	    .facet = Facet::Objects,
	    .description = "A tiger stands still, and a rock flies at its legs from the west every five seconds.",
	    .expected = "Each rock that hits the tiger sways its body. The body swings back on its spring once a game "
	                "turn and settles. A body that turns on the spot or sits does not sway.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-25.0f, 0.0f}, {10.0f, 0.0f}}},
	    .creatures = {Posed(CreatureType::Tiger, {0.0f, 0.0f}, 90.0f)},
	    .objects = {{.type = MobileStaticInfo::Rock, .offset = {-25.0f, 0.0f}}},
	    .throws =
	        {{.object = 0, .height = 14.0f, .velocity = {14.0f, 4.0f, 0.0f}, .delaySeconds = 2.0f, .repeatSeconds = 5.0f}},
	});

	// Rocks that a script makes go into the physics
	const auto create = [](float delay, int32_t type, glm::vec2 point, float height) {
		return Command {
		    .kind = Kind::CallNative,
		    .delaySeconds = delay,
		    .script = "CREATE",
		    .scriptArguments = {{.kind = ScriptArgument::Kind::Integer, .number = static_cast<float>(type)},
		                        {.kind = ScriptArgument::Kind::Integer, .number = static_cast<float>(MobileStaticInfo::Rock)},
		                        {.kind = ScriptArgument::Kind::Position, .point = point, .height = height}}};
	};
	all.push_back({
	    .id = "objects.script_rock_falls",
	    .name = "A rock that a script makes falls onto the land",
	    .facet = Facet::Objects,
	    .description = "After one second, CREATE makes a rock as a mobile static 10 metres above the land, 10 metres "
	                   "west of the middle. Two seconds later, CREATE makes a rock by the rock type at the same height, "
	                   "10 metres east of the middle.",
	    .expected = "The first rock falls from the height that the script gave and comes to rest on the land. The "
	                "second rock shows on the land at once, turned to the slope, and stays there.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-15.0f, 0.0f}, {15.0f, 0.0f}}},
	    .commands = {create(1.0f, 8, {-10.0f, 0.0f}, 10.0f), create(2.0f, 33, {10.0f, 0.0f}, 10.0f)},
	});

	all.push_back({
	    .id = "senses.creature_runs_from_a_wolf",
	    .name = "A wolf frightens the creature",
	    .facet = Facet::Senses,
	    .description = "A tiger with its fear as strong as it gets, and a wolf fifteen metres east of it.",
	    .expected = "The tiger runs away from the wolf. Tigers, leopards, wolves and the wolves of a spell frighten a "
	                "creature as lions do.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-30.0f, 0.0f}, {20.0f, 0.0f}}},
	    .creatures = {CreatureSetup {
	        .species = CreatureType::Tiger,
	        .offset = {0.0f, 0.0f},
	        .needs = Content(),
	        .desires = {{.desire = Desire::Fear, .fraction = 1.0f}},
	    }},
	    .objects = {{.type = AnimalInfo::Wolf, .offset = {15.0f, 0.0f}, .living = true}},
	});
}

void AddStoreScenarios(std::vector<Scenario>& all)
{
	std::vector<ObjectSetup> objects {
	    {.type = AbodeInfo::CelticTownCentre, .offset = {0.0f, 50.0f}},
	    {.type = AbodeInfo::CelticStoragePit, .offset = {-30.0f, 30.0f}, .life = 0.1f, .storedFood = 2000, .storedWood = 1000},
	};
	for (size_t i = 0; i < 3; ++i)
	{
		const float x = (static_cast<float>(i) - 1.5f) * 3.0f;
		objects.push_back({.type = VillagerInfo::CelticForesterMale, .offset = {x, 20.0f}, .joinTown = true});
		objects.push_back({.type = VillagerInfo::CelticHousewifeFemale, .offset = {x, 17.0f}, .joinTown = true});
	}
	all.push_back({
	    .id = "objects.store_stops_working",
	    .name = "A broken storage pit calls people to its piles",
	    .facet = Facet::Objects,
	    .description = "A Celtic town of the player's with a village centre and a storage pit with food and wood. The "
	                   "storage pit has damage down to a tenth of its life. Six villagers of the town stand near it.",
	    .expected = "The storage pit stops working. Its food pile and each wood pile with wood in it call the people "
	                "near them, as a pile on its own does (the inspector shows the reactions of the piles). A repair "
	                "of the pit above its limit stops the calls. A pile that becomes empty stops its call and keeps its "
	                "place.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-30.0f, 30.0f}, {0.0f, 50.0f}, {0.0f, 17.0f}}},
	    .objects = std::move(objects),
	});

	// A toadstool, as Land 1's script puts nine on the land, let go above a storage pit
	constexpr glm::vec2 k_Pit {-30.0f, 30.0f};
	all.push_back({
	    .id = "objects.toadstool_poisons_store",
	    .name = "A toadstool poisons the food of a storage pit",
	    .facet = Facet::Objects,
	    .description = "A Celtic town of the player's with a village centre and a storage pit with food and wood, and "
	                   "a toadstool beside them. After one second the toadstool is lifted and let go above the "
	                   "storage pit, as the hand drops one there.",
	    .expected = "The storage pit takes the toadstool as 150 food. Its food pile becomes poisoned and shows a pale "
	                "green tint. Its wood piles keep their usual colour. Villagers who take food or wood from the "
	                "storage pit become poisoned.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {k_Pit - glm::vec2 {12.0f}, k_Pit + glm::vec2 {12.0f}}},
	    .objects = {{.type = AbodeInfo::CelticTownCentre, .offset = {0.0f, 50.0f}},
	                {.type = AbodeInfo::CelticStoragePit, .offset = k_Pit, .storedFood = 2000, .storedWood = 1000},
	                {.type = MobileObjectInfo::Toadstool, .offset = k_Pit + glm::vec2 {15.0f, -15.0f}}},
	    .throws = {{.object = 2, .from = k_Pit, .height = 6.0f, .velocity = {0.0f, -1.0f, 0.0f}, .delaySeconds = 1.0f}},
	});
}

void AddPoisonScenarios(std::vector<Scenario>& all)
{
	// What the Plague's script does first: it enables the poison of the town and then of its storage pit, and later cures
	// the town. The scenario runs those calls.
	constexpr glm::vec2 k_Pit {-30.0f, 30.0f};
	using Argument = ScriptArgument;
	const auto poison = [](float delay, Argument thing, int flag) {
		return Command {.kind = Kind::CallNative,
		                .delaySeconds = delay,
		                .script = "SET_POISONED",
		                .scriptArguments = {{.kind = Argument::Kind::Integer, .number = static_cast<float>(flag)}, thing}};
	};
	all.push_back({
	    .id = "objects.script_poisons_town_and_store",
	    .name = "A script poisons a town and its storage pit",
	    .facet = Facet::Objects,
	    .description = "A Celtic town of the player's with a village centre, a storage pit with 2000 food and 1000 wood, "
	                   "and four villagers of the town. After one second a script poisons the town. After three seconds it "
	                   "poisons the storage pit. After ten seconds it cures the town.",
	    .expected = "At one second each of the four villagers is poisoned, as the inspector shows. At three seconds the "
	                "food pile of the storage pit is poisoned and shows a pale green tint. Its wood piles keep their usual "
	                "colour. At ten seconds the villagers are cured, but the food pile stays poisoned. Villagers who take "
	                "food from the pit become poisoned again.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {k_Pit - glm::vec2 {12.0f}, k_Pit + glm::vec2 {12.0f}}},
	    .objects = {{.type = AbodeInfo::CelticTownCentre, .offset = {0.0f, 50.0f}},
	                {.type = AbodeInfo::CelticStoragePit, .offset = k_Pit, .storedFood = 2000, .storedWood = 1000},
	                {.type = VillagerInfo::CelticFarmerMale, .offset = k_Pit + glm::vec2 {-6.0f, -8.0f}, .joinTown = true},
	                {.type = VillagerInfo::CelticFarmerMale, .offset = k_Pit + glm::vec2 {-3.0f, -9.0f}, .joinTown = true},
	                {.type = VillagerInfo::CelticHousewifeFemale, .offset = k_Pit + glm::vec2 {3.0f, -9.0f}, .joinTown = true},
	                {.type = VillagerInfo::CelticHousewifeFemale, .offset = k_Pit + glm::vec2 {6.0f, -8.0f}, .joinTown = true}},
	    .commands = {poison(1.0f, {.kind = Argument::Kind::Town}, 1),
	                 poison(2.0f, {.kind = Argument::Kind::Object, .index = 1}, 1),
	                 poison(7.0f, {.kind = Argument::Kind::Town}, 0)},
	});

	// The end of the Plague's script cures the storage pit and then the town. A script can cure the storage pit
	// (deviation 179).
	all.push_back({
	    .id = "objects.script_cures_store",
	    .name = "A script cures a storage pit",
	    .facet = Facet::Objects,
	    .description = "A Celtic town of the player's with a village centre and a storage pit with 2000 food and 1000 wood. "
	                   "After one second a script poisons the storage pit. After six seconds it cures the storage pit.",
	    .expected = "At one second the food pile of the storage pit is poisoned and shows a pale green tint. At six seconds "
	                "the food pile is cured, and the tint goes. The food stays in the pile.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {k_Pit - glm::vec2 {12.0f}, k_Pit + glm::vec2 {12.0f}}},
	    .objects = {{.type = AbodeInfo::CelticTownCentre, .offset = {0.0f, 50.0f}},
	                {.type = AbodeInfo::CelticStoragePit, .offset = k_Pit, .storedFood = 2000, .storedWood = 1000}},
	    .commands = {poison(1.0f, {.kind = Argument::Kind::Object, .index = 1}, 1),
	                 poison(5.0f, {.kind = Argument::Kind::Object, .index = 1}, 0)},
	});
}

void AddWorkshopScenarios(std::vector<Scenario>& all)
{
	constexpr glm::vec2 k_Workshop {-30.0f, 30.0f};
	all.push_back({
	    .id = "objects.workshop_makes_scaffolds",
	    .name = "A workshop makes scaffolds from its wood",
	    .facet = Facet::Objects,
	    .description = "A Celtic town of the player's with a village centre and a workshop that holds 10000 wood, the wood of "
	                   "four scaffolds. The yard of the workshop faces the camera.",
	    .expected = "A pile of logs stands at the back of the workshop and holds its wood. The workshop uses the wood of "
	                "one scaffold from the pile, and the pile becomes smaller. The work sound starts. When the time to make a "
	                "scaffold is over, a small scaffold appears in the first free place of the yard, a horn sounds and "
	                "the work sound stops. The workshop makes scaffolds until it has three in its yard. The hand can "
	                "take a scaffold from the yard, and the workshop then makes one more.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {k_Workshop - glm::vec2 {15.0f}, k_Workshop + glm::vec2 {15.0f}}},
	    .objects = {{.type = AbodeInfo::CelticTownCentre, .offset = {0.0f, 50.0f}},
	                {.type = AbodeInfo::CelticWorkshop, .offset = k_Workshop, .yawDegrees = 180.0f, .storedWood = 10000}},
	});

	all.push_back({
	    .id = "objects.workshop_graveyard_burial_scaffold",
	    .name = "A burial on a workshop that stands in for the graveyard makes a scaffold with no wood",
	    .facet = Facet::Objects,
	    .description = "A Celtic town of the player's with a village centre, a workshop with no wood, its yard towards the "
	                   "camera, and a graveyard. After one second the graveyard goes, and the workshop becomes the "
	                   "graveyard of the town. After two seconds a villager of the town with little life catches fire.",
	    .expected = "The villager burns and dies. The workshop counts the burial: its scaffold shows over the workshop "
	                "from then on. On the next turn of the workshop a small scaffold appears in the first place of the "
	                "yard and a horn sounds, although the workshop has no wood. The work sound does not play. The "
	                "inspector shows the stand-in graves count 1 on the workshop.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {k_Workshop - glm::vec2 {15.0f}, k_Workshop + glm::vec2 {15.0f}}},
	    .objects = {{.type = AbodeInfo::CelticTownCentre, .offset = {0.0f, 50.0f}},
	                {.type = AbodeInfo::CelticWorkshop, .offset = k_Workshop, .yawDegrees = 180.0f, .storedWood = 0},
	                {.type = AbodeInfo::CelticGraveyard, .offset = {30.0f, 30.0f}},
	                {.type = VillagerInfo::CelticFarmerMale,
	                 .offset = k_Workshop + glm::vec2 {12.0f, -12.0f},
	                 .joinTown = true,
	                 .life = 0.05f}},
	    .commands = {{.kind = Command::Kind::RemoveObject, .delaySeconds = 1.0f, .object = 2},
	                 {.kind = Command::Kind::SetAlight, .delaySeconds = 1.0f, .value = 1, .object = 3}},
	    .townBeliefInPlayer = 1.0f,
	});

	// The scenario turns the workshop so that its yard faces the camera. The first place of the Celtic workshop's yard is
	// 7.8 m in front of the workshop.
	constexpr glm::vec2 k_FirstPlace = k_Workshop + glm::vec2 {0.0f, -7.8f};
	all.push_back({
	    .id = "objects.scaffold_snaps_into_yard",
	    .name = "A scaffold put down near a workshop's yard goes into its place",
	    .facet = Facet::Objects,
	    .description = "A Celtic town of the player's with a village centre and a workshop with no wood, its yard towards the "
	                   "camera, and a loose "
	                   "scaffold that no workshop counts. After one second the scaffold is lifted and let go one metre "
	                   "from the first place of the yard, as the hand puts one down.",
	    .expected = "The scaffold comes to rest and moves into the first place of the yard, turned as the place is "
	                "turned. No puff shows. The workshop now counts the scaffold as its own (the inspector shows it), "
	                "and the physics does not move the scaffold while it stands there.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {k_Workshop - glm::vec2 {12.0f}, k_Workshop + glm::vec2 {12.0f}}},
	    .objects = {{.type = AbodeInfo::CelticTownCentre, .offset = {0.0f, 50.0f}},
	                {.type = AbodeInfo::CelticWorkshop, .offset = k_Workshop, .yawDegrees = 180.0f, .storedWood = 0},
	                {.type = ScaffoldInfo::Normal, .offset = k_Workshop + glm::vec2 {10.0f, 10.0f}, .scale = 0.7f}},
	    .throws = {{.object = 2,
	                .from = k_FirstPlace + glm::vec2 {0.7f, -0.7f},
	                .height = 1.0f,
	                .velocity = {0.0f, -1.0f, 0.0f},
	                .delaySeconds = 1.0f}},
	});

	all.push_back({
	    .id = "objects.scaffold_tap_breaks",
	    .name = "A tap breaks one scaffold off a scaffold, and a storage pit takes a scaffold as wood",
	    .facet = Facet::Objects,
	    .description = "A Celtic town of the player's with a village centre and a storage pit, and a loose scaffold that "
	                   "is worth three scaffolds and that no workshop counts. Put the hand over the scaffold, tap it, "
	                   "then pick up a part and hold it over the other part and over the storage pit.",
	    .expected = "Over the scaffold the tooltip shows \"Scaffold Value\" with 3, and \"Tap\". A tap breaks one small "
	                "scaffold off. The two parts move apart along a quarter turn of the scaffold and one of four sounds "
	                "plays. The old scaffold is now worth 2. A held part over the other part shows \"Combine "
	                "Scaffolds\". Over the storage pit it shows \"Supply\", and the pit takes the wood of all the "
	                "scaffolds that the part is worth. Elsewhere the tooltip shows the name of the building that the "
	                "phantom shows, or \"Build\", then \"Throw\".",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {k_Workshop - glm::vec2 {15.0f}, k_Workshop + glm::vec2 {15.0f}}},
	    .objects = {{.type = AbodeInfo::CelticTownCentre, .offset = {0.0f, 50.0f}},
	                {.type = AbodeInfo::CelticStoragePit, .offset = k_Workshop + glm::vec2 {-15.0f, 0.0f}},
	                {.type = ScaffoldInfo::Normal, .offset = k_Workshop, .scale = 1.1f}},
	});

	all.push_back({
	    .id = "objects.workshop_wood_by_hand",
	    .name = "The hand takes wood from a workshop's pile and gives it back",
	    .facet = Facet::Objects,
	    .description = "A Celtic town of the player's with a village centre and a workshop that holds 1000 wood, less "
	                   "than the wood of one scaffold, so the workshop makes no scaffold. The pile of logs of the "
	                   "workshop faces the camera. The input tools/sim/hand_inputs/workshop_wood.json moves the hand.",
	    .expected = "With a held press on the pile, the hand takes logs from it, and the pile becomes smaller. The "
	                "tooltip of the workshop shows less wood. When the hand lets the logs go on the workshop, the "
	                "workshop takes them into its pile again, and the tooltip shows the wood that it had at the start.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {k_Workshop - glm::vec2 {12.0f}, k_Workshop + glm::vec2 {12.0f}}},
	    .objects = {{.type = AbodeInfo::CelticTownCentre, .offset = {0.0f, 50.0f}},
	                {.type = AbodeInfo::CelticWorkshop, .offset = k_Workshop, .yawDegrees = 90.0f, .storedWood = 1000}},
	});

	all.push_back({
	    .id = "objects.workshop_takes_thrown_wood",
	    .name = "A workshop takes a handful of wood that the hand throws at it",
	    .facet = Facet::Objects,
	    .description = "A Celtic town of the player's with a village centre and a workshop that holds 1000 wood. A "
	                   "handful of 600 wood is lifted 20 m west of the workshop and thrown at it as the hand throws it.",
	    .expected = "The handful flies east and comes down at the workshop. It goes, and the workshop takes the 600 wood "
	                "into its pile: the workshop then holds 1600 wood, still less than the wood of one scaffold.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {k_Workshop - glm::vec2 {25.0f}, k_Workshop + glm::vec2 {15.0f}}},
	    .objects = {{.type = AbodeInfo::CelticTownCentre, .offset = {0.0f, 50.0f}},
	                {.type = AbodeInfo::CelticWorkshop, .offset = k_Workshop, .yawDegrees = 90.0f, .storedWood = 1000},
	                {.type = PotInfo::HandWood, .offset = k_Workshop + glm::vec2 {-20.0f, 0.0f}, .amount = 600}},
	    .throws = {{.object = 2,
	                .from = k_Workshop + glm::vec2 {-20.0f, 0.0f},
	                .height = 6.0f,
	                .velocity = {11.0f, 3.0f, 0.0f},
	                .delaySeconds = 2.0f}},
	});

	all.push_back({
	    .id = "objects.workshop_wood_flag",
	    .name = "The flag of a workshop shows the wood that the workshop needs for a scaffold",
	    .facet = Facet::Objects,
	    .description = "The town and the workshop of objects.workshop_wood_by_hand: the workshop holds 1000 wood, less "
	                   "than the wood of one scaffold. The input tools/sim/hand_inputs/workshop_wood.json moves the hand: "
	                   "it takes logs from the pile of the workshop, then gives them back.",
	    .expected = "The wood flag flies beside the workshop and turns to face the camera. When the hand takes logs, the "
	                "flag moves up smoothly, because the workshop needs more wood. When the hand gives the logs back, the "
	                "flag moves down to its first height.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {k_Workshop - glm::vec2 {12.0f}, k_Workshop + glm::vec2 {12.0f}}},
	    .objects = {{.type = AbodeInfo::CelticTownCentre, .offset = {0.0f, 50.0f}},
	                {.type = AbodeInfo::CelticWorkshop, .offset = k_Workshop, .yawDegrees = 90.0f, .storedWood = 1000}},
	});

	all.push_back({
	    .id = "hand.tooltips_workshop_wood_flag",
	    .name = "The hand over the wood flag of a workshop",
	    .facet = Facet::Objects,
	    .description = "The town and the workshop of objects.workshop_wood_by_hand: the workshop holds 1000 wood, less "
	                   "than the wood of one scaffold, and has room for three scaffolds. Put the hand over the wood flag "
	                   "beside the workshop, also out of the influence of the player.",
	    .expected = "Over the flag the tooltip shows \"Wood Required\" with the wood of three scaffolds less 1000, at "
	                "once and with no mouse button drawn. It shows also where the hand is out of the influence.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {k_Workshop - glm::vec2 {12.0f}, k_Workshop + glm::vec2 {12.0f}}},
	    .objects = {{.type = AbodeInfo::CelticTownCentre, .offset = {0.0f, 50.0f}},
	                {.type = AbodeInfo::CelticWorkshop, .offset = k_Workshop, .yawDegrees = 90.0f, .storedWood = 1000}},
	});

	all.push_back({
	    .id = "hand.tooltips_workshop_production",
	    .name = "The hand over a workshop that makes a scaffold",
	    .facet = Facet::Objects,
	    .description = "A Celtic town of the player's with a village centre and a workshop that holds 10000 wood, the "
	                   "wood of four scaffolds. The hand is over the workshop, in the influence of the player.",
	    .expected = "The workshop starts a scaffold, and its chimney smokes. Over the workshop the tooltip shows "
	                "\"Production Completed\" with the part of the scaffold that the workshop made, as a whole percent "
	                "that goes up from 0 to 99. When the scaffold is finished, the percent starts again at 0 for the next "
	                "scaffold. When the yard holds three scaffolds, the workshop stops, the tooltip shows 0, and the "
	                "smoke stops.",
	    .environment = {.dispenserGrid = false, .cursor = glm::vec2 {0.5f, 0.5f}},
	    .framing = {.shot = Shot::Placed,
	                .eye = {k_Workshop.x, 22.0f, k_Workshop.y - 32.0f},
	                .look = {k_Workshop.x, 3.0f, k_Workshop.y}},
	    .objects = {{.type = AbodeInfo::CelticTownCentre, .offset = {0.0f, 50.0f}},
	                {.type = AbodeInfo::CelticWorkshop, .offset = k_Workshop, .yawDegrees = 180.0f, .storedWood = 10000}},
	});

	// The place where the hand puts the villager down, in front of the workshop and beside its yard
	constexpr glm::vec2 k_DropPlace = k_Workshop + glm::vec2 {6.0f, -6.0f};
	all.push_back({
	    .id = "villagers.craftsman_supplies_workshop",
	    .name = "Craftsmen bring the wood of the storage pit to the workshop",
	    .facet = Facet::Objects,
	    .description = "A Celtic town of the player's (which believes in the player from the start) with a village centre, "
	                   "a storage pit that holds 3000 wood 25 m to the west, and a built workshop with no wood. A "
	                   "craftsman disciple stands by the workshop. A grown villager of the town, not a disciple, stands "
	                   "to the east. After one second the hand takes that villager, and after three more seconds it puts "
	                   "the villager down by the workshop. Best watched at game speed 4.",
	    .expected = "The villager that the hand puts down by the workshop becomes a craftsman disciple: it glows and the "
	                "craftsman's line plays. Each craftsman walks to the storage pit, takes as much wood as it can carry, "
	                "walks to the workshop and puts the wood in. When the workshop has the wood of a scaffold, it makes a "
	                "scaffold. The town's desire for wood (the inspector's living.town_desires, desire 1) adds the share of "
	                "the adults that are craftsmen.",
	    .environment = {.dispenserGrid = false, .cursor = glm::vec2 {0.5f, 0.5f}},
	    .framing = {.shot = Shot::Placed,
	                .eye = {k_DropPlace.x - 10.0f, 30.0f, k_DropPlace.y - 40.0f},
	                .look = {k_DropPlace.x, 0.0f, k_DropPlace.y}},
	    .objects = {{.type = AbodeInfo::CelticTownCentre, .offset = {0.0f, 50.0f}},
	                {.type = AbodeInfo::CelticStoragePit, .offset = k_Workshop + glm::vec2 {-25.0f, 0.0f}, .storedWood = 3000},
	                {.type = AbodeInfo::CelticWorkshop, .offset = k_Workshop, .yawDegrees = 180.0f, .storedWood = 0},
	                {.type = VillagerInfo::CelticFarmerMale,
	                 .offset = k_Workshop + glm::vec2 {-6.0f, -8.0f},
	                 .joinTown = true,
	                 .disciple = VillagerDisciple::Craftsman},
	                {.type = VillagerInfo::CelticFarmerMale,
	                 .offset = k_Workshop + glm::vec2 {20.0f, -8.0f},
	                 .joinTown = true}},
	    .commands = {{.kind = Kind::HandTake, .delaySeconds = 1.0f, .object = 4},
	                 {.kind = Kind::HandRelease, .delaySeconds = 3.0f}},
	    // The town belongs to the player at the start. Thus the hand of the player holds a villager of its own.
	    .townBeliefInPlayer = 1000.0f,
	});

	constexpr glm::vec2 k_LooseScaffold {-25.0f, 30.0f};
	constexpr glm::vec2 k_BuildPlace {-5.0f, 25.0f};
	constexpr glm::vec2 k_FoundingScaffold {-60.0f, 80.0f};
	all.push_back({
	    .id = "objects.scaffold_starts_building",
	    .name = "A scaffold put down near a town starts the building that it offers",
	    .facet = Facet::Objects,
	    .description = "A Celtic town of the player's with a village centre and a loose scaffold of the value of one "
	                   "scaffold. The input tools/sim/hand_inputs/scaffold_build.json moves the hand: it takes the "
	                   "scaffold, holds it still near the village centre, and puts it down.",
	    .expected = "While the hand holds the scaffold still near the village centre, the phantom of the building that "
	                "the town wants grows below the scaffold and turns slowly. When the hand puts the scaffold down, no "
	                "puff shows, a sound plays, and a building site of that building opens where the scaffold is. The "
	                "scaffold stays on the site. While nothing of the building is built, the hand can take the scaffold "
	                "again, and the site then goes.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {k_LooseScaffold - glm::vec2 {10.0f}, k_BuildPlace + glm::vec2 {10.0f}}},
	    .objects = {{.type = AbodeInfo::CelticTownCentre, .offset = {0.0f, 50.0f}},
	                {.type = ScaffoldInfo::Normal, .offset = k_LooseScaffold, .scale = 0.7f}},
	});

	constexpr glm::vec2 k_JoinTarget {-25.0f, 18.0f};
	all.push_back({
	    .id = "hand.scaffold_phantom_sounds_and_join",
	    .name = "A held scaffold plays the sounds of its phantom, names the building, and joins a scaffold",
	    .facet = Facet::Hand,
	    .description = "A Celtic town of the player's with a village centre, a loose scaffold of the value of two "
	                   "scaffolds and, 12 m south of it, a loose scaffold of the value of one. Take the larger "
	                   "scaffold and hold it still near the village centre. Then hold it over the smaller scaffold and "
	                   "press the Action button. Run with the audio log at the debug level to see the sounds.",
	    .expected = "Near the village centre the phantom of the building that the town wants grows below the scaffold, "
	                "and the appear sound plays one time. The tooltip shows the name of that building, then \"Throw\". "
	                "When the offer changes or goes, the phantom becomes small and the vanish sound plays one time. Over "
	                "the smaller scaffold the tooltip shows \"Combine Scaffolds\". The press removes the held scaffold. "
	                "The scaffold on the land now has the value of three. A puff shows at it, and the first of the four "
	                "join sounds plays at the hand.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {k_JoinTarget - glm::vec2 {10.0f}, k_BuildPlace + glm::vec2 {10.0f}}},
	    .objects = {{.type = AbodeInfo::CelticTownCentre, .offset = {0.0f, 50.0f}},
	                {.type = ScaffoldInfo::Normal, .offset = k_LooseScaffold, .scale = 0.9f},
	                {.type = ScaffoldInfo::Normal, .offset = k_JoinTarget, .scale = 0.7f}},
	    .townBeliefInPlayer = 1.0f,
	});

	all.push_back({
	    .id = "light.snow_on_scaffold_phantom",
	    .name = "The phantom of a scaffold carries snow in the hand and none on its site",
	    .facet = Facet::Light,
	    .description = "The town, the scaffold and the camera of objects.scaffold_starts_building, in deep snow over the "
	                   "whole island. The input tools/sim/hand_inputs/scaffold_build.json moves the hand: it takes the "
	                   "scaffold, holds it still near the village centre, and puts it down.",
	    .expected = "While the hand holds the scaffold still, the phantom of the building grows below the scaffold, and "
	                "snow comes onto it in about four seconds. When the hand puts the scaffold down, the phantom on the "
	                "building site shows no snow, and it never shows snow again. The scaffold itself shows snow as "
	                "other things do.",
	    .environment = {.dispenserGrid = false, .snowDepth = 200.0f},
	    .framing = {.shot = Shot::Overview, .include = {k_LooseScaffold - glm::vec2 {10.0f}, k_BuildPlace + glm::vec2 {10.0f}}},
	    .objects = {{.type = AbodeInfo::CelticTownCentre, .offset = {0.0f, 50.0f}},
	                {.type = ScaffoldInfo::Normal, .offset = k_LooseScaffold, .scale = 0.7f}},
	});

	constexpr glm::vec2 k_ImpressedHut {-25.0f, 47.0f};
	all.push_back({
	    .id = "objects.scaffold_impresses_town",
	    .name = "A scaffold that the player puts down impresses the people of the near town",
	    .facet = Facet::Objects,
	    .description = "The town, the scaffold and the camera of objects.scaffold_starts_building, with a hut of four "
	                   "villagers of the town beside the village centre. The input tools/sim/hand_inputs/scaffold_build.json "
	                   "moves the hand: it takes the scaffold, holds it still near the village centre, and puts it down.",
	    .expected = "When the hand puts the scaffold down, a building site opens. At the same time, symbols of belief in "
	                "the player rise from the first villagers of the hut: from a part of the town, and from one villager "
	                "at least. The belief of the town in the player becomes larger.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {k_LooseScaffold - glm::vec2 {10.0f}, k_BuildPlace + glm::vec2 {10.0f}}},
	    .objects = {{.type = AbodeInfo::CelticTownCentre, .offset = {0.0f, 50.0f}},
	                {.type = ScaffoldInfo::Normal, .offset = k_LooseScaffold, .scale = 0.7f},
	                {.type = AbodeInfo::CelticHut, .offset = k_ImpressedHut},
	                {.type = VillagerInfo::CelticFarmerMale,
	                 .offset = k_ImpressedHut + glm::vec2 {-6.0f, -6.0f},
	                 .joinTown = true,
	                 .homeIn = 2},
	                {.type = VillagerInfo::CelticHousewifeFemale,
	                 .offset = k_ImpressedHut + glm::vec2 {-3.0f, -7.0f},
	                 .joinTown = true,
	                 .homeIn = 2},
	                {.type = VillagerInfo::CelticForesterMale,
	                 .offset = k_ImpressedHut + glm::vec2 {3.0f, -7.0f},
	                 .joinTown = true,
	                 .homeIn = 2},
	                {.type = VillagerInfo::CelticHousewifeFemale,
	                 .offset = k_ImpressedHut + glm::vec2 {6.0f, -6.0f},
	                 .joinTown = true,
	                 .homeIn = 2}},
	});

	all.push_back({
	    .id = "objects.scaffold_offers_wonder",
	    .name = "A scaffold of the value of seven offers the town a wonder",
	    .facet = Facet::Objects,
	    .description = "The town and the camera of objects.scaffold_starts_building. The town believes in the player, "
	                   "so it wants a wonder. The loose scaffold has the value of seven scaffolds. An oak stands near the "
	                   "place where the hand puts the scaffold down. The input tools/sim/hand_inputs/scaffold_build.json "
	                   "moves the hand: it takes the scaffold, holds it still near the village centre, and puts it down.",
	    .expected = "While the hand holds the scaffold still, the phantom of the Celtic wonder grows below the scaffold. "
	                "Its size comes from the wonder power at that place: the desire of the town for a wonder, and the "
	                "things near the place. When the hand puts the scaffold down, a building site of the wonder opens. "
	                "The inspector query town.wonders shows the wonder and its power. When the wonder is finished, the "
	                "most Celtic power of the player becomes larger by the power of the wonder.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {k_LooseScaffold - glm::vec2 {10.0f}, k_BuildPlace + glm::vec2 {10.0f}}},
	    .objects = {{.type = AbodeInfo::CelticTownCentre, .offset = {0.0f, 50.0f}},
	                {.type = ScaffoldInfo::Normal, .offset = k_LooseScaffold, .scale = 1.9f},
	                {.type = TreeInfo::Oak, .offset = k_BuildPlace + glm::vec2 {-4.0f, -6.0f}}},
	    .townBeliefInPlayer = 1.0f,
	});

	all.push_back({
	    .id = "objects.scaffold_site_destroyed_starts_again",
	    .name = "A scaffold starts a new site when an effect destroys its unfinished building",
	    .facet = Facet::Objects,
	    .description = "The town, the scaffold and the camera of objects.scaffold_starts_building. After one second the "
	                   "scaffold starts the building that the town offers where it lies, as when the hand puts it down. "
	                   "After four seconds more, an effect destroys the unfinished building of the site, as when a fire "
	                   "takes the last of its life.",
	    .expected = "A building site opens at the scaffold, with the phantom of its building. When the effect destroys "
	                "the building, a ghost of the building flickers out, and at once the scaffold opens a new site where "
	                "it lies. The new site can show another building or another angle, because the town looks for its "
	                "offer again. The planting sound plays, and the hand can take the scaffold again for a time.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {k_LooseScaffold - glm::vec2 {10.0f}, k_BuildPlace + glm::vec2 {10.0f}}},
	    .objects = {{.type = AbodeInfo::CelticTownCentre, .offset = {0.0f, 50.0f}},
	                {.type = ScaffoldInfo::Normal, .offset = k_LooseScaffold, .scale = 0.7f}},
	    .commands = {{.kind = Kind::ScaffoldStartsSite, .delaySeconds = 1.0f, .object = 1},
	                 {.kind = Kind::DestroyScaffoldSite, .delaySeconds = 4.0f, .object = 1}},
	});

	all.push_back({
	    .id = "objects.scaffold_site_phantom_see_through",
	    .name = "The phantom of the building on a scaffold's site draws in the see-through modes",
	    .facet = Facet::Objects,
	    .description = "The town and the scaffold of objects.scaffold_starts_building, with the camera near the scaffold. "
	                   "After one second the scaffold starts the building that the town offers where it lies, as when "
	                   "the hand puts it down.",
	    .expected = "A building site opens at the scaffold, with the phantom of its building over the site at about a "
	                "third of its alpha. Each material of the phantom draws in its see-through mode with the alpha "
	                "reference 10, which becomes 0 at the alpha of the phantom. Thus no part of the phantom is cut: also "
	                "the edges that its textures make clear show at the alpha of their texels, and write depth.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview,
	                .include = {k_LooseScaffold - glm::vec2 {12.0f}, k_LooseScaffold + glm::vec2 {12.0f}}},
	    .objects = {{.type = AbodeInfo::CelticTownCentre, .offset = {0.0f, 50.0f}},
	                {.type = ScaffoldInfo::Normal, .offset = k_LooseScaffold, .scale = 0.7f}},
	    .commands = {{.kind = Kind::ScaffoldStartsSite, .delaySeconds = 1.0f, .object = 1}},
	});

	all.push_back({
	    .id = "objects.scaffold_script_active",
	    .name = "A script sets a scaffold active, and the scaffold starts its building at once",
	    .facet = Facet::Objects,
	    .description = "The town, the scaffold and the camera of objects.scaffold_starts_building. After one second a "
	                   "script sets the scaffold inactive. After one second more, the script sets the scaffold active.",
	    .expected = "The first call does not change the scaffold. At the second call, a building site opens at the "
	                "scaffold, with the phantom of the building that the town offers where the scaffold lies. No player "
	                "put the scaffold down, so the planting sound does not play for the local player.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {k_LooseScaffold - glm::vec2 {10.0f}, k_BuildPlace + glm::vec2 {10.0f}}},
	    .objects = {{.type = AbodeInfo::CelticTownCentre, .offset = {0.0f, 50.0f}},
	                {.type = ScaffoldInfo::Normal, .offset = k_LooseScaffold, .scale = 0.7f}},
	    .commands = {{.kind = Kind::CallNative,
	                  .delaySeconds = 1.0f,
	                  .script = "SET_ACTIVE",
	                  .scriptArguments = {{.kind = ScriptArgument::Kind::Integer, .number = 0.0f},
	                                      {.kind = ScriptArgument::Kind::Object, .index = 1}}},
	                 {.kind = Kind::CallNative,
	                  .delaySeconds = 1.0f,
	                  .script = "SET_ACTIVE",
	                  .scriptArguments = {{.kind = ScriptArgument::Kind::Integer, .number = 1.0f},
	                                      {.kind = ScriptArgument::Kind::Object, .index = 1}}}},
	});

	all.push_back({
	    .id = "objects.wood_poured_on_scaffold_site",
	    .name = "The hand pours wood onto a scaffold on a site, and the site gets the wood",
	    .facet = Facet::Objects,
	    .description = "The town, the scaffold and the camera of objects.scaffold_starts_building. After one second the "
	                   "scaffold starts the building that the town offers where it lies. After three seconds more, the "
	                   "hand of the player pours 300 wood over the scaffold.",
	    .expected = "A building site opens at the scaffold. The poured wood goes into the site, as into a store. No pile "
	                "of wood stays on the ground at the scaffold. The inspector shows the wood of the site go up by the "
	                "wood that the hand poured.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {k_LooseScaffold - glm::vec2 {10.0f}, k_BuildPlace + glm::vec2 {10.0f}}},
	    .objects = {{.type = AbodeInfo::CelticTownCentre, .offset = {0.0f, 50.0f}},
	                {.type = ScaffoldInfo::Normal, .offset = k_LooseScaffold, .scale = 0.7f}},
	    .commands = {{.kind = Kind::ScaffoldStartsSite, .delaySeconds = 1.0f, .object = 1},
	                 {.kind = Kind::HandPour, .delaySeconds = 3.0f, .point = k_LooseScaffold, .value = 300}},
	});

	all.push_back({
	    .id = "objects.scaffold_founds_town",
	    .name = "A scaffold put down away from every town founds a new town",
	    .facet = Facet::Objects,
	    .description = "A Celtic town of the player's in the middle of the map, with no buildings, and a scaffold of the "
	                   "town with the value of five scaffolds, 100 m away from the town. After one second the scaffold "
	                   "starts the building that it offers where it lies, as when the hand puts it down.",
	    .expected = "A building site of a Celtic village centre opens at the scaffold, with the phantom of the village "
	                "centre. The inspector query town.list shows a new Celtic town of the player's with the id 703058. "
	                "The new town believes in the player with the starting belief of a new town, and has no reach of "
	                "its own until it has buildings.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview,
	                .include = {k_FoundingScaffold - glm::vec2 {20.0f}, k_FoundingScaffold + glm::vec2 {20.0f}}},
	    .objects = {{.type = ScaffoldInfo::Normal, .offset = k_FoundingScaffold, .scale = 1.5f, .joinTown = true}},
	    .commands = {{.kind = Kind::ScaffoldStartsSite, .delaySeconds = 1.0f, .object = 0}},
	});

	all.push_back({
	    .id = "objects.burning_scaffold_tap",
	    .name = "A tap on a burning scaffold breaks off a burning piece, and the fire burns the scaffolds away",
	    .facet = Facet::Objects,
	    .description = "A loose scaffold that is worth three scaffolds and that no workshop counts, alight as at the start "
	                   "of a fire, with no town near. After one second the hand of the player taps the scaffold.",
	    .expected = "The tap breaks one small scaffold off. The two parts move apart and one of four sounds plays. The "
	                "piece burns as the old scaffold burns: the inspector shows a Fire on the two scaffolds. The fire "
	                "takes the life of each scaffold. When a scaffold has no life, it goes and leaves nothing.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview,
	                .include = {k_FoundingScaffold - glm::vec2 {12.0f}, k_FoundingScaffold + glm::vec2 {12.0f}}},
	    .objects = {{.type = ScaffoldInfo::Normal, .offset = k_FoundingScaffold, .scale = 1.1f, .alight = true}},
	    .commands = {{.kind = Kind::HandTapObject, .delaySeconds = 1.0f, .object = 0}},
	});

	all.push_back({
	    .id = "objects.scaffold_clears_ground",
	    .name = "A scaffold that a script marked clears its ground when the hand lets it go",
	    .facet = Facet::Objects,
	    .description = "The town, the scaffold and the camera of objects.scaffold_starts_building. A script marked the "
	                   "scaffold to destroy the things in its way. A bush and a ball stand where the hand puts the "
	                   "scaffold down. The input tools/sim/hand_inputs/scaffold_build.json moves the hand: it takes the "
	                   "scaffold, holds it still near the village centre, and puts it down.",
	    .expected = "When the hand lets the scaffold go, the bush and the ball jump out of the way. Each starts from the "
	                "height of the scaffold above the land and flies a short distance. A building site then opens where "
	                "the scaffold is.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {k_LooseScaffold - glm::vec2 {10.0f}, k_BuildPlace + glm::vec2 {10.0f}}},
	    .objects = {{.type = AbodeInfo::CelticTownCentre, .offset = {0.0f, 50.0f}},
	                {.type = ScaffoldInfo::Normal, .offset = k_LooseScaffold, .scale = 0.7f, .clearsGround = true},
	                {.type = TreeInfo::Bush, .offset = k_BuildPlace},
	                {.type = MobileObjectInfo::Ball, .offset = k_BuildPlace + glm::vec2 {0.0f, -1.5f}}},
	});

	all.push_back({
	    .id = "objects.scaffold_clears_ground_creature",
	    .name = "A scaffold that clears its ground leaves a creature alone",
	    .facet = Facet::Objects,
	    .description =
	        "A loose scaffold that a script marked to destroy the things in its way, away from every town. The "
	        "player's tiger stands still 3 m east of the scaffold, and a bush stands 1.4 m north-east of it. After one "
	        "second the scaffold clears its ground, as when the hand lets it go.",
	    .expected = "The bush jumps out of the way. The tiger stays where it is and stays in the game: the inspector "
	                "still shows it. The game log tells of the push of the bush and of no removal.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview,
	                .include = {k_FoundingScaffold - glm::vec2 {15.0f}, k_FoundingScaffold + glm::vec2 {15.0f}}},
	    .creatures = {{.species = CreatureType::Tiger,
	                   .offset = k_FoundingScaffold + glm::vec2 {3.0f, 0.0f},
	                   .pauseMind = true}},
	    .objects = {{.type = ScaffoldInfo::Normal, .offset = k_FoundingScaffold, .scale = 0.7f, .clearsGround = true},
	                {.type = TreeInfo::Bush, .offset = k_FoundingScaffold + glm::vec2 {1.0f, 1.0f}}},
	    .commands = {{.kind = Kind::ScaffoldClearsGround, .delaySeconds = 1.0f, .object = 0}},
	});

	all.push_back({
	    .id = "objects.scaffold_clears_ground_fixed_rock",
	    .name = "A scaffold that clears its ground removes a rock and pushes a ball",
	    .facet = Facet::Objects,
	    .description = "A loose scaffold that a script marked to destroy the things in its way, away from every town. A "
	                   "rock stands 1.5 m east of it and a ball 1.5 m north of it. After one second the scaffold clears "
	                   "its ground, as when the hand lets it go.",
	    .expected = "The rock has the fixed mark of the land, so the scaffold does not push it: the rock goes from the "
	                "game at once. The ball has no mark, so it jumps out of the way and stays in the game. The game log "
	                "tells of the removal of the rock and of the push of the ball.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview,
	                .include = {k_FoundingScaffold - glm::vec2 {15.0f}, k_FoundingScaffold + glm::vec2 {15.0f}}},
	    .objects = {{.type = ScaffoldInfo::Normal, .offset = k_FoundingScaffold, .scale = 0.7f, .clearsGround = true},
	                {.type = MobileStaticInfo::Rock, .offset = k_FoundingScaffold + glm::vec2 {1.5f, 0.0f}},
	                {.type = MobileObjectInfo::Ball, .offset = k_FoundingScaffold + glm::vec2 {0.0f, 1.5f}}},
	    .commands = {{.kind = Kind::ScaffoldClearsGround, .delaySeconds = 1.0f, .object = 0}},
	    .checks = {{.label = "the rock goes; the scaffold and the ball stay in the game",
	                .quantity = checks::Quantity::ObjectsAlive,
	                .afterTurns = 50,
	                .atLeast = 2,
	                .atMost = 2}},
	});

	all.push_back({
	    .id = "objects.scaffold_push_on_high_land",
	    .name = "A thing that a clearing scaffold pushes starts under high land",
	    .facet = Facet::Objects,
	    .description = "A loose scaffold that a script marked to destroy the things in its way lies on the land, which is "
	                   "about 21 m high here. A bush stands 1.5 m east of it and a ball 1.5 m north of it. After one second "
	                   "the scaffold clears its ground, as when the hand lets it go.",
	    .expected = "As in the game, the bush and the ball start at the height of the scaffold above the land plus half a "
	                "metre, twice as far from the scaffold as the edge of the scaffold. That is a height of the world, so "
	                "each starts under the land here. The land throws each one far across the map.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview,
	                .include = {k_FoundingScaffold - glm::vec2 {15.0f}, k_FoundingScaffold + glm::vec2 {15.0f}}},
	    .objects = {{.type = ScaffoldInfo::Normal, .offset = k_FoundingScaffold, .scale = 0.7f, .clearsGround = true},
	                {.type = TreeInfo::Bush, .offset = k_FoundingScaffold + glm::vec2 {1.5f, 0.0f}},
	                {.type = MobileObjectInfo::Ball, .offset = k_FoundingScaffold + glm::vec2 {0.0f, 1.5f}}},
	    .commands = {{.kind = Kind::ScaffoldClearsGround, .delaySeconds = 1.0f, .object = 0}},
	});

	all.push_back({
	    .id = "objects.scaffold_push_straight_west_or_south",
	    .name = "A clearing scaffold pushes a thing straight west and a thing straight south",
	    .facet = Facet::Objects,
	    .description = "A loose scaffold that a script marked to destroy the things in its way. A bush stands 1.5 m "
	                   "straight west of it and a ball 1.5 m straight south of it. After one second the scaffold clears "
	                   "its ground, as when the hand lets it go.",
	    .expected = "The bush starts west and the ball starts south of the scaffold, under the land, as in the game. The "
	                "land throws each one far, and the bush becomes a dead tree. The game divides the push by 0 here and "
	                "the things go from the map. openblack divides by the larger size, so both things stay on the map.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview,
	                .include = {k_FoundingScaffold - glm::vec2 {15.0f}, k_FoundingScaffold + glm::vec2 {15.0f}}},
	    .objects = {{.type = ScaffoldInfo::Normal, .offset = k_FoundingScaffold, .scale = 0.7f, .clearsGround = true},
	                {.type = TreeInfo::Bush, .offset = k_FoundingScaffold + glm::vec2 {-1.5f, 0.0f}},
	                {.type = MobileObjectInfo::Ball, .offset = k_FoundingScaffold + glm::vec2 {0.0f, -1.5f}}},
	    .commands = {{.kind = Kind::ScaffoldClearsGround, .delaySeconds = 1.0f, .object = 0}},
	    .checks = {{.label = "the scaffold and the ball stay in the game",
	                .quantity = checks::Quantity::ObjectsAlive,
	                .afterTurns = 100,
	                .atLeast = 2},
	               {.label = "the ball lands near the scaffold, not at the corner of the map",
	                .quantity = checks::Quantity::ObjectDistance,
	                .index = 2,
	                .point = k_FoundingScaffold,
	                .afterTurns = 100,
	                .atMost = 100.0}},
	});

	all.push_back({
	    .id = "objects.workshop_site_wood_on_its_pile",
	    .name = "The wood for a workshop that goes up lies on the pile of the workshop",
	    .facet = Facet::Objects,
	    .description = "A Celtic town of the player's with a village centre, a storage pit that holds 6000 wood 25 m to the "
	                   "west, four builders of the town, and a workshop that is a tenth built. The workshop holds 1000 "
	                   "wood. Best watched at game speed 4.",
	    .expected = "The wood of the workshop lies on the pile of logs at the back of the workshop. No other pile of wood "
	                "comes for the site. The builders take wood from this pile to build, and they bring wood from the "
	                "storage pit to the workshop. When the workshop is finished, the wood that is left stays on the pile "
	                "as the wood of the workshop. The villagers do not take it away.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {k_Workshop - glm::vec2 {15.0f}, k_Workshop + glm::vec2 {15.0f}}},
	    .objects =
	        {{.type = AbodeInfo::CelticTownCentre, .offset = {0.0f, 50.0f}},
	         {.type = AbodeInfo::CelticStoragePit, .offset = k_Workshop + glm::vec2 {-25.0f, 0.0f}, .storedWood = 6000},
	         {.type = AbodeInfo::CelticWorkshop, .offset = k_Workshop, .yawDegrees = 180.0f, .storedWood = 1000, .built = 0.1f},
	         {.type = VillagerInfo::CelticFarmerMale, .offset = k_Workshop + glm::vec2 {-6.0f, -10.0f}, .joinTown = true},
	         {.type = VillagerInfo::CelticFarmerMale, .offset = k_Workshop + glm::vec2 {-3.0f, -11.0f}, .joinTown = true},
	         {.type = VillagerInfo::CelticForesterMale, .offset = k_Workshop + glm::vec2 {3.0f, -11.0f}, .joinTown = true},
	         {.type = VillagerInfo::CelticFarmerMale, .offset = k_Workshop + glm::vec2 {6.0f, -10.0f}, .joinTown = true}},
	});

	all.push_back({
	    .id = "story.reward_chest_scaffold",
	    .name = "A reward chest gives a town with a workshop a scaffold",
	    .facet = Facet::Objects,
	    .description = "A Celtic town of the player's with a village centre and a built workshop with no wood. After one "
	                   "second a reward chest of a scaffold lies opened in the town, near the workshop.",
	    .expected = "A small scaffold, worth one scaffold and not turned, appears where the chest is. The workshop counts "
	                "it as its own (the inspector shows it in the scaffolds of the workshop), but it does not move into "
	                "the yard.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {k_Workshop - glm::vec2 {15.0f}, k_Workshop + glm::vec2 {15.0f}}},
	    .objects = {{.type = AbodeInfo::CelticTownCentre, .offset = {0.0f, 50.0f}},
	                {.type = AbodeInfo::CelticWorkshop, .offset = k_Workshop, .yawDegrees = 180.0f, .storedWood = 0}},
	    .rewards = {{.type = RewardObjectInfo::Scaffold,
	                 .offset = k_Workshop + glm::vec2 {12.0f, -12.0f},
	                 .fromSky = false,
	                 .inTown = true,
	                 .delaySeconds = 1.0f,
	                 .opened = true}},
	});

	// The tiger and a second loose scaffold stand inside the shot of objects.scaffold_starts_building, away from the
	// hand's way, so that the shot and the hand's input stay the same. The tiger stays still until the input is over.
	// The second scaffold is far from the village centre: the creature walks round the centre, and a scaffold near its
	// edge is out of the creature's reach from there.
	constexpr glm::vec2 k_SiteWatcher {-32.0f, 47.0f};
	constexpr glm::vec2 k_SecondScaffold {-24.0f, 42.0f};
	constexpr float k_WatcherWaits = 20.0f;
	all.push_back({
	    .id = "objects.creature_scaffold_on_site",
	    .name = "A creature leaves a scaffold that a building site keeps",
	    .facet = Facet::Objects,
	    .description = "The town, the scaffold and the camera of objects.scaffold_starts_building, with the player's tiger "
	                   "and a second loose scaffold north of the scaffold. The tiger stays still for 20 seconds. The input "
	                   "tools/sim/hand_inputs/scaffold_build.json moves the hand: it takes the scaffold, holds it still "
	                   "near the village centre, and puts it down. After 100 seconds a script tells the tiger to examine "
	                   "the scaffold on the site by picking it up, and after 40 seconds more, the loose scaffold.",
	    .expected = "The scaffold opens a building site. After the time that a hand can take it back, the tiger cannot "
	                "pick it up: a creature takes a scaffold only when a hand can take it. The tiger picks up the loose "
	                "scaffold and looks at it.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {k_LooseScaffold - glm::vec2 {10.0f}, k_BuildPlace + glm::vec2 {10.0f}}},
	    .creatures = {{.species = CreatureType::Tiger,
	                   .offset = k_SiteWatcher,
	                   .pauseMind = true,
	                   .thinksAfterSeconds = k_WatcherWaits}},
	    .objects = {{.type = AbodeInfo::CelticTownCentre, .offset = {0.0f, 50.0f}},
	                {.type = ScaffoldInfo::Normal, .offset = k_LooseScaffold, .scale = 0.7f},
	                {.type = ScaffoldInfo::Normal, .offset = k_SecondScaffold, .scale = 0.7f}},
	    .commands = {{.kind = Kind::DoAction, .delaySeconds = 100.0f, .object = 1, .action = "ExamineByPickingUp"},
	                 {.kind = Kind::DoAction, .delaySeconds = 40.0f, .object = 2, .action = "ExamineByPickingUp"}},
	});

	// The abode number of a house and the size, as a land script gives Khazar's scaffold for a house
	constexpr float k_HouseNumber = 4.0f;
	constexpr float k_ScriptSize = 2.0f;
	all.push_back({
	    .id = "objects.script_gives_scaffold",
	    .name = "A script makes a scaffold, gives it a building and a size, and locks it",
	    .facet = Facet::Objects,
	    .description = "The town of objects.scaffold_starts_building with no loose scaffold. After one second, the "
	                   "script lines of a land give a scaffold: CREATE makes a scaffold (type 39) where the loose "
	                   "scaffold was, and SET_SCAFFOLD_PROPERTIES gives it the house of abode number 4 and the size 2. "
	                   "The input tools/sim/hand_inputs/scaffold_build.json moves the hand: it takes the scaffold, holds "
	                   "it still near the village centre, and puts it down. After ten more seconds, SET_ID_PICKUPABLE "
	                   "and SET_ID_MOVEABLE lock the scaffold.",
	    .expected = "A scaffold appears on the land. It has the size of a scaffold of the value of two scaffolds. While "
	                "the hand holds it still, the phantom shows only a house of abode number 4. When the hand puts it "
	                "down, a building site of that house opens. After the lock, the hand can not take the scaffold, and "
	                "the inspector shows CannotBePickedUp and Immovable on it.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {k_LooseScaffold - glm::vec2 {10.0f}, k_BuildPlace + glm::vec2 {10.0f}}},
	    .objects = {{.type = AbodeInfo::CelticTownCentre, .offset = {0.0f, 50.0f}}},
	    .commands = {{.kind = Kind::CallNative,
	                  .delaySeconds = 1.0f,
	                  .script = "CREATE",
	                  .scriptArguments = {{.kind = ScriptArgument::Kind::Integer,
	                                       .number = static_cast<float>(script::ObjectType::Scaffold)},
	                                      {.kind = ScriptArgument::Kind::Integer, .number = 5000.0f},
	                                      {.kind = ScriptArgument::Kind::Position, .point = k_LooseScaffold}}},
	                 {.kind = Kind::CallNative,
	                  .delaySeconds = 0.0f,
	                  .script = "SET_SCAFFOLD_PROPERTIES",
	                  .scriptArguments = {{.kind = ScriptArgument::Kind::Made, .index = 0},
	                                      {.kind = ScriptArgument::Kind::Integer, .number = k_HouseNumber},
	                                      {.kind = ScriptArgument::Kind::Number, .number = k_ScriptSize},
	                                      {.kind = ScriptArgument::Kind::Integer, .number = 0.0f}}},
	                 {.kind = Kind::CallNative,
	                  .delaySeconds = 10.0f,
	                  .script = "SET_ID_PICKUPABLE",
	                  .scriptArguments = {{.kind = ScriptArgument::Kind::Integer, .number = 0.0f},
	                                      {.kind = ScriptArgument::Kind::Made, .index = 0}}},
	                 {.kind = Kind::CallNative,
	                  .delaySeconds = 0.0f,
	                  .script = "SET_ID_MOVEABLE",
	                  .scriptArguments = {{.kind = ScriptArgument::Kind::Integer, .number = 0.0f},
	                                      {.kind = ScriptArgument::Kind::Made, .index = 0}}}},
	});

	// The workshop's abode number and the size, as the third land's script lays out the workshop of the home town
	constexpr float k_WorkshopNumber = 9.0f;
	constexpr float k_WorkshopSize = 3.0f;
	constexpr glm::vec2 k_RockyPlace {-45.0f, 15.0f};
	all.push_back({
	    .id = "objects.scaffold_builds_among_rocks",
	    .name = "A script scaffold starts its workshop among rocks",
	    .facet = Facet::Objects,
	    .description = "A Celtic town of the player's (which believes in the player from the start) with a village "
	                   "centre. Four rocks lie 4 m round a place 57 m from the centre. After one second, CREATE makes a "
	                   "scaffold (type 39) at that place, and "
	                   "SET_SCAFFOLD_PROPERTIES gives it the workshop (abode number 9) and the size 3. Then SET_ACTIVE "
	                   "sets the scaffold active, as the third land's script does with the workshop of the home town.",
	    .expected = "A building site of a workshop opens at the scaffold, with the phantom of the workshop over the "
	                "rocks. The rocks do not stop the building. The scaffold stays on the site.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {k_RockyPlace - glm::vec2 {15.0f}, k_RockyPlace + glm::vec2 {15.0f}}},
	    .objects = {{.type = AbodeInfo::CelticTownCentre, .offset = {0.0f, 50.0f}},
	                {.type = MobileStaticInfo::RockChalk, .offset = k_RockyPlace + glm::vec2 {4.0f, 0.0f}},
	                {.type = MobileStaticInfo::RockChalk, .offset = k_RockyPlace + glm::vec2 {-4.0f, 0.0f}},
	                {.type = MobileStaticInfo::RockChalk, .offset = k_RockyPlace + glm::vec2 {0.0f, 4.0f}},
	                {.type = MobileStaticInfo::RockChalk, .offset = k_RockyPlace + glm::vec2 {0.0f, -4.0f}}},
	    .commands = {{.kind = Kind::CallNative,
	                  .delaySeconds = 1.0f,
	                  .script = "CREATE",
	                  .scriptArguments = {{.kind = ScriptArgument::Kind::Integer,
	                                       .number = static_cast<float>(script::ObjectType::Scaffold)},
	                                      {.kind = ScriptArgument::Kind::Integer, .number = 5000.0f},
	                                      {.kind = ScriptArgument::Kind::Position, .point = k_RockyPlace}}},
	                 {.kind = Kind::CallNative,
	                  .delaySeconds = 0.0f,
	                  .script = "SET_SCAFFOLD_PROPERTIES",
	                  .scriptArguments = {{.kind = ScriptArgument::Kind::Made, .index = 0},
	                                      {.kind = ScriptArgument::Kind::Integer, .number = k_WorkshopNumber},
	                                      {.kind = ScriptArgument::Kind::Number, .number = k_WorkshopSize},
	                                      {.kind = ScriptArgument::Kind::Integer, .number = 0.0f}}},
	                 {.kind = Kind::CallNative,
	                  .delaySeconds = 0.0f,
	                  .script = "SET_ACTIVE",
	                  .scriptArguments = {{.kind = ScriptArgument::Kind::Integer, .number = 1.0f},
	                                      {.kind = ScriptArgument::Kind::Made, .index = 0}}}},
	    // A town of the neutral player wants no workshop, so the town stays the player's
	    .townBeliefInPlayer = 1000.0f,
	});
	// A fish farm 10 m from the scaffold, in the middle of the next cell and inside the outline of the workshop
	constexpr glm::vec2 k_FarmPlace {10.0f, 0.0f};
	all.push_back({
	    .id = "objects.scaffold_stopped_by_fish_farm",
	    .name = "A fish farm stops the workshop of a script scaffold",
	    .facet = Facet::Objects,
	    .description = "A Celtic town of the player's (which believes in the player from the start) with a village "
	                   "centre. A fish farm stands 10 m from a place 57 m from the centre. After one second, CREATE makes "
	                   "a scaffold (type 39) at that place, and SET_SCAFFOLD_PROPERTIES gives it the workshop (abode "
	                   "number 9) and the size 3. Then SET_ACTIVE sets the scaffold active.",
	    .expected = "No building site opens at the scaffold, because the fish farm is in the outline of the workshop. "
	                "The scaffold stays where it is, with no phantom of the workshop.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {k_RockyPlace - glm::vec2 {15.0f}, k_RockyPlace + glm::vec2 {15.0f}}},
	    .objects = {{.type = AbodeInfo::CelticTownCentre, .offset = {0.0f, 50.0f}},
	                {.type = FishFarmInfo::Normal, .offset = k_RockyPlace + k_FarmPlace}},
	    .commands = {{.kind = Kind::CallNative,
	                  .delaySeconds = 1.0f,
	                  .script = "CREATE",
	                  .scriptArguments = {{.kind = ScriptArgument::Kind::Integer,
	                                       .number = static_cast<float>(script::ObjectType::Scaffold)},
	                                      {.kind = ScriptArgument::Kind::Integer, .number = 5000.0f},
	                                      {.kind = ScriptArgument::Kind::Position, .point = k_RockyPlace}}},
	                 {.kind = Kind::CallNative,
	                  .delaySeconds = 0.0f,
	                  .script = "SET_SCAFFOLD_PROPERTIES",
	                  .scriptArguments = {{.kind = ScriptArgument::Kind::Made, .index = 0},
	                                      {.kind = ScriptArgument::Kind::Integer, .number = k_WorkshopNumber},
	                                      {.kind = ScriptArgument::Kind::Number, .number = k_WorkshopSize},
	                                      {.kind = ScriptArgument::Kind::Integer, .number = 0.0f}}},
	                 {.kind = Kind::CallNative,
	                  .delaySeconds = 0.0f,
	                  .script = "SET_ACTIVE",
	                  .scriptArguments = {{.kind = ScriptArgument::Kind::Integer, .number = 1.0f},
	                                      {.kind = ScriptArgument::Kind::Made, .index = 0}}}},
	    .townBeliefInPlayer = 1000.0f,
	});

	// The Workshop of the second land with a home town of more than 40 grown ups. The quest's script counts at most 40
	// of them when it finds the share of homeless grown ups. The places are the script's, from the middle of the land.
	constexpr glm::vec2 k_LandTwoMiddle {2240.0f, 2720.0f};
	const auto onLandTwo = [k_LandTwoMiddle](float x, float z) { return glm::vec2(x, z) - k_LandTwoMiddle; };
	constexpr uint32_t k_HomeTown = 0;
	const auto homeWorkshop = onLandTwo(3252.0894f, 3341.0461f);
	std::vector<ObjectSetup> homeTown {
	    {.type = AbodeInfo::NorseWorkshop,
	     .offset = homeWorkshop,
	     .yawDegrees = 160.0f,
	     .landTown = k_HomeTown,
	     .storedWood = 0},
	    {.type = AbodeInfo::NorseTempleX, .offset = onLandTwo(3190.0f, 3440.0f), .landTown = k_HomeTown},
	    {.type = AbodeInfo::NorseTempleX, .offset = onLandTwo(3215.0f, 3445.0f), .landTown = k_HomeTown},
	    {.type = AbodeInfo::NorseTempleX, .offset = onLandTwo(3170.0f, 3420.0f), .landTown = k_HomeTown},
	    {.type = AbodeInfo::NorseTempleX, .offset = onLandTwo(3240.0f, 3450.0f), .landTown = k_HomeTown}};
	constexpr size_t k_GrownUps = 45;
	for (size_t i = 0; i < k_GrownUps; ++i)
	{
		homeTown.push_back(
		    {.type = i % 2 == 0 ? VillagerInfo::NorseHousewifeFemale : VillagerInfo::NorseFarmerMale,
		     .offset = onLandTwo(3226.0f + static_cast<float>(i % 6) * 2.0f, 3405.0f + static_cast<float>(i / 6) * 2.0f),
		     .landTown = k_HomeTown});
	}
	all.push_back({
	    .id = "story.workshop_homeless_cap",
	    .name = "Story: The Workshop with a home town of more than 40 grown ups",
	    .facet = Facet::Story,
	    .description = "The second land of the story. The player's first town gets a workshop at the place of the "
	                   "quest, four houses and 45 grown ups. It then has more than 40 grown ups and room for 20 of "
	                   "them. The quest's script for the finished workshop runs. Play the quest by hand: click the "
	                   "scroll, give wood to the workshop, put its scaffold down as a house and build the house.",
	    .expected = "In the housing step the quest counts 40 grown ups, not more. With room for 20, half of the 40 have "
	                "no home and the step waits. When the room is 25 or more, fewer than 40 in 100 have no home and the "
	                "step ends: the second scroll appears at the workshop. With no limit of 40 the step would wait "
	                "until the room is 30.",
	    .environment = {.land = Land::Story, .storyLand = "Land2.txt", .dispenserGrid = false},
	    .framing = {.shot = Shot::Overview,
	                .include = {homeWorkshop + glm::vec2(-60.0f, -20.0f), homeWorkshop + glm::vec2(30.0f, 110.0f)}},
	    .objects = homeTown,
	    .commands = {{.kind = Kind::RunScript,
	                  .delaySeconds = 1.0f,
	                  .script = "TheWorkshopComplete",
	                  .scriptArguments = {{.kind = ScriptArgument::Kind::Object, .index = 0}}}},
	});
}

std::vector<Scenario> Build()
{
	std::vector<Scenario> all;
	AddIdle(all);
	AddExpressions(all);
	AddSenses(all);
	AddNeeds(all);
	AddRestAndHome(all);
	AddGrowth(all);
	AddAppearance(all);
	AddLight(all);
	AddMovement(all);
	AddFootprints(all);
	AddAudio(all);
	AddObjects(all);
	AddHand(all);
	AddLeash(all);
	AddCombat(all);
	AddMind(all);
	AddParticles(all);
	AddEditor(all);
	AddMiracleScenarios(all);
	AddGlobeScenarios(all);
	AddLifeLightScenarios(all);
	AddCreatureCastingScenarios(all);
	AddFriendActionScenarios(all);
	AddBenchmark(all);
	AddCreatureModeScenarios(all);
	AddSavedCameraScenarios(all);
	AddGestureScenarios(all);
	AddStormScenarios(all);
	AddFlockScenarios(all);
	AddBirdScenarios(all);
	AddTeleportScenarios(all);
	AddVortexScenarios(all);
	AddLandFiveQuestScenarios(all);
	AddLandsFourFiveScrollScenarios(all);
	AddLand4StoneScenarios(all);
	AddLand4SlegScenarios(all);
	AddPuzzleQuestScenarios(all);
	AddUndeadTotemScenarios(all);
	AddRewardScenarios(all);
	AddTornadoScenarios(all);
	AddPhysicsScenarios(all);
	AddNatureScenarios(all);
	AddHandNavigationScenarios(all);
	AddHandLookScenarios(all);
	AddGateScenarios(all);
	AddVillageTotemScenarios(all);
	AddTempleVillagerScenarios(all);
	AddFishScenarios(all);
	AddKnockScenarios(all);
	AddFootballScenarios(all);
	AddVillagerSpeedScenarios(all);
	AddDiscipleScenarios(all);
	AddVillagerLabelScenarios(all);
	AddTempleKeyScenarios(all);
	AddHighlightScenarios(all);
	AddSkyScenarios(all);
	AddAdvisorScenarios(all);
	AddStory(all);
	AddExplorersBoat(all);
	AddTownGrowthScenarios(all);
	AddStatisticsScenarios(all);
	AddMultiplayerFrontEndScenarios(all);
	AddOtherGodScenarios(all);
	AddJoinOnlineScenarios(all);
	AddTutorialScenarios(all);
	AddCreatureDanceScenarios(all);
	AddCreatureLearningScenarios(all);
	AddComputerPlayerScenarios(all);
	AddGameEndScenarios(all);
	AddEndingScenarios(all);
	AddLastStandScenarios(all);
	AddWorldSoundScenarios(all);
	AddTownMiracleScenarios(all);
	AddFixedKindScenarios(all);
	AddCreatureExplorationScenarios(all);
	AddCreatureActionScenarios(all);
	AddScriptTaskRelease(all);
	AddLandOneScriptStubScenarios(all);
	AddVillagerSizeAndReachScenarios(all);
	AddSwayAndFrightScenarios(all);
	AddStoreScenarios(all);
	AddPoisonScenarios(all);
	AddWorkshopScenarios(all);
	AddGodsRingMapScenarios(all);
	AddSelfCheckScenarios(all);
	AddWorshipSiteScenarios(all);
	AddLateSilverScrollScenarios(all);
	return all;
}

bool InRange(float value, float low, float high)
{
	return value >= low && value <= high;
}

void CheckNeeds(const NeedOverrides& needs, std::vector<std::string>& problems, std::string_view who)
{
	const std::array<std::pair<std::string_view, std::optional<float>>, 5> k_UnitNeeds {{
	    {"exhaustion", needs.exhaustion},
	    {"dehydration", needs.dehydration},
	    {"poo", needs.poo},
	    {"life", needs.life},
	    {"energy", needs.energy},
	}};
	for (const auto& [name, value] : k_UnitNeeds)
	{
		if (value.has_value() && !InRange(*value, 0.0f, creature_physiology::k_MaxGrownSize))
		{
			problems.push_back(fmt::format("{}: {} {} out of range", who, name, *value));
		}
	}
	if (needs.warmth.has_value() && !InRange(*needs.warmth, -1.0f, 1.0f))
	{
		problems.push_back(fmt::format("{}: warmth {} out of range", who, *needs.warmth));
	}
}

bool ValidAnimation(Kind kind, size_t animation)
{
	switch (kind)
	{
	case Kind::PlayAction:
		return animation >= animations::k_FirstAction && animation < animations::k_FirstAction + animations::k_ActionCount;
	case Kind::PlayGesture:
		return animation >= animations::k_FirstGesture && animation < animations::k_FirstGesture + animations::k_GestureCount;
	case Kind::PullFace:
		return animation >= animations::k_FirstFace && animation < animations::k_FirstFace + animations::k_FaceCount;
	default:
		return true;
	}
}

bool ValidOffset(glm::vec2 offset)
{
	return std::abs(offset.x) <= k_MaxOffset && std::abs(offset.y) <= k_MaxOffset;
}

/// A script run names a script, and every object, creature and marker it hands over is the scenario's and on the map.
/// An object that a native call gave back must come from a native call before this command.
bool ScriptArgumentsFit(const Command& command, const Scenario& scenario)
{
	const auto before =
	    std::ranges::find_if(scenario.commands, [&command](const Command& other) { return &other == &command; });
	const auto earlierCalls = static_cast<size_t>(
	    std::count_if(scenario.commands.begin(), before, [](const Command& other) { return other.kind == Kind::CallNative; }));
	const auto fits = [&scenario, &command, earlierCalls](const ScriptArgument& argument) {
		switch (argument.kind)
		{
		case ScriptArgument::Kind::Number:
		case ScriptArgument::Kind::Integer:
		case ScriptArgument::Kind::Result:
		case ScriptArgument::Kind::LastResult:
		case ScriptArgument::Kind::Town:
			return true;
		case ScriptArgument::Kind::Marker:
		case ScriptArgument::Kind::Position:
			return ValidOffset(argument.point);
		case ScriptArgument::Kind::Object:
			return argument.index < scenario.objects.size();
		case ScriptArgument::Kind::Creature:
			return argument.index < scenario.creatures.size();
		case ScriptArgument::Kind::Temple:
			return argument.index < scenario.temples.size();
		case ScriptArgument::Kind::Flock:
			// The flock needs a villager or an animal of the scenario
			return std::ranges::any_of(scenario.objects, [](const ObjectSetup& object) {
				return std::holds_alternative<VillagerInfo>(object.type) || std::holds_alternative<AnimalInfo>(object.type);
			});
		case ScriptArgument::Kind::Made:
			return argument.index < earlierCalls;
		case ScriptArgument::Kind::Text:
			return command.kind == Command::Kind::CallNative && !argument.text.empty();
		}
		return false;
	};
	return !command.script.empty() && std::ranges::all_of(command.scriptArguments, fits) &&
	       std::ranges::all_of(command.scriptGlobals,
	                           [&fits](const auto& global) { return !global.first.empty() && fits(global.second); });
}

/// What is wrong with what a command acts on, if anything: the object it picks up must be of a kind a creature may pick
/// up (a thing, pot, animal, villager, static, tree or magic mushroom), what it knocks down a thing or a tree, the
/// part it strokes, the way it looks something over and the leash put on real ones
std::string_view CommandProblem(const Command& command, std::span<const ObjectSetup> objects)
{
	const auto* object = command.object < objects.size() ? &objects[command.object] : nullptr;
	switch (command.kind)
	{
	case Kind::PickUp:
		return object != nullptr &&
		               (std::holds_alternative<MobileObjectInfo>(object->type) ||
		                std::holds_alternative<AnimalInfo>(object->type) ||
		                std::holds_alternative<VillagerInfo>(object->type) || std::holds_alternative<PotInfo>(object->type) ||
		                std::holds_alternative<MobileStaticInfo>(object->type) ||
		                std::holds_alternative<TreeInfo>(object->type) ||
		                (std::holds_alternative<FeatureInfo>(object->type) &&
		                 std::get<FeatureInfo>(object->type) == FeatureInfo::MagicMushroom))
		           ? ""
		           : "picks up no thing";
	case Kind::KnockDown:
		return object != nullptr && !std::holds_alternative<FeatureInfo>(object->type) ? "" : "knocks down no thing or tree";
	case Kind::TieLeash:
		return object != nullptr ? "" : "ties the leash to nothing";
	case Kind::JoinFootball:
		return object != nullptr && std::holds_alternative<VillagerInfo>(object->type) ? ""
		                                                                               : "sends no villager to the football";
	case Kind::Examine:
		return command.value < creature_object_actions::k_KeepAnimationCount ? "" : "no such way of looking it over";
	case Kind::HandStroke:
		return command.bodyPart < creature_feedback::k_BodyPartCount ? "" : "no such part of the body";
	case Kind::HandSlap:
		return command.slapHeight > 0.0f && command.slapHeight < creature_feedback::k_SlapAbove ? "" : "slaps above it";
	case Kind::PutOnLeash:
		return command.leash == LeashType::Rope || command.leash == LeashType::Evil || command.leash == LeashType::Good
		           ? ""
		           : "no such leash";
	case Kind::ConfineToHome:
		return command.radius > 0.0f ? "" : "keeps it nowhere";
	case Kind::LeashKey:
		return command.value < 3 ? "" : "no such leash key";
	case Kind::HandTapLeash:
		return command.player < PlayerNames::_COUNT ? "" : "no such player";
	case Kind::FightBlow:
		return command.value < 3 && command.chargeMs >= 0.0f && command.chargeMs <= creature_fight::k_MaxChargeMs
		           ? ""
		           : "no such blow or charge";
	case Kind::FightStep:
		return command.value < 4 ? "" : "no such step";
	default:
		return "";
	}
}
} // namespace

bool testbed_scenarios::NeedsNoCreature(Command::Kind kind)
{
	return kind == Kind::SetHour || kind == Kind::RestoreDeviceInDomeFollow || kind == Kind::RestoreDevice ||
	       kind == Kind::PutLoadedArena || kind == Kind::HoldSeed || kind == Kind::DrawGesture || kind == Kind::SummonSeed ||
	       kind == Kind::PressKey || kind == Kind::HandTakeFireBall || kind == Kind::SetAlignment || kind == Kind::WideScreen ||
	       kind == Kind::SetInterfaceLevel || kind == Kind::FlyToBookmark || kind == Kind::TabToTown ||
	       kind == Kind::SetOpenClose || kind == Kind::LayGateStone || kind == Kind::HandTapObject || kind == Kind::Advisor ||
	       kind == Kind::RunText || kind == Kind::PlayHandDemo || kind == Kind::Reframe || kind == Kind::DestroyTemple ||
	       kind == Kind::RunHelpMessage || kind == Kind::EndGuidanceWaits || kind == Kind::SetTownBeliefInRival ||
	       kind == Kind::GiveLandTown || kind == Kind::RecordChallenge || kind == Kind::EnterTemple || kind == Kind::HandTake ||
	       kind == Kind::HandApply || kind == Kind::HandTakeShownSeed || kind == Kind::RunScript || kind == Kind::ClickScroll ||
	       kind == Kind::ClickCreatureAt || kind == Kind::CameraLooksAt || kind == Kind::HandTakeNear ||
	       kind == Kind::ClickCreatureNear || kind == Kind::TapRewardNear || kind == Kind::PlayBoatFilm ||
	       kind == Kind::HandPour || kind == Kind::HandRelease || kind == Kind::SetReportLatency ||
	       kind == Kind::RemoveObject || kind == Kind::ReadTip || kind == Kind::LookAtTempleScroll ||
	       kind == Kind::HandGripTotem || kind == Kind::ProfileFolderCheck || kind == Kind::PlayCustomMusic ||
	       kind == Kind::ShowCustomMusicControls || kind == Kind::OwnTattooPicture || kind == Kind::OpenTattooMenu ||
	       kind == Kind::OpenControlsBox || kind == Kind::OpenMenuPage || kind == Kind::PressMenuKey ||
	       kind == Kind::StopScript || kind == Kind::StartScriptMusic || kind == Kind::SaveGame || kind == Kind::LoadGame ||
	       kind == Kind::QuickSave || kind == Kind::ContinueGame || kind == Kind::PlayLogoEffect ||
	       kind == Kind::PlayFallingSpellFilm || kind == Kind::PlayIntroFilm || kind == Kind::SayLine ||
	       kind == Kind::PlayGuidanceVoice || kind == Kind::ScriptRelease || kind == Kind::FillSiteIcons ||
	       kind == Kind::ChargeIconWithSeedHeld || kind == Kind::TakeIconSeed || kind == Kind::TapSiteIcon ||
	       kind == Kind::TapTempleEntrance || kind == Kind::FinishBuilding || kind == Kind::FillBuiltAmount ||
	       kind == Kind::ScaffoldStartsSite || kind == Kind::DestroyScaffoldSite || kind == Kind::ScaffoldClearsGround ||
	       kind == Kind::CallNative || kind == Kind::PauseGame || kind == Kind::SetGameSpeed ||
	       kind == Kind::RemoveTeleportStone || kind == Kind::MakeDance || kind == Kind::OpenMultiplayerBox ||
	       kind == Kind::BuildWorshipSites || kind == Kind::ScriptBuildsWorshipSites || kind == Kind::PressPlainKey ||
	       kind == Kind::PlainKeyDown || kind == Kind::PlainKeyRepeat || kind == Kind::PlainKeyUp ||
	       kind == Kind::OtherGodHand || kind == Kind::ObserverCamera || kind == Kind::OwnGod || kind == Kind::JoinOnline ||
	       kind == Kind::EndBox || kind == Kind::BringCreature || kind == Kind::TattooEditorAsNetworkGame ||
	       kind == Kind::ShowDemoPing || kind == Kind::AddIncomingMessage || kind == Kind::Chat ||
	       kind == Kind::ComputerPersonality || kind == Kind::ComputerUntaughtSends || kind == Kind::SkirmishBox ||
	       kind == Kind::LookAtSaveGamePicture || kind == Kind::HandCastOnDispenser || kind == Kind::HandCastOnCreature ||
	       kind == Kind::CloseHeldMiracle || kind == Kind::JoinFootball || IsPointerCommand(kind);
}

std::string_view testbed_scenarios::Name(Facet facet)
{
	constexpr std::array<std::string_view, k_FacetCount> k_Names {
	    "Idle",       "Expressions", "Senses",    "Needs",         "Growth",      "Appearance", "Light",   "Movement",
	    "Footprints", "Audio",       "Objects",   "Hand",          "Leash",       "Combat",     "Mind",    "Particles",
	    "Editor",     "Miracles",    "Benchmark", "Creature Mode", "Physics",     "Nature",     "Animals", "Sky",
	    "Advisors",   "Story",       "Tutorial",  "Interface",     "Multiplayer",
	};
	return k_Names.at(static_cast<size_t>(facet));
}

std::string_view testbed_scenarios::Name(Weather weather)
{
	constexpr std::array<std::string_view, 5> k_Names {"clear", "rain", "thunderstorm", "snow", "blizzard"};
	return k_Names.at(static_cast<size_t>(weather));
}

std::string_view testbed_scenarios::Name(Shot shot)
{
	constexpr std::array<std::string_view, 5> k_Names {"testbed", "overview", "follow", "head", "placed"};
	return k_Names.at(static_cast<size_t>(shot));
}

std::string_view testbed_scenarios::Name(Command::Kind kind)
{
	constexpr std::array<std::string_view, 231> k_Names {
	    "walk to",
	    "run to",
	    "follow",
	    "flee from",
	    "turn to face",
	    "face the camera",
	    "stop",
	    "play action",
	    "gesture",
	    "pull face",
	    "draw a gesture",
	    "show feeling",
	    "sit down",
	    "stand up",
	    "sleep",
	    "wake",
	    "eat",
	    "drink",
	    "poo",
	    "fart",
	    "puke",
	    "faint",
	    "stroke",
	    "slap",
	    "set hour",
	    "pick up",
	    "put down",
	    "toss away",
	    "eat it",
	    "look it over",
	    "throw at",
	    "knock down",
	    "point at",
	    "hand strokes",
	    "hand slaps",
	    "hand lets go",
	    "hand takes hold",
	    "put on leash",
	    "tie leash to",
	    "tie leash to desire flag",
	    "tie leash to worship site flag",
	    "untie leash",
	    "take off leash",
	    "keep at home",
	    "make leashable",
	    "hand taps to leash",
	    "leash key",
	    "shake the hand",
	    "order to a point",
	    "order on the object",
	    "hang the temple's leashes",
	    "tap a temple leash",
	    "tap a building with the leash",
	    "start fight",
	    "fight blow",
	    "fight block",
	    "fight step",
	    "fight special",
	    "fight by itself",
	    "fight miracle",
	    "show the fight",
	    "pointer on opponent",
	    "pointer on the cave creature",
	    "fight autopilot",
	    "fight lesson check",
	    "knock out",
	    "bring round",
	    "abort fight",
	    "tie leash to creature",
	    "set desire",
	    "set stage",
	    "reward if",
	    "see skill",
	    "see miracle",
	    "player did",
	    "player did it to the object",
	    "press C",
	    "double click",
	    "camera keys",
	    "clear the view",
	    "give the camera back",
	    "saved follow camera",
	    "saved turning camera",
	    "saved free camera",
	    "saved camera below the land",
	    "press F5",
	    "tattoo",
	    "take tattoo off",
	    "open tattoo editor",
	    "hold seed",
	    "draw gesture",
	    "summon seed",
	    "know miracle",
	    "cast miracle",
	    "press key",
	    "hand takes fireball",
	    "pointer to",
	    "press button",
	    "let go of button",
	    "move mouse",
	    "turn wheel",
	    "set alignment",
	    "open or close",
	    "lay gate stone",
	    "cinema bars",
	    "set interface level",
	    "set fight lean",
	    "set miracle sightings",
	    "hand tap",
	    "tell an advisor",
	    "walk home",
	    "run a help message",
	    "end the guidance's waits",
	    "set town belief in rival",
	    "destroy temple",
	    "script play",
	    "run text",
	    "play hand demo",
	    "reframe",
	    "hand take",
	    "hand give",
	    "hand take shown seed",
	    "hand apply",
	    "swap minds",
	    "record challenge",
	    "enter temple",
	    "play boat film",
	    "run script",
	    "hand release",
	    "hand pour",
	    "do action",
	    "make friends",
	    "only desire",
	    "remove object",
	    "read tip",
	    "look at temple scroll",
	    "look at a save game picture",
	    "set alight",
	    "hand grips totem",
	    "pointer on altar",
	    "pointer on rope",
	    "pointer on object",
	    "set fizz",
	    "check the profile folder",
	    "give the player a picture of their own",
	    "open the tattoo editor from the menu",
	    "open the box of the controls page",
	    "open a page of the menu",
	    "press a key in the menu",
	    "play custom music",
	    "show the custom music controls",
	    "save the game",
	    "load the game",
	    "script reaction",
	    "script place",
	    "stop script",
	    "pointer on creature",
	    "start script music",
	    "write the web page",
	    "fly to a bookmark",
	    "press the Tab key",
	    "keep the creature in a bookmark and follow it",
	    "play the falling spell film",
	    "play the intro film",
	    "give hand glows",
	    "give a creed power",
	    "attach a script sound",
	    "attach a script sound to the object",
	    "say a line",
	    "play a guidance voice",
	    "stop at once",
	    "turn to face when still",
	    "script release",
	    "fill the worship site icons",
	    "quick save",
	    "continue the game",
	    "play the logo effect",
	    "charge an icon with its seed in the hand",
	    "finish the building",
	    "fill the built amount",
	    "start a site with the scaffold",
	    "destroy the site of the scaffold",
	    "clear the ground of the scaffold",
	    "call a script native",
	    "animal flees",
	    "hold the route search",
	    "pause the game",
	    "set the game's speed",
	    "force an action",
	    "remove the teleport stone",
	    "make a dance",
	    "open a multiplayer box",
	    "build the worship sites",
	    "press a key on the keyboard",
	    "show another god's hand",
	    "show an observer's camera",
	    "be the god of a player at this computer",
	    "press Join Online Game of the page",
	    "end box",
	    "bring a creature into a network game",
	    "give the hand's reports a latency",
	    "tattoo editor as in a network game",
	    "show a made-up ping",
	    "add an incoming message",
	    "chat",
	    "computer personality",
	    "computer sends with no lesson",
	    "skirmish box",
	    "put a key down",
	    "repeat a held key",
	    "let go of a key",
	    "a god orders its creature",
	    "take a seed from its icon",
	    "tap a worship site icon",
	    "change the body's look",
	    "tap the temple's entrance",
	    "give a town of the land to the player",
	    "cast the held miracle on a dispenser",
	    "cast the held miracle on a creature",
	    "close the held miracle and let go",
	    "click a scroll",
	    "put the camera to look at a point",
	    "join the football of the town",
	    "a script builds the worship sites",
	    "take the mobile object nearest a point into the hand",
	    "click the creature nearest a point",
	    "tap the reward chest nearest a point",
	    "click a creature",
	    "make the graphics device again while the dome follows the sky",
	    "make the graphics device again",
	    "put an arena of a loaded file between two creatures",
	};
	return k_Names.at(static_cast<size_t>(kind));
}

bool testbed_scenarios::IsPointerCommand(Command::Kind kind)
{
	return kind == Kind::PointerTo || kind == Kind::PointerPress || kind == Kind::PointerRelease ||
	       kind == Kind::PointerSweep || kind == Kind::WheelTurn || kind == Kind::PointerOnOpponent ||
	       kind == Kind::PointerOnCaveCreature || kind == Kind::PointerOnCreature || kind == Kind::PointerOnAltar ||
	       kind == Kind::PointerOnRope || kind == Kind::PointerOnObject;
}

bool NeedOverrides::Empty() const
{
	return !energy && !exhaustion && !dehydration && !poo && !life && !warmth && !age;
}

std::span<const Scenario> testbed_scenarios::All()
{
	static const std::vector<Scenario> k_All = Build();
	return k_All;
}

const Scenario* testbed_scenarios::Find(std::string_view id)
{
	const auto all = All();
	const auto found = std::ranges::find(all, id, &Scenario::id);
	return found != all.end() ? &*found : nullptr;
}

std::vector<std::string> testbed_scenarios::Problems(const Scenario& scenario)
{
	std::vector<std::string> problems;
	if (scenario.id.empty() || scenario.name.empty() || scenario.description.empty() || scenario.expected.empty())
	{
		problems.emplace_back("missing its id, name, description or what to expect");
	}
	if (static_cast<size_t>(scenario.facet) >= k_FacetCount)
	{
		problems.emplace_back("no such facet");
	}
	const auto& environment = scenario.environment;
	if (!InRange(environment.hour, 0.0f, k_HoursPerDay) || environment.bodyTimeScale < 0.0f)
	{
		problems.emplace_back("hour or body time out of range");
	}
	// A land other than the testbed brings its own things
	if (environment.land == Land::Testbed && scenario.creatures.empty() && scenario.particles.empty() &&
	    scenario.miracles.empty() && scenario.dispensers.empty() && scenario.birdFlocks.empty() && scenario.temples.empty() &&
	    !environment.dispenserGrid && !scenario.crowd.has_value() && !environment.playerAlignment.has_value() &&
	    scenario.throws.empty() && scenario.objects.empty() && scenario.vortices.empty() && scenario.rewards.empty() &&
	    scenario.fireflyRewards.empty() && scenario.landStorms.empty() && scenario.highlights.empty() &&
	    scenario.landScript.empty() &&
	    std::ranges::none_of(scenario.commands, [](const Command& command) { return NeedsNoCreature(command.kind); }))
	{
		problems.emplace_back("no creatures, things, particles, miracles, dispensers, birds, temples, vortices, reward "
		                      "chests, crowd, fireflies' rewards, land storms, scrolls or signs, land script lines, player's "
		                      "commands or alignment for the "
		                      "hand");
	}
	for (const auto& storm : scenario.landStorms)
	{
		if (!ValidOffset(storm.offset) || !ValidOffset(storm.destination) || storm.age < 0.0f || storm.cloudCount < 0)
		{
			problems.emplace_back("a land storm off the map, or with a negative age or cloud count");
		}
	}
	if ((environment.playerAlignment && !InRange(*environment.playerAlignment, -1.0f, 1.0f)) ||
	    (environment.cursor && (!InRange(environment.cursor->x, 0.0f, 1.0f) || !InRange(environment.cursor->y, 0.0f, 1.0f))))
	{
		problems.emplace_back("alignment or cursor out of range");
	}
	if (scenario.crowd.has_value() && (scenario.crowd->count == 0 || scenario.crowd->perFrame == 0))
	{
		problems.emplace_back("a crowd of no one, or spawned none a frame");
	}
	for (const auto& miracle : scenario.miracles)
	{
		const auto type = static_cast<size_t>(miracle.type);
		if (type == 0 || type >= k_Miracles)
		{
			problems.emplace_back("a miracle of no such magic type");
		}
		if (miracle.target == MiracleCast::Target::Creature && miracle.creature >= scenario.creatures.size())
		{
			problems.emplace_back("a miracle cast on a creature that isn't there");
		}
		if (!ValidOffset(miracle.point) || !ValidOffset(miracle.handOffset) || miracle.delaySeconds < 0.0f ||
		    miracle.holdSeconds.value_or(0.0f) < 0.0f || miracle.repeatSeconds.value_or(1.0f) <= 0.0f)
		{
			problems.emplace_back("a miracle's points or times out of range");
		}
	}
	for (const auto& dispenser : scenario.dispensers)
	{
		const auto type = static_cast<size_t>(dispenser.type);
		if (type == 0 || type >= k_Miracles || !ValidOffset(dispenser.offset))
		{
			problems.emplace_back("a dispenser of no such magic type, or off the map");
		}
	}
	for (const auto& particle : scenario.particles)
	{
		if (particle.file.empty() && particles::ParticleTypeFile(particle.type).empty())
		{
			problems.emplace_back("a particle type that has no file");
		}
		if (particle.targetsCreatures && scenario.creatures.empty())
		{
			problems.emplace_back("a particle effect given creatures to act on, without creatures");
		}
		if (particle.magnitude < 0.0f || particle.restartSeconds < 0.0f)
		{
			problems.emplace_back("a particle effect's magnitude or restart out of range");
		}
	}
	const auto creatures = scenario.creatures.size();
	// The commands can also name the creatures of the filled slots: one for each temple at most
	const auto others = creatures + (scenario.fillEmptySlots ? scenario.temples.size() : 0);
	const auto& framing = scenario.framing;
	if ((framing.shot == Shot::Follow || framing.shot == Shot::Head) && framing.creature >= creatures)
	{
		problems.emplace_back("the camera frames a creature that isn't there");
	}
	if (framing.distance <= 0.0f || !std::ranges::all_of(framing.include, ValidOffset))
	{
		problems.emplace_back("the framing's distance or points are out of range");
	}
	for (size_t i = 0; i < creatures; ++i)
	{
		const auto& creature = scenario.creatures.at(i);
		const auto who = fmt::format("creature {}", i);
		const auto species = static_cast<int32_t>(creature.species);
		if (species < 1 || species >= static_cast<int32_t>(CreatureType::_COUNT))
		{
			problems.push_back(fmt::format("{}: no such species", who));
		}
		if (static_cast<size_t>(creature.owner) >= static_cast<size_t>(PlayerNames::_COUNT))
		{
			problems.push_back(fmt::format("{}: no such owner", who));
		}
		if (!ValidOffset(creature.offset))
		{
			problems.push_back(fmt::format("{}: off the map", who));
		}
		if ((creature.alignment && !InRange(*creature.alignment, -1.0f, 1.0f)) ||
		    (creature.fatness && !InRange(*creature.fatness, 0.0f, 1.0f)) ||
		    (creature.strength && !InRange(*creature.strength, 0.0f, 1.0f)) ||
		    (creature.size && !InRange(*creature.size, 0.05f, 4.0f)))
		{
			problems.push_back(fmt::format("{}: body out of range", who));
		}
		if (!creature.mindFile.empty() && !creature.mindFile.starts_with("chosen:") && !creature.mindFile.starts_with("game:"))
		{
			problems.push_back(fmt::format("{}: a mind file is chosen: or game:", who));
		}
		if (creature.phase && *creature.phase > k_LastPhase)
		{
			problems.push_back(fmt::format("{}: no such stage of growing up", who));
		}
		CheckNeeds(creature.needs, problems, who);
		for (const auto& desire : creature.desires)
		{
			if (static_cast<size_t>(desire.desire) >= creature_desires::k_DesireCount || !InRange(desire.fraction, 0.0f, 1.0f))
			{
				problems.push_back(fmt::format("{}: desire out of range", who));
			}
		}
		for (const auto& tattoo : creature.tattoos)
		{
			if (tattoo.design >= creature_tattoo::k_DesignCount || tattoo.Empty())
			{
				problems.push_back(fmt::format("{}: no such tattoo design or site", who));
			}
		}
		if (creature.tattoos.size() > creature_tattoo::k_SlotCount)
		{
			problems.push_back(fmt::format("{}: more tattoos than slots", who));
		}
		for (const auto& mark : creature.wounds)
		{
			if (mark.type >= creature_marks::k_WoundLifetimes.size() || mark.column >= 8 || mark.skin >= 4)
			{
				problems.push_back(fmt::format("{}: no such wound", who));
			}
		}
	}
	for (const auto& object : scenario.objects)
	{
		const bool valid = std::visit(
		    []<typename T>(T type) {
			    const auto index = static_cast<int32_t>(type);
			    return index >= 0 && index < static_cast<int32_t>(T::_COUNT);
		    },
		    object.type);
		if (!valid || object.scale <= 0.0f || !ValidOffset(object.offset))
		{
			problems.emplace_back("an object of no such kind, size or place");
		}
		// The model of this kind of food pile is the grain at the side of a worship site. It lies about 50 m from the
		// point where the pile is, so you do not see the pile there. Use a different kind of food pile.
		if (const auto* pot = std::get_if<PotInfo>(&object.type); pot != nullptr && *pot == PotInfo::FoodPile)
		{
			problems.emplace_back("a food pile of the kind whose model is far from its point");
		}
		// On a land of the story, the scenario's town is a town of the testbed, not a town of the land. Name the land's
		// town with landTown. Only a land of the story has towns of its own.
		const bool storyLand = scenario.environment.land == Land::Story;
		if (object.joinTown && storyLand)
		{
			problems.emplace_back("an object that joins the scenario's town on a land of the story; use landTown");
		}
		if (object.landTown.has_value() && !storyLand)
		{
			problems.emplace_back("an object with a town of the land on a land that is not of the story");
		}
	}
	for (const auto& footpath : scenario.footpaths)
	{
		const auto outOfRange = [&scenario](size_t object) { return object >= scenario.objects.size(); };
		if ((footpath.startsAt.has_value() && outOfRange(*footpath.startsAt)) ||
		    std::ranges::any_of(footpath.leadsFrom, outOfRange) ||
		    !std::ranges::all_of(footpath.nodes, [](glm::vec2 node) { return ValidOffset(node); }) || footpath.nodes.empty() ||
		    footpath.leadsFrom.empty())
		{
			problems.emplace_back("a footpath with no nodes, a node off the map or an object that isn't there");
		}
	}
	for (const auto& throwing : scenario.throws)
	{
		const auto things = throwing.reward ? scenario.rewards.size() : scenario.objects.size();
		if (throwing.object >= things || (throwing.from.has_value() && !ValidOffset(*throwing.from)) || throwing.height < 0.0f)
		{
			problems.emplace_back("a throw of no such object, or from no such place");
		}
	}
	for (size_t i = 0; i < scenario.commands.size(); ++i)
	{
		const auto& command = scenario.commands.at(i);
		const auto what = fmt::format("command {} ({})", i, Name(command.kind));
		// An object set alight (value 1 or 2) needs no creature, but the object must be there
		const bool objectAlight = command.kind == Kind::SetAlight && (command.value == 1 || command.value == 2);
		if (objectAlight && command.object >= scenario.objects.size())
		{
			problems.push_back(fmt::format("{}: no such object", what));
		}
		if (!NeedsNoCreature(command.kind) && !objectAlight && command.creature >= creatures)
		{
			problems.push_back(fmt::format("{}: no such creature", what));
		}
		if (command.kind == Kind::SetAlignment && !InRange(command.alignment, -1.0f, 1.0f))
		{
			problems.push_back(fmt::format("{}: alignment out of range", what));
		}
		if ((command.kind == Kind::SetOpenClose && command.object >= scenario.objects.size()) ||
		    (command.kind == Kind::LayGateStone &&
		     (command.object >= scenario.objects.size() || command.value >= scenario.objects.size())))
		{
			problems.push_back(fmt::format("{}: no such object", what));
		}
		if ((command.kind == Kind::Follow || command.kind == Kind::StartFight || command.kind == Kind::TieLeashToCreature ||
		     command.kind == Kind::MakeFriends) &&
		    (command.value >= others || command.value == command.creature))
		{
			problems.push_back(fmt::format("{}: acts on no other creature", what));
		}
		if (!ValidAnimation(command.kind, command.value))
		{
			problems.push_back(fmt::format("{}: animation {} isn't of its kind", what, command.value));
		}
		if (const auto problem = CommandProblem(command, scenario.objects); !problem.empty())
		{
			problems.push_back(fmt::format("{}: {}", what, problem));
		}
		if ((command.kind == Kind::RunScript || command.kind == Kind::CallNative) && !ScriptArgumentsFit(command, scenario))
		{
			problems.push_back(fmt::format("{}: runs no script, or hands it what the scenario hasn't", what));
		}
		const bool offMapOrder = command.kind == Kind::LeashOrderAt && std::abs(command.point.x) <= k_MaxOrderOffset &&
		                         std::abs(command.point.y) <= k_MaxOrderOffset;
		if ((!ValidOffset(command.point) && !offMapOrder) || command.delaySeconds < 0.0f ||
		    !InRange(command.hour, 0.0f, k_HoursPerDay))
		{
			problems.push_back(fmt::format("{}: point, delay or hour out of range", what));
		}
		if ((command.kind == Kind::SetDesire &&
		     (command.value >= creature_desires::k_DesireCount || !InRange(command.amount, 0.0f, 1.0f))) ||
		    (command.kind == Kind::SetPhase && command.value > k_LastPhase) ||
		    (command.kind == Kind::SetFightLean && !InRange(command.amount, -1.0f, 1.0f)) ||
		    (command.kind == Kind::SetMiracleSightings && (command.value >= k_Miracles || command.amount < 0.0f)) ||
		    (command.kind == Kind::ShowFeeling && command.value >= creature_face::k_CueCount) ||
		    (command.kind == Kind::SeeSkill && command.value >= k_Skills) ||
		    ((command.kind == Kind::SeeMiracle || command.kind == Kind::KnowMiracle || command.kind == Kind::CastMiracle ||
		      command.kind == Kind::FightSpell) &&
		     command.value >= k_Miracles) ||
		    (command.kind == Kind::CastMiracle &&
		     (command.atCreature.has_value() ? *command.atCreature >= scenario.creatures.size()
		                                     : command.object >= scenario.objects.size())) ||
		    ((command.kind == Kind::PlayerDid || command.kind == Kind::PlayerDidOn) && command.value >= k_Deeds) ||
		    (command.kind == Kind::CameraKeys && (command.value >= 4 || command.amount <= 0.0f)) ||
		    (command.kind == Kind::PointerTo &&
		     (!InRange(command.point.x, 0.0f, 1.0f) || !InRange(command.point.y, 0.0f, 1.0f))) ||
		    ((command.kind == Kind::PointerPress || command.kind == Kind::PointerRelease) &&
		     (command.value < 1 || command.value > 3)) ||
		    (command.kind == Kind::PointerSweep && command.amount <= 0.0f) ||
		    (command.kind == Kind::OpenCreatureCave && command.value >= creature_cave::k_PageCount) ||
		    ((command.kind == Kind::ApplyTattoo || command.kind == Kind::RemoveTattoo) &&
		     (command.value >= creature_tattoo::k_DesignCount || command.bodyPart >= creature_tattoo::k_SlotCount)) ||
		    ((command.kind == Kind::HoldSeed || command.kind == Kind::SummonSeed) && command.value >= k_SeedCount) ||
		    (command.kind == Kind::DrawGesture && (command.value == 0 || command.value > k_LastGesture)))
		{
			problems.push_back(fmt::format("{}: value {} out of range", what, command.value));
		}
	}
	if (scenario.repeatFrom.has_value() && *scenario.repeatFrom >= scenario.commands.size())
	{
		problems.emplace_back("repeats from a command that isn't there");
	}
	return problems;
}

glm::vec2 testbed_scenarios::MapPoint(glm::vec2 middle, glm::vec2 offset)
{
	return middle + offset;
}

std::optional<uint32_t> testbed_scenarios::HelpTextNumber(std::span<const std::string> names, std::string_view name)
{
	const auto found = std::ranges::find(names, name);
	if (found == names.end())
	{
		return std::nullopt;
	}
	return static_cast<uint32_t>(std::distance(names.begin(), found));
}

float testbed_scenarios::CreatureHeight(float size)
{
	// A creature of size 1 stands about 15 units tall, whatever its species' mesh
	constexpr float k_HeightPerSize = 15.0f;
	return k_HeightPerSize * std::max(size, 0.05f);
}

CameraPlacement testbed_scenarios::Overview(glm::vec3 centre, glm::vec2 halfSize, glm::vec2 fieldOfView, float distanceFactor)
{
	// Looking down from the south at this angle, far enough back for the box's width to fit across the view and its
	// depth, foreshortened, up and down it, with a margin round it for the creatures' heights
	constexpr float k_Pitch = 0.65f;
	constexpr float k_MinDistance = 40.0f;
	constexpr float k_Margin = 25.0f;
	const auto halfAngles = glm::clamp(fieldOfView * 0.5f, 0.1f, 1.5f);
	// The near side of the box is closer to the camera than its middle, so its width takes a wider angle
	const auto across =
	    ((halfSize.x + k_Margin) / portable_math::Tan(halfAngles.x)) + ((halfSize.y + k_Margin) * portable_math::Cos(k_Pitch));
	const auto upAndDown = (halfSize.y + k_Margin) * portable_math::Sin(k_Pitch) / portable_math::Tan(halfAngles.y);
	const auto distance = distanceFactor * std::max({across, upAndDown, k_MinDistance});
	return {
	    .origin = centre + glm::vec3(0.0f, distance * portable_math::Sin(k_Pitch), -distance * portable_math::Cos(k_Pitch)),
	    .focus = centre,
	};
}

CameraPlacement testbed_scenarios::Follow(glm::vec3 position, float height, float distanceFactor)
{
	constexpr float k_MinDistance = 25.0f;
	const glm::vec3 focus = position + glm::vec3(0.0f, height * 0.5f, 0.0f);
	const auto distance = distanceFactor * std::max(height * 3.5f, k_MinDistance);
	return {.origin = focus + glm::vec3(0.0f, distance * 0.45f, -distance * 0.9f), .focus = focus};
}

CameraPlacement testbed_scenarios::Head(glm::vec3 position, glm::vec2 ahead, float height, float distanceFactor)
{
	const auto length = glm::length(ahead);
	const auto forward = length > 0.0f ? ahead / length : glm::vec2(0.0f, -1.0f);
	const glm::vec3 forward3 {forward.x, 0.0f, forward.y};
	// The head is high on the body and a little forward of its middle
	const glm::vec3 focus = position + glm::vec3(0.0f, height * 0.75f, 0.0f) + (forward3 * (height * 0.3f));
	const auto distance = distanceFactor * height;
	return {.origin = focus + (forward3 * distance) + glm::vec3(0.0f, height * 0.15f, 0.0f), .focus = focus};
}

Bounds testbed_scenarios::BoundsOf(std::span<const glm::vec2> points, glm::vec2 minimumHalfSize)
{
	if (points.empty())
	{
		return {.centre = glm::vec2(0.0f), .halfSize = minimumHalfSize};
	}
	auto low = points.front();
	auto high = points.front();
	for (const auto& point : points)
	{
		low = glm::min(low, point);
		high = glm::max(high, point);
	}
	return {.centre = (low + high) * 0.5f, .halfSize = glm::max((high - low) * 0.5f, minimumHalfSize)};
}

void testbed_scenarios::Apply(const NeedOverrides& overrides, creature_physiology::Needs& needs, float size)
{
	const auto set = [](float& value, const std::optional<float>& wanted, float low, float high) {
		if (wanted.has_value())
		{
			value = std::clamp(*wanted, low, high);
		}
	};
	set(needs.energy, overrides.energy, 0.0f, std::max(1.0f, size));
	set(needs.exhaustion, overrides.exhaustion, 0.0f, 1.0f);
	set(needs.dehydration, overrides.dehydration, 0.0f, 1.0f);
	set(needs.poo, overrides.poo, 0.0f, 1.0f);
	set(needs.life, overrides.life, 0.0f, 1.0f);
	set(needs.warmth, overrides.warmth, -1.0f, 1.0f);
	if (overrides.age.has_value())
	{
		needs.age = *overrides.age;
	}
}

void testbed_scenarios::Apply(std::span<const DesireOverride> overrides, creature_desires::Desires& desires)
{
	for (const auto& override : overrides)
	{
		auto& state = desires[override.desire];
		state.activated = true;
		state.value = std::clamp(override.fraction, 0.0f, 1.0f) * std::max(state.max, 0.0f);
	}
}

std::vector<size_t> testbed_scenarios::GivesAt(const std::map<uint32_t, std::vector<size_t>>& gives, uint32_t turn,
                                               size_t commands)
{
	std::vector<size_t> given;
	if (const auto found = gives.find(turn); found != gives.end())
	{
		std::ranges::copy_if(found->second, std::back_inserter(given), [commands](size_t place) { return place < commands; });
	}
	return given;
}

std::vector<size_t> testbed_scenarios::Advance(Timeline& timeline, std::span<const Command> commands,
                                               std::optional<size_t> repeatFrom, float seconds,
                                               const std::function<bool(const Command& command)>& isFree)
{
	std::vector<size_t> due;
	if (commands.empty() || timeline.next >= commands.size())
	{
		timeline.done = true;
	}
	if (timeline.done)
	{
		return due;
	}
	timeline.seconds += seconds;
	timeline.sinceGiven += seconds;
	// At most a round of the commands at once, so commands that go round again without waiting can't run away
	while (!timeline.done && due.size() < commands.size())
	{
		const auto& command = commands[timeline.next];
		if (command.waitUntilFree && !timeline.freed)
		{
			if (timeline.sinceGiven < k_SettleSeconds || !isFree(command))
			{
				// Its delay counts from when the creature is free
				timeline.seconds = 0.0f;
				break;
			}
			timeline.freed = true;
		}
		if (timeline.seconds < command.delaySeconds)
		{
			break;
		}
		due.push_back(timeline.next);
		timeline.seconds = 0.0f;
		timeline.sinceGiven = 0.0f;
		timeline.freed = false;
		if (++timeline.next >= commands.size())
		{
			if (repeatFrom.has_value() && *repeatFrom < commands.size())
			{
				timeline.next = *repeatFrom;
			}
			else
			{
				timeline.done = true;
			}
		}
	}
	return due;
}

bool testbed_scenarios::KeepsDispenserGrid(const Scenario& scenario)
{
	if (!scenario.environment.dispenserGrid)
	{
		return false;
	}
	const auto onGrid = [](const auto& setup) { return testbed_dispensers::InGridArea(setup.offset); };
	return std::ranges::none_of(scenario.creatures, onGrid) && std::ranges::none_of(scenario.objects, onGrid);
}
