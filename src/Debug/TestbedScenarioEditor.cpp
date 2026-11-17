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
	    .commands = {{.kind = Kind::LoadGeneratedLand, .delaySeconds = 1.0f, .value = 7}},
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
	    .commands = {{.kind = Kind::LoadGeneratedLand, .delaySeconds = 1.0f, .value = 7}},
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
	    .commands = {{.kind = Kind::LoadGeneratedLand, .delaySeconds = 1.0f, .value = 42}},
	});

	all.push_back({
	    .id = "editor.generated_cursed",
	    .name = "The cursed places of a generated land",
	    .facet = Facet::Editor,
	    .description = "After a second, the land generator makes the land of seed 1 and the game loads it. Fly from the "
	                   "start area to the high ground far from the town, where the cursed places are.",
	    .expected = "One to three cursed places stand on high ground, far from the town. Each place has groups of "
	                "burnt trees and one to three stacks of dark mist over a clump of boulders. Some places have one "
	                "or two craters with a puff of grey smoke. The ground of a place is the ground of the land round it.",
	    .environment = {.clockRuns = true},
	    .framing = {.shot = Shot::Overview, .include = {{-40.0f, 30.0f}, {40.0f, -30.0f}}},
	    .commands = {{.kind = Kind::LoadGeneratedLand, .delaySeconds = 1.0f, .value = 1}},
	});

	all.push_back({
	    .id = "editor.generated_cliffs",
	    .name = "The terraces, cliffs and shore of a generated land",
	    .facet = Facet::Editor,
	    .description = "After a second, the land generator makes the land of seed 42 and the game loads it. Turn the "
	                   "camera from the start area to the land round it and to the coast.",
	    .expected = "Flat terraces stand at a few levels, with sharp cliffs between them. Some parts of the land are "
	                "calm, with wide terraces, and other parts have many cliffs. Along the sea, a flat strip of 30 to "
	                "45 metres goes round the island, with a bank or a cliff behind it.",
	    .environment = {.clockRuns = true},
	    .framing = {.shot = Shot::Overview, .include = {{-40.0f, 30.0f}, {40.0f, -30.0f}}},
	    .commands = {{.kind = Kind::LoadGeneratedLand, .delaySeconds = 1.0f, .value = 42}},
	});

	all.push_back({
	    .id = "editor.generated_massifs",
	    .name = "The mountain massifs of a generated land",
	    .facet = Facet::Editor,
	    .description = "After a second, the land generator makes the land of seed 7 and the game loads it. Turn the "
	                   "camera from the start area to the mountains inland.",
	    .expected = "Fifteen to seventeen massifs stand inland. Each massif has a main peak and one or two lower peaks. "
	                "Spurs go down from each summit with gullies between them, and bands of cliffs go round the slopes. "
	                "The ground between the massifs rolls in low steps with short faces.",
	    .environment = {.clockRuns = true},
	    .framing = {.shot = Shot::Overview, .include = {{-40.0f, 30.0f}, {40.0f, -30.0f}}},
	    .commands = {{.kind = Kind::LoadGeneratedLand, .delaySeconds = 1.0f, .value = 7}},
	});

	all.push_back({
	    .id = "editor.generated_high_mountains",
	    .name = "The high mountains of a generated land",
	    .facet = Facet::Editor,
	    .description = "After a second, the land generator makes the land of seed 42 and the game loads it. Fly the "
	                   "camera from the start area to the highest summits and round them.",
	    .expected = "The highest summits are about 900 to 1050 metres high, as high as the mountains of Snowdonia. Their "
	                "slopes are steep, with snow on the high ground. The low land, the shore and the town are as on a "
	                "land of the story. The land has no steps or gaps on the high slopes.",
	    .environment = {.clockRuns = true},
	    .framing = {.shot = Shot::Overview, .include = {{-40.0f, 30.0f}, {40.0f, -30.0f}}},
	    .commands = {{.kind = Kind::LoadGeneratedLand, .delaySeconds = 1.0f, .value = 42}},
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
