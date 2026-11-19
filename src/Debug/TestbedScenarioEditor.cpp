/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// Testbed scenarios for the land editor.

#include <vector>

#include <SDL_scancode.h>

#include "3D/TempleRoom.h"
#include "Creature/CreatureCave.h"
#include "Creature/CreatureFight.h"
#include "Input/PlatformKeys.h"
#include "TestbedScenarioBuilders.h"
#include "TestbedScenarioRegistry.h"

using namespace openblack;
using namespace openblack::testbed_scenarios;

namespace
{
/// The object of a command that loads a generated land: the type of the land plus 1
constexpr size_t k_StoryIsland = 1;
constexpr size_t k_BigIsland = 2;
constexpr size_t k_Highlands = 3;
constexpr size_t k_Valleys = 4;
constexpr size_t k_Archipelago = 5;

/// The player flies the camera in the temple's main room and then in its save game room, with the editor's cheat of
/// the free camera on or off. The same presses go to the game in both scenarios.
std::vector<Command> TempleFreeCameraFlight(bool cheatOn)
{
	using Kind = Command::Kind;
	const auto key = [](Kind kind, SDL_Scancode code, float delay) {
		return Command {.kind = kind, .delaySeconds = delay, .value = static_cast<size_t>(code)};
	};
	constexpr size_t k_Middle = 2;
	std::vector<Command> commands {
	    {.kind = Kind::TempleFreeCamera, .delaySeconds = 0.5f, .value = cheatOn ? 1u : 0u},
	    {.kind = Kind::EnterTemple, .delaySeconds = 0.5f, .value = static_cast<size_t>(TempleRoom::Main)},
	};
	const auto flight = [&](float delay) {
		commands.push_back({.kind = Kind::PointerTo, .delaySeconds = delay, .waitUntilFree = true, .point = {0.5f, 0.5f}});
		commands.push_back({.kind = Kind::PointerPress, .delaySeconds = 0.2f, .value = k_Middle});
		commands.push_back({.kind = Kind::PointerSweep, .delaySeconds = 0.1f, .point = {0.2f, -0.1f}, .amount = 1.0f});
		commands.push_back({.kind = Kind::PointerRelease, .delaySeconds = 1.2f, .value = k_Middle});
		commands.push_back(key(Kind::PlainKeyDown, SDL_SCANCODE_UP, 0.3f));
		commands.push_back(key(Kind::PlainKeyUp, SDL_SCANCODE_UP, 1.5f));
		commands.push_back(key(Kind::PlainKeyDown, SDL_SCANCODE_LEFT, 0.3f));
		commands.push_back(key(Kind::PlainKeyUp, SDL_SCANCODE_LEFT, 1.0f));
	};
	flight(8.0f);
	commands.push_back({.kind = Kind::EnterTemple, .delaySeconds = 3.0f, .value = static_cast<size_t>(TempleRoom::SaveGame)});
	flight(3.0f);
	return commands;
}

} // namespace

