/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// Testbed scenarios for the scripts of Land 1 and the tasks that they release.

#include "3D/TempleInteriorInterface.h"
#include "Creature/CreatureCave.h"
#include "Creature/CreatureFight.h"
#include "ECS/IncomingMessageRules.h"
#include "TestbedScenarioBuilders.h"
#include "TestbedScenarioRegistry.h"

using namespace openblack;
using namespace openblack::testbed_scenarios;

/// A task of a land's script that stops gives back the objects that its variables hold. The objects stay in a script
/// until the scripts' turn ends, and then they are in no script
void testbed_scenarios::AddScriptTaskRelease(std::vector<Scenario>& all)
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
	    .id = "interface.world_text_glyphs",
	    .name = "The letters of the temple's signs, read from the font as the game reads them",
	    .facet = Facet::Interface,
	    .description = "The player has a temple and is put in its main room. Look at the signs over the doors.",
	    .expected = "Each letter of a sign is read from the font from one and a half texels before its first column to "
	                "the middle of its last column, so the letters sit a little to the right in their quads and their "
	                "right edges are a little thinner than the left edges.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview},
	    .temples = {{.offset = {-300.0f, 100.0f}, .angle = 36.0f}},
	    .commands = {{.kind = Kind::EnterTemple, .delaySeconds = 2.0f, .value = 3}},
	});

	all.push_back({
	    .id = "interface.screen_text_glyphs",
	    .name = "The letters of the text in a box on the screen, read from the font as the game reads them",
	    .facet = Facet::Interface,
	    .description = "A script shows a help text in the dialogue box. Look at the letters of the text.",
	    .expected = "Each letter of text under 26 pixels high comes from the small glyphs, a quarter of the full height, "
	                "so it is softer than before. Each letter is two units wider than its bitmap and starts at its left "
	                "bearing, so it sits one unit to the right.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview},
	    .commands = {{.kind = Kind::RunText, .delaySeconds = 0.5f, .value = 1250}},
	});

	all.push_back({
	    .id = "interface.screen_quad_pixels",
	    .name = "The boxes, buttons and text of a box on the screen, on the game's pixel centres",
	    .facet = Facet::Interface,
	    .description = "After a second the land information box of Two Gods shows. Look at the edges of its band, its "
	                   "buttons and its letters.",
	    .expected = "Each picture of the box reads its texture at the same points as the game. The band, the buttons, "
	                "the Back arrow and the pointer are one pixel further right and down than before. The letters read "
	                "half a texel further back, so they are a little softer.",
	    .framing = {.shot = Shot::Overview},
	    .commands = {{.kind = Kind::SkirmishBox, .delaySeconds = 1.0f, .value = 3}},
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
void testbed_scenarios::AddLandOneScriptStubScenarios(std::vector<Scenario>& all)
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
