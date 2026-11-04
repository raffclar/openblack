/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// Testbed scenarios for the land editor.

#include "Creature/CreatureCave.h"
#include "Creature/CreatureFight.h"
#include "Input/PlatformKeys.h"
#include "TestbedScenarioBuilders.h"
#include "TestbedScenarioRegistry.h"

using namespace openblack;
using namespace openblack::testbed_scenarios;

void testbed_scenarios::AddEditor(std::vector<Scenario>& all)
{
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
}