void testbed_scenarios::AddEditor(std::vector<Scenario>& all)
{
	all.push_back({
	    .id = "editor.temple_free_camera",
	    .name = "The editor's free camera flies through the temple's rooms",
	    .facet = Facet::Editor,
	    .description = "The editor's cheat of the free camera goes on, and the player goes into the temple's main room. "
	                   "When the camera has the player, the middle button turns the view right and up for a second. "
	                   "Then the up arrow key moves the camera forwards and the left arrow key moves it left. Then the "
	                   "player goes into the save game room, and the same presses come again.",
	    .expected = "In each room, the camera comes in along the room's path as usual. Then the view turns right and up "
	                "about the place of the camera, and the camera flies forwards along the view and to the left. The "
	                "room does not turn, and no door opens.",
	    .framing = {.shot = Shot::Overview},
	    .commands = TempleFreeCameraFlight(true),
	});

	all.push_back({
	    .id = "editor.temple_free_camera_off",
	    .name = "With the editor's free camera off, the temple's camera is as usual",
	    .facet = Facet::Editor,
	    .description = "The same presses as editor.temple_free_camera, with the editor's cheat of the free camera off.",
	    .expected = "The middle button does nothing in the temple. The up arrow key leans the main room's view and the "
	                "left arrow key turns the room, as with no cheat. In the save game room, the keys turn the room and "
	                "move the camera up and down.",
	    .framing = {.shot = Shot::Overview},
	    .commands = TempleFreeCameraFlight(false),
	});

	all.push_back({
	    .id = "editor.one_of_each",
	    .name = "One of each kind to pick",
	    .facet = Facet::Editor,
	    .description = "A tiger, a villager, an oak, a pillar of rock and a ball, spread out in view, for trying the "
	                   "editor (F2) on: the outliner lists each under its kind, a click on the land or in the list "
	                   "picks one, and the inspector shows it.",
	    .expected = "Each picks with a box drawn round it. The Move (E) and Rotate (R) tools drag the picked thing over "
	                "the land and turn it; Ctrl+D copies it beside itself and Delete removes it. The tiger's inspector "
	                "has every creature section.",
	    .environment = {.clockRuns = false},
	    .framing = {.shot = Shot::Overview, .include = {{-40.0f, 30.0f}, {40.0f, -30.0f}}},
	    .creatures = {Posed(CreatureType::Tiger, {0.0f, 0.0f})},
	    .objects = {{.type = VillagerInfo::CelticFarmerMale, .offset = {-25.0f, -10.0f}},
	                {.type = TreeInfo::Oak, .offset = {-40.0f, 20.0f}},
	                {.type = FeatureInfo::FatPilarChalk, .offset = {40.0f, 20.0f}},
	                {.type = MobileObjectInfo::Ball, .offset = {20.0f, -15.0f}}},
	});

	all.push_back({
	    .id = "editor.orbit_walker",
	    .name = "A walking creature to orbit and follow",
	    .facet = Facet::Editor,
	    .description = "A tiger walks round a square 80 units across, again and again, for trying the editor's cameras "
	                   "on it: pick it, then Orbit (O) or Follow (Shift+O).",
	    .expected = "Orbiting, the camera keeps round the tiger as it walks, turned by dragging with the right button "
	                "and drawn in and out by the wheel. Following, it eases along behind the tiger as it turns each "
	                "corner. Free, or Escape, hands the camera back where it is.",
	    .framing = {.shot = Shot::Overview, .include = {{-40.0f, -40.0f}, {40.0f, 40.0f}}},
	    .creatures = {Posed(CreatureType::Tiger, {-40.0f, -40.0f})},
	    .commands = {Go(Kind::WalkTo, 0, {40.0f, -40.0f}, 0.5f), Go(Kind::WalkTo, 0, {40.0f, 40.0f}, 0.5f),
	                 Go(Kind::WalkTo, 0, {-40.0f, 40.0f}, 0.5f), Go(Kind::WalkTo, 0, {-40.0f, -40.0f}, 0.5f)},
	    .repeatFrom = 0,
	});

	all.push_back({
	    .id = "editor.pick_several_undo",
	    .name = "Things to pick together, then undo and redo",
	    .facet = Facet::Editor,
	    .description = "A row of five oaks, a ring of four pillars of rock and two villagers, still, for trying the "
	                   "editor's picks of several things and its history (F2): a drag over the land with the Select (W) "
	                   "tool picks all in the rectangle, Ctrl+click picks or lets go of one more, Shift+click adds one, "
	                   "and Shift+click in the outliner picks a run of rows.",
	    .expected = "Each picked thing has a box; the main pick has the brighter box. Move (E) drags all the picks "
	                "together and Rotate (R) turns them about their middle. Ctrl+D copies each, Delete removes all. "
	                "Ctrl+Z puts each step back and Ctrl+Y (or Ctrl+Shift+Z) does it again; the tool bar names the step.",
	    .environment = {.clockRuns = false},
	    .framing = {.shot = Shot::Overview, .include = {{-40.0f, 30.0f}, {40.0f, -30.0f}}},
	    .objects = {{.type = TreeInfo::Oak, .offset = {-30.0f, 20.0f}},
	                {.type = TreeInfo::Oak, .offset = {-15.0f, 20.0f}},
	                {.type = TreeInfo::Oak, .offset = {0.0f, 20.0f}},
	                {.type = TreeInfo::Oak, .offset = {15.0f, 20.0f}},
	                {.type = TreeInfo::Oak, .offset = {30.0f, 20.0f}},
	                {.type = FeatureInfo::FatPilarChalk, .offset = {-10.0f, -10.0f}},
	                {.type = FeatureInfo::FatPilarChalk, .offset = {10.0f, -10.0f}},
	                {.type = FeatureInfo::FatPilarChalk, .offset = {-10.0f, -25.0f}},
	                {.type = FeatureInfo::FatPilarChalk, .offset = {10.0f, -25.0f}},
	                {.type = VillagerInfo::CelticFarmerMale, .offset = {-30.0f, -15.0f}},
	                {.type = VillagerInfo::CelticHousewifeFemale, .offset = {30.0f, -15.0f}}},
	});

	all.push_back({
	    .id = "editor.land_tools",
	    .name = "The land brush, footpaths, streams and script things",
	    .facet = Facet::Editor,
	    .description = "Two huts, a row of oaks and a pillar of rock on flat land, and the editor opens after a second. "
	                   "In the Land tools tab, the Land tool raises or lowers the land in a circle or paints its "
	                   "country, and the Paths tool draws a footpath or a stream, a click for each point. The Script tab "
	                   "of the palette places markers, scrolls and the scrolls of challenges. Save land files writes a "
	                   "landscape file, a land script, a footpath file and a world file into the profile folder's Editor "
	                   "folder.",
	    .expected = "A stroke of Raise makes a round hill: the full step in the inner circle, less to none at the outer "
	                "circle. The oaks and the pillar stay on the land. Paint country changes the ground's texture in "
	                "the outer circle. A footpath from hut to hut shows as a line of nodes in the View menu's "
	                "footpaths. A stream shows its bed and water. A marker shows as a pink diamond while the editor is "
	                "open.",
	    .environment = {.clockRuns = false},
	    .framing = {.shot = Shot::Overview, .include = {{-60.0f, 40.0f}, {60.0f, -40.0f}}},
	    .objects = {{.type = AbodeInfo::CelticHut, .offset = {-45.0f, -25.0f}},
	                {.type = AbodeInfo::CelticHut, .offset = {45.0f, -25.0f}},
	                {.type = TreeInfo::Oak, .offset = {-20.0f, 25.0f}},
	                {.type = TreeInfo::Oak, .offset = {0.0f, 25.0f}},
	                {.type = TreeInfo::Oak, .offset = {20.0f, 25.0f}},
	                {.type = FeatureInfo::FatPilarChalk, .offset = {0.0f, 0.0f}}},
	    .commands = {{.kind = Kind::PressPlainKey,
	                  .delaySeconds = 1.0f,
	                  .value = static_cast<size_t>(input::platform_keys::EditorToggleKey(k_Platform))}},
	});

	all.push_back({
	    .id = "editor.land_generator",
	    .name = "The land generator of the editor, and Exit Editor",
	    .facet = Facet::Editor,
	    .description = "A hut and an oak on flat land, and the editor opens after a second. In the Land generator tab, "
	                   "Start the land generator saves the game into an empty slot and loads a land from a random seed. "
	                   "Type seed 7 and click Generate, then move a slider and click Generate again. Then open the "
	                   "Escape menu and click Exit Editor.",
	    .expected = "The save game room has a new save, Before the land generator, and no other save changes. Each "
	                "Generate clears the world and loads an island with mountains, rivers and trees. The tab shows the "
	                "seed of the land. The third button of the Escape menu is Exit Editor. Exit Editor and the exit "
	                "button of the tab load the testbed again with the hut and the oak.",
	    .environment = {.clockRuns = false},
	    .framing = {.shot = Shot::Overview, .include = {{-40.0f, 30.0f}, {40.0f, -30.0f}}},
	    .objects = {{.type = AbodeInfo::CelticHut, .offset = {-20.0f, 0.0f}}, {.type = TreeInfo::Oak, .offset = {20.0f, 0.0f}}},
	    .commands = {{.kind = Kind::PressPlainKey,
	                  .delaySeconds = 1.0f,
	                  .value = static_cast<size_t>(input::platform_keys::EditorToggleKey(k_Platform))}},
	});

	all.push_back({
	    .id = "editor.generated_start_town",
	    .name = "The temple, town, fields and forests of a generated land",
	    .facet = Facet::Editor,
	    .description = "After a second, the land generator makes the land of seed 7 and the game loads it. The camera "
	                   "looks down at the start area from the south.",
	    .expected = "On the flat start area, the temple of player one stands on one side and a town on the other side. "
	                "The town has a town centre, a storage pit, a creche, five to seven houses with their people and a "
	                "block of fields. The worship site of the tribe of the town stands next to the temple. Trees in "
	                "groups near the town are forests: the foresters go to them for wood.",
	    .environment = {.clockRuns = true},
	    .framing = {.shot = Shot::Overview, .include = {{-40.0f, 30.0f}, {40.0f, -30.0f}}},
	    .commands = {{.kind = Kind::LoadGeneratedLand, .delaySeconds = 1.0f, .value = 7, .object = k_BigIsland}},
	});

	all.push_back({
	    .id = "editor.generated_towns",
	    .name = "The neutral towns of a generated land",
	    .facet = Facet::Editor,
	    .description = "After a second, the land generator makes the land of seed 7 and the game loads it. Fly from the "
	                   "start area to the towns round it, then from town to town.",
	    .expected = "The land has six to eleven towns with the town of player one. Each other town is a neutral town on "
	                "flat ground, 220 to 840 metres from the town nearest to it, and 70 to 270 metres from the sea. "
	                "The towns stand at many heights, low and high. Each town has a town centre, five to seven houses "
	                "with their people and a block of fields. Most towns have a storage pit and a creche. Some towns "
	                "also have a workshop, a graveyard or the plan of a wonder. The houses are the kinds of the tribe "
	                "of the town. A town near the sea can have two to six fish farms on the low shore. The land has "
	                "four to six tribes. No tree grows on the ground of a town or on a fish farm.",
	    .environment = {.clockRuns = true},
	    .framing = {.shot = Shot::Overview, .include = {{-40.0f, 30.0f}, {40.0f, -30.0f}}},
	    .commands = {{.kind = Kind::LoadGeneratedLand, .delaySeconds = 1.0f, .value = 7, .object = k_BigIsland}},
	});

	all.push_back({
	    .id = "editor.generated_paths",
	    .name = "The winding paths and the lanterns of a generated land",
	    .facet = Facet::Editor,
	    .description = "After a second, the land generator makes the land of seed 7 and the game loads it. Follow the "
	                   "path from the temple to the town of player one, then the paths from town to town.",
	    .expected = "A path of bare brown earth goes from the temple to the town of player one. From there, paths go on "
	                "to each neutral town. A path winds on easy ground, goes round the water and does not climb a "
	                "cliff. It stops a short distance from each building and field. On a hill, the path has a smooth "
	                "and level bed, cut into the land or built up above it, with a bank on each side as a road has. "
	                "On a side slope, the bed is a level shelf about 30 m wide. A low cliff or a steep side next to the "
	                "path has a bank of 1 in 2 back to the land, as the cutting of a motorway. In a narrow valley, the "
	                "bed cuts into the gentler side. A path that climbs a steep slope keeps its bends. At the foot of a "
	                "high cliff, the brown earth stays below the cliff and does not cover its top. About one path in "
	                "three has "
	                "lanterns on its sides: street lanterns near the towns and the temple, and lanterns on fires "
	                "farther out. At night the lanterns light the ground.",
	    .environment = {.clockRuns = true},
	    .framing = {.shot = Shot::Overview, .include = {{-40.0f, 30.0f}, {40.0f, -30.0f}}},
	    .commands = {{.kind = Kind::LoadGeneratedLand, .delaySeconds = 1.0f, .value = 7, .object = k_BigIsland}},
	});

	all.push_back({
	    .id = "editor.generated_players_2",
	    .name = "The starts of two players on a generated land",
	    .facet = Facet::Editor,
	    .description = "After a second, the land generator makes the land of seed 7 for two players and the game loads "
	                   "it. The mind file LethysCreature of the game takes the place of the creature that the profile "
	                   "keeps. Fly from the start of player one to the start of player two.",
	    .expected = "Each player has a temple and a town on a flat start area, as on the skirmish lands. The town of "
	                "player two is 510 to 1080 metres from the town of player one. Each town has a town centre, a "
	                "storage pit, a creche, five to seven houses with their people and a block of fields. The ground and "
	                "the trees near the two starts are almost the same. The neutral towns are between the starts and "
	                "round them. Player two is a computer god with a leopard at its temple, as on Two Gods. In the first "
	                "turn, player one brings the ape Matey to its temple. The leopard takes a size like the size of "
	                "the ape.",
	    .environment = {.clockRuns = true},
	    .framing = {.shot = Shot::Overview, .include = {{-40.0f, 30.0f}, {40.0f, -30.0f}}},
	    .commands = {{.kind = Kind::LoadGeneratedLand,
	                  .player = PlayerNames::PLAYER_TWO,
	                  .delaySeconds = 1.0f,
	                  .value = 7,
	                  .object = k_BigIsland,
	                  .mindFile = "LethysCreature"}},
	});

	all.push_back({
	    .id = "editor.generated_players_3",
	    .name = "The starts of three players on a generated land",
	    .facet = Facet::Editor,
	    .description = "At the start, the land generator makes the land of seed 7 for three players and the game loads "
	                   "it. The mind file LethysCreature of the game takes the place of the creature that the profile "
	                   "keeps. In a network game, the land loads before the humans join. Fly from the start of player one "
	                   "to the starts of the other players.",
	    .expected = "Each of the three players has a temple and a town on a flat start area, as on the skirmish lands. "
	                "Players two and three are computer gods with a lion and a horse at their temples, as on Three Gods. "
	                "In the first turn, player one brings the ape Matey to its temple. The lion and the horse take a "
	                "size like the size of the ape. In a network game for two humans, player one brings no creature, and "
	                "player three also gets a computer god with a creature at its temple.",
	    .environment = {.clockRuns = true},
	    .framing = {.shot = Shot::Overview, .include = {{-40.0f, 30.0f}, {40.0f, -30.0f}}},
	    .commands = {{.kind = Kind::LoadGeneratedLand,
	                  .player = PlayerNames::PLAYER_THREE,
	                  .delaySeconds = 0.0f,
	                  .value = 7,
	                  .object = k_BigIsland,
	                  .mindFile = "LethysCreature"}},
	});

	all.push_back({
	    .id = "editor.generated_players_4",
	    .name = "The starts of four players on a generated land",
	    .facet = Facet::Editor,
	    .description = "After a second, the land generator makes the land of seed 7 for four players and the game loads "
	                   "it. The mind file LethysCreature of the game takes the place of the creature that the profile "
	                   "keeps. Fly from the start of player one to the starts of the other players.",
	    .expected = "Each of the four players has a temple and a town on a flat start area, as on the skirmish lands. The "
	                "town of each player is 510 to 1080 metres from the nearest town of another player. The ground and "
	                "the trees near the starts are almost the same. The neutral towns are between the starts and round "
	                "them. Players two, three and four are computer gods with a horse, a tortoise and a zebra at their "
	                "temples, as on Four Gods. In the first turn, player one brings the ape Matey to its temple. The "
	                "three creatures take a size like the size of the ape. A path joins the town of each player to the "
	                "nearest town or temple, and a path joins each temple to its town.",
	    .environment = {.clockRuns = true},
	    .framing = {.shot = Shot::Overview, .include = {{-40.0f, 30.0f}, {40.0f, -30.0f}}},
	    .commands = {{.kind = Kind::LoadGeneratedLand,
	                  .player = PlayerNames::PLAYER_FOUR,
	                  .delaySeconds = 1.0f,
	                  .value = 7,
	                  .object = k_BigIsland,
	                  .mindFile = "LethysCreature"}},
	});

	all.push_back({
	    .id = "editor.generated_trees",
	    .name = "The stands of trees and the big forests of a generated land",
	    .facet = Facet::Editor,
	    .description = "After a second, the land generator makes the land of seed 7 and the game loads it. Fly over "
	                   "the land and look at the trees.",
	    .expected = "Almost all trees stand in groups, and few trees stand alone. Each group has a main kind and some "
	                "trees of the kinds that grow with it, such as palms with palms and conifers with pines. Bushes "
	                "are more often at the edge of a group. Palms grow near the coast and on low plateaus, and no "
	                "conifer grows below 10 metres. Big forests stand alone in the woods, with open ground round them.",
	    .environment = {.clockRuns = true},
	    .framing = {.shot = Shot::Overview, .include = {{-40.0f, 30.0f}, {40.0f, -30.0f}}},
	    .commands = {{.kind = Kind::LoadGeneratedLand, .delaySeconds = 1.0f, .value = 7, .object = k_BigIsland}},
	});

	all.push_back({
	    .id = "editor.generated_rocks",
	    .name = "The rocks, the ring of rocks and the sea stacks of a generated land",
	    .facet = Facet::Editor,
	    .description = "After a second, the land generator makes the land of seed 42 and the game loads it. Fly over "
	                   "the woods, the ring of rocks and the beaches.",
	    .expected = "Small rocks lie in clumps of two to fifteen, a few metres apart, and most boulders and slabs lie "
	                "alone or in pairs. The rocks lean and sink into the ground as the rocks of the story do. Most "
	                "clumps lie in or near a wood. One ring of eight rocks, about 60 metres across, stands on flat "
	                "grass. Groups of three to seven spiky pillars stand on the beaches.",
	    .environment = {.clockRuns = true},
	    .framing = {.shot = Shot::Overview, .include = {{-40.0f, 30.0f}, {40.0f, -30.0f}}},
	    .commands = {{.kind = Kind::LoadGeneratedLand, .delaySeconds = 1.0f, .value = 42, .object = k_BigIsland}},
	});

	all.push_back({
	    .id = "editor.generated_cursed",
	    .name = "The cursed places of a generated land",
	    .facet = Facet::Editor,
	    .description = "After a second, the land generator makes the land of seed 1 and the game loads it. Fly from the "
	                   "start area to the high ground far from the town, where the cursed places are.",
	    .expected = "One to three cursed places stand on high ground, far from the town. Each place has groups of "
	                "burnt trees and dead trees that lie on the ground, and one to three stacks of dark mist over a "
	                "clump of boulders. Some places have one or two craters with a puff of grey smoke. The ground of a "
	                "place is dark: dark scrub at its edge, which is not round, and black rock at its middle.",
	    .environment = {.clockRuns = true},
	    .framing = {.shot = Shot::Overview, .include = {{-40.0f, 30.0f}, {40.0f, -30.0f}}},
	    .commands = {{.kind = Kind::LoadGeneratedLand, .delaySeconds = 1.0f, .value = 1, .object = k_BigIsland}},
	});

	all.push_back({
	    .id = "editor.generated_swamp",
	    .name = "The green mist over a swamp of a generated land",
	    .facet = Facet::Editor,
	    .description = "After a second, the land generator makes the land of seed 1 and the game loads it. Fly about "
	                   "800 metres to the west of the start area, to the swamp at about (2010, 1730).",
	    .expected = "A sheet of dark green mist stands over the wet ground of a swamp: a group of four puffs, a pair "
	                "of puffs and a line of five flat puffs, as on the fourth land of the story. Flat puffs of the same "
	                "mist cover the rest of the swamp, about 60 metres apart, but not its edge. Bushes, oaks, birches "
	                "and other trees of the stream banks of the story grow in the swamp.",
	    .environment = {.clockRuns = true},
	    .framing = {.shot = Shot::Overview, .include = {{-40.0f, 30.0f}, {40.0f, -30.0f}}},
	    .commands = {{.kind = Kind::LoadGeneratedLand, .delaySeconds = 1.0f, .value = 1, .object = k_BigIsland}},
	});

	all.push_back({
	    .id = "editor.generated_cliffs",
	    .name = "The terraces, cliffs and shore of a generated land",
	    .facet = Facet::Editor,
	    .description = "After a second, the land generator makes the land of seed 42 and the game loads it. Turn the "
	                   "camera from the start area to the land round it and to the coast.",
	    .expected = "Flat terraces stand at a few levels, with sharp cliffs between them. Some parts of the land are "
	                "calm, with wide terraces, and other parts have many cliffs. No cliff face is a vertical wall and "
	                "no thin spire stands on the mountains: the steepest faces are about 75 degrees. Along the sea, flat "
	                "shore steps of about 40 metres go round the island, with a bank, a cliff or a long beach behind them.",
	    .environment = {.clockRuns = true},
	    .framing = {.shot = Shot::Overview, .include = {{-40.0f, 30.0f}, {40.0f, -30.0f}}},
	    .commands = {{.kind = Kind::LoadGeneratedLand, .delaySeconds = 1.0f, .value = 42, .object = k_BigIsland}},
	});

	all.push_back({
	    .id = "editor.generated_coast",
	    .name = "The beaches, banks and sea cliffs of a generated land",
	    .facet = Facet::Editor,
	    .description = "After a second, the land generator makes the land of seed 7 and the game loads it. Fly round "
	                   "the coast of the island.",
	    .expected = "The coast has three kinds of parts. On a beach, flat sand goes 60 to 125 metres from the water, "
	                "with palms at its back and dunes behind it, and some beaches have groups of spiky pillars. On a "
	                "bank, a flat strip of about 40 metres goes to a slope. On a sea cliff, a steep face rises 25 to 45 "
	                "metres from the flat strip, also where the land behind is low, and rivers cut through the cliff.",
	    .environment = {.clockRuns = true},
	    .framing = {.shot = Shot::Overview, .include = {{-40.0f, 30.0f}, {40.0f, -30.0f}}},
	    .commands = {{.kind = Kind::LoadGeneratedLand, .delaySeconds = 1.0f, .value = 7, .object = k_BigIsland}},
	});

	all.push_back({
	    .id = "editor.generated_massifs",
	    .name = "The mountain massifs of a generated land",
	    .facet = Facet::Editor,
	    .description = "After a second, the land generator makes the land of seed 7 and the game loads it. Turn the "
	                   "camera from the start area to the mountains inland.",
	    .expected = "Nine to thirteen massifs stand inland. Each massif has a main peak and one or two lower peaks. "
	                "Spurs go down from each summit with gullies between them, and bands of cliffs go round the slopes. "
	                "The ground between the massifs rolls in low steps with short faces.",
	    .environment = {.clockRuns = true},
	    .framing = {.shot = Shot::Overview, .include = {{-40.0f, 30.0f}, {40.0f, -30.0f}}},
	    .commands = {{.kind = Kind::LoadGeneratedLand, .delaySeconds = 1.0f, .value = 7, .object = k_BigIsland}},
	});

	all.push_back({
	    .id = "editor.generated_streams",
	    .name = "The streams of a generated land",
	    .facet = Facet::Editor,
	    .description = "After a second, the land generator makes the land of seed 1 and the game loads it. Fly the "
	                   "camera about 500 metres from the start area to the stream at about (4340, 3270), and follow it "
	                   "down to the sea.",
	    .expected = "The land has streams as the lands of the story do: thin streams of water that start on the hills "
	                "and run down to the sea or into another stream. Each stream is about 30 to 640 metres long, with "
	                "its bed in the land. Stands of trees grow on parts of the banks, near the water. Cedars grow by "
	                "the streams of the jungle and the beach.",
	    .environment = {.clockRuns = true},
	    .framing = {.shot = Shot::Overview, .include = {{-40.0f, 30.0f}, {40.0f, -30.0f}}},
	    .commands = {{.kind = Kind::LoadGeneratedLand, .delaySeconds = 1.0f, .value = 1, .object = k_BigIsland}},
	});

	all.push_back({
	    .id = "editor.generated_high_lakes",
	    .name = "The lakes on the hills of a generated land",
	    .facet = Facet::Editor,
	    .description = "After a second, the land generator makes the land of seed 9 and the game loads it. "
	                   "Fly the camera round a lake on a hill and round a lake in a valley, and look at each shore from "
	                   "below the slope.",
	    .expected = "Each lake has its water at its own level. Its water surface follows its shore and is not a "
	                "rectangle: no water shows through the slopes of the hill. The water has the ripple, texture and "
	                "colour of the sea, and the land under the shallow water shows through it. Two lakes that touch "
	                "have one surface at one level.",
	    .environment = {.clockRuns = true},
	    .framing = {.shot = Shot::Overview, .include = {{-40.0f, 30.0f}, {40.0f, -30.0f}}},
	    .commands = {{.kind = Kind::LoadGeneratedLand, .delaySeconds = 1.0f, .value = 9, .object = k_BigIsland}},
	});

	all.push_back({
	    .id = "editor.generated_lake_shores",
	    .name = "The sand round the lakes of a generated land",
	    .facet = Facet::Editor,
	    .description = "After a second, the land generator makes the land of seed 7 and the game loads it. Fly the "
	                   "camera to the lake on the hill at about (3080, 2380) and look at its shore, then at the sea "
	                   "shore.",
	    .expected = "Each lake has the same sand edge as the sea shore. Wet sand covers the shallow water and the ground "
	                "just above the level of the lake. On gentle warm ground, a beach of dry sand goes 40 to 120 metres "
	                "from the water, or as far as a beach of the sea goes there. A lake on a hill has its sand at its "
	                "own level. A steep side of a lake has a flat band of sand at the water, as the shore steps of the "
	                "sea.",
	    .environment = {.clockRuns = true},
	    .framing = {.shot = Shot::Overview, .include = {{-40.0f, 30.0f}, {40.0f, -30.0f}}},
	    .commands = {{.kind = Kind::LoadGeneratedLand, .delaySeconds = 1.0f, .value = 7, .object = k_BigIsland}},
	});

	all.push_back({
	    .id = "editor.generated_high_mountains",
	    .name = "The high mountains of a generated land",
	    .facet = Facet::Editor,
	    .description = "After a second, the land generator makes the land of seed 42 and the game loads it. Fly the "
	                   "camera from the start area to the highest summits and round them.",
	    .expected = "The highest summits are about 980 to 1130 metres high, as high as the mountains of Snowdonia. Their "
	                "slopes are steep, with snow on the high ground. The low land, the shore and the town are as on a "
	                "land of the story. The land has no steps or gaps on the high slopes.",
	    .environment = {.clockRuns = true},
	    .framing = {.shot = Shot::Overview, .include = {{-40.0f, 30.0f}, {40.0f, -30.0f}}},
	    .commands = {{.kind = Kind::LoadGeneratedLand, .delaySeconds = 1.0f, .value = 42, .object = k_BigIsland}},
	});

	all.push_back({
	    .id = "editor.generated_story_island",
	    .name = "A generated land of the size of a land of the story",
	    .facet = Facet::Editor,
	    .description = "After a second, the land generator makes the land of seed 7 as an island of the size of a land "
	                   "of the story, and the game loads it. Fly round the island from the start area.",
	    .expected = "The land is one island of 2 to 4 square kilometres, about 2 kilometres across, with open sea round "
	                "it. Its mountains are lower than those of the big island, but steep, with terraces and cliffs. The "
	                "start area, the temple, the town of the start, the neutral towns and the shore are as on the big "
	                "island.",
	    .environment = {.clockRuns = true},
	    .framing = {.shot = Shot::Overview, .include = {{-40.0f, 30.0f}, {40.0f, -30.0f}}},
	    .commands = {{.kind = Kind::LoadGeneratedLand, .delaySeconds = 1.0f, .value = 7, .object = k_StoryIsland}},
	});

	all.push_back({
	    .id = "editor.generated_highlands",
	    .name = "A generated land of highlands",
	    .facet = Facet::Editor,
	    .description = "After a second, the land generator makes the land of seed 7 as highlands, and the game loads it. "
	                   "Fly from the start area to the coast and inland.",
	    .expected = "The land is one island of 3 to 5 square kilometres. The ground rises soon after the shore to high "
	                "plains with terraces and cliffs. The plains are higher than on the other types of land. The towns "
	                "stand on flat ground.",
	    .environment = {.clockRuns = true},
	    .framing = {.shot = Shot::Overview, .include = {{-40.0f, 30.0f}, {40.0f, -30.0f}}},
	    .commands = {{.kind = Kind::LoadGeneratedLand, .delaySeconds = 1.0f, .value = 7, .object = k_Highlands}},
	});

	all.push_back({
	    .id = "editor.generated_valleys",
	    .name = "A generated land of valleys",
	    .facet = Facet::Editor,
	    .description = "After a second, the land generator makes the land of seed 7 as a land of valleys, and the game "
	                   "loads it. Fly along the mountain ranges.",
	    .expected = "The land is one island of 3 to 5 square kilometres. Two or three long mountain ranges cross it, "
	                "with valleys between them. The towns stand on flat ground in the valleys and near the coast.",
	    .environment = {.clockRuns = true},
	    .framing = {.shot = Shot::Overview, .include = {{-40.0f, 30.0f}, {40.0f, -30.0f}}},
	    .commands = {{.kind = Kind::LoadGeneratedLand, .delaySeconds = 1.0f, .value = 7, .object = k_Valleys}},
	});

	all.push_back({
	    .id = "editor.generated_archipelago",
	    .name = "A generated archipelago",
	    .facet = Facet::Editor,
	    .description = "After a second, the land generator makes the land of seed 7 as an archipelago, and the game "
	                   "loads it. Fly from island to island. The archipelago waits on the user: without its choice, the "
	                   "seed does not make one.",
	    .expected = "The land is three to five islands with sea between them, each of 0.5 to 2 square kilometres. The "
	                "start area and its town are on one island. Neutral towns stand on the islands that are near enough.",
	    .environment = {.clockRuns = true},
	    .framing = {.shot = Shot::Overview, .include = {{-40.0f, 30.0f}, {40.0f, -30.0f}}},
	    .commands = {{.kind = Kind::LoadGeneratedLand, .delaySeconds = 1.0f, .value = 7, .object = k_Archipelago}},
	});

	all.push_back({
	    .id = "editor.generated_fairy_rings",
	    .name = "The fairy rings and the lone pillars of a generated land",
	    .facet = Facet::Editor,
	    .description = "After a second, the land generator makes the land of seed 64 and the game loads it. Fly to "
	                   "the fairy rings on the gentle grass and to the lone pillars of rock.",
	    .expected = "This land is like the first land of the story. Each of its six fairy rings has ten magic mushrooms "
	                "and two Champi mushrooms round five small rocks, about 17 metres across. About forty groups of lone "
	                "pillars of limestone stand alone or in pairs, 217 metres or more apart. On the land of seed 42, "
	                "each fairy ring has six large magic mushrooms. A creature can eat a magic mushroom of a ring.",
	    .environment = {.clockRuns = true},
	    .framing = {.shot = Shot::Overview, .include = {{-40.0f, 30.0f}, {40.0f, -30.0f}}},
	    .commands = {{.kind = Kind::LoadGeneratedLand, .delaySeconds = 1.0f, .value = 64, .object = k_BigIsland}},
	});

	all.push_back({
	    .id = "editor.generated_mushroom_clumps",
	    .name = "The clumps of mushrooms and toadstools of a generated land",
	    .facet = Facet::Editor,
	    .description = "After a second, the land generator makes the land of seed 42 and the game loads it. Fly to "
	                   "the small clumps of magic mushrooms, Champi mushrooms and toadstools on the grass and the heather.",
	    .expected = "This land is like the third land of the story. Each clump is a copy of a clump of that land: seven "
	                "magic mushrooms, two or five Champi mushrooms, or three toadstools. The clumps are 60 metres or more "
	                "apart. On the land of seed 64, a clump has twenty small magic mushrooms and five toadstools. No tree "
	                "grows on the high sides of the mountains.",
	    .environment = {.clockRuns = true},
	    .framing = {.shot = Shot::Overview, .include = {{-40.0f, 30.0f}, {40.0f, -30.0f}}},
	    .commands = {{.kind = Kind::LoadGeneratedLand, .delaySeconds = 1.0f, .value = 42, .object = k_BigIsland}},
	});

	all.push_back({
	    .id = "editor.animals",
	    .name = "Every animal of the palette",
	    .facet = Facet::Editor,
	    .description = "Open flat land, and the editor opens after a second. The Animals tab of the "
	                   "palette lists each animal that a land's script makes: the grazers, the hunters of the wild and "
	                   "the birds of the land. Lay out all puts one of each in rows in the middle of the view.",
	    .expected = "Each animal comes down on the land as a land's script makes it, then lives as the land's animals "
	                "do: the grazers eat grass, the hunters look for food and the birds fly over the land. The outliner "
	                "lists each under Animals, and Ctrl+D copies one.",
	    .framing = {.shot = Shot::Overview, .include = {{-30.0f, 30.0f}, {30.0f, -30.0f}}},
	    .commands = {{.kind = Kind::PressPlainKey,
	                  .delaySeconds = 1.0f,
	                  .value = static_cast<size_t>(input::platform_keys::EditorToggleKey(k_Platform))}},
	});
}
