/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The testbed's scenarios of the scrolls and signs a land's scripts put up

#include <vector>

#include "TestbedScenarioRegistry.h"

using namespace openblack;
using namespace openblack::testbed_scenarios;

void testbed_scenarios::AddHighlightScenarios(std::vector<Scenario>& all)
{
	constexpr glm::vec2 k_Hut {30.0f, 30.0f};
	all.push_back({
	    .id = "scripts.highlights",
	    .name = "Scrolls and signs",
	    .facet = Facet::Objects,
	    .description = "The four kinds of highlight a land's script puts up, in a row on bare land: a plain scroll, a "
	                   "\"did you know\" sign with a tip, a silver scroll and a gold one; a started silver and gold "
	                   "scroll behind them; a gold scroll 6 above the land; and a gold scroll on a Celtic hut.",
	    .expected = "The scrolls spin half a turn a second and are drawn larger as the camera goes further, up to 30 away; "
	                "the sign keeps its size and spins too until its tip is read. Each but the plain scroll has a soft white "
	                "glow before it, "
	                "brightest on the silver; the silver and gold ones glint in their colour. The started ones show their "
	                "active models and their active sparkles. The raised one floats 6 up, and the one by the hut stands on "
	                "its roof. Tapping the sign starts it (it turns to face the camera and loses its glow), has the "
	                "advisors explain the signs and, from the next turn, opens the blue tip bubble over it: \"Did you "
	                "know...?\" over the tip, its tail pointing at the sign; tapping the sign again closes it, and so "
	                "does turning the camera off it. Tapping a started scroll does nothing more that shows.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-20.0f, 0.0f}, {20.0f, 10.0f}, k_Hut}, .distance = 0.5f},
	    .objects = {{.type = AbodeInfo::CelticHut, .offset = k_Hut, .yawDegrees = 180.0f}},
	    .highlights = {{.kind = 0, .offset = {-15.0f, 0.0f}},
	                   {.kind = 1, .offset = {-5.0f, 0.0f}, .challenge = 0, .tip = std::pair {4127u, 3u}},
	                   {.kind = 2, .offset = {5.0f, 0.0f}},
	                   {.kind = 3, .offset = {15.0f, 0.0f}},
	                   {.kind = 2, .offset = {5.0f, 10.0f}, .active = true},
	                   {.kind = 3, .offset = {15.0f, 10.0f}, .active = true},
	                   {.kind = 3, .offset = {-15.0f, 10.0f}, .height = 6.0f},
	                   {.kind = 3, .offset = k_Hut}},
	});
	all.push_back({
	    .id = "scripts.highlight_effects_frames",
	    .name = "A gold scroll and a started silver scroll close up, their effects stepped in the drawn frames",
	    .facet = Facet::Objects,
	    .description = "A gold scroll and a started silver scroll on bare land, close up, with the camera still. Their "
	                   "glints and the sparkles of the started scroll step in the frames that draw them, no more often "
	                   "than 60 times a second.",
	    .expected = "The gold glints and the silver sparkles move as they do at 60 frames a second, also at hundreds of "
	                "frames a second. They do not move faster or flicker more at a high frame rate. Their random numbers "
	                "are only this computer's, so the world stays the same.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 6.0f, 12.0f}, .look = {0.0f, 2.0f, 25.0f}},
	    .highlights = {{.kind = 3, .offset = {-3.0f, 25.0f}}, {.kind = 2, .offset = {3.0f, 25.0f}, .active = true}},
	});

	// A lesson's gold scroll, as the creature lessons put it up at the pen, near the camera
	all.push_back({
	    .id = "scripts.highlight_click",
	    .name = "Clicking a lesson scroll",
	    .facet = Facet::Hand,
	    .description = "A gold scroll 6 above bare land near the camera, as a creature lesson puts one up. Move the hand "
	                   "over the scroll and click the Action button on it.",
	    .expected = "The hand finds the scroll anywhere in a circle round it. A click records the scroll as the thing the "
	                "player clicked: the inspector shows it in the hand's HandClicked, so a script that waits for the "
	                "scroll to be clicked continues. Let go with the hand off the scroll, the click is forgotten.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 12.0f, -20.0f}, .look = {0.0f, 6.0f, 0.0f}},
	    .highlights = {{.kind = 3, .offset = {0.0f, 0.0f}, .height = 6.0f}},
	});

	// One sign near the camera and one at a usual playing distance, both with tips not read yet, so that they spin
	all.push_back({
	    .id = "hand.help_sign",
	    .name = "Hovering over and tapping a sign",
	    .facet = Facet::Hand,
	    .description = "Two \"did you know\" signs whose tips haven't been read, one near the camera and one about 40 "
	                   "away, on bare land. Move the hand over each and press the Action button on it.",
	    .expected = "Until it is tapped a sign spins half a turn a second at its own size. The hand rests on the sign "
	                "where the pointer meets its post or board, staying turned to the land, as a sign is lower than half "
	                "the hand's height, so the board turns through the fingers. As the board turns edge on to the "
	                "camera the pointer beside the post meets the land behind and the hand eases back through the sign "
	                "to it. A sign is found only where the pointer is on its post or board, not anywhere in a circle "
	                "around it; a press on the land between the pointer and the hand's middle, nearer the middle than "
	                "the pointer's point is, still taps it. Tapped, the sign stops, turns to face the camera and opens "
	                "its tip in the blue bubble.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 6.0f, -15.0f}, .look = {0.0f, 2.0f, 0.0f}},
	    .highlights = {{.kind = 1, .offset = {-3.0f, 0.0f}, .challenge = 0, .tip = std::pair {4128u, 3u}},
	                   {.kind = 1, .offset = {6.0f, 25.0f}, .challenge = 0, .tip = std::pair {4129u, 3u}}},
	});

	// Signs in different land blocks: the game spins a sign only in the frames where its list of things to draw has it
	all.push_back({
	    .id = "scripts.sign_phases",
	    .name = "Signs spin only while the game draws them",
	    .facet = Facet::Objects,
	    .description = "Four \"did you know\" signs whose tips are not read: two before the camera, one 300 behind it "
	                   "and one 750 before it. Wait a few seconds, turn the camera round to the sign behind it, then turn "
	                   "back.",
	    .expected = "The two signs before the camera spin together, half a turn a second. The sign behind the camera "
	                "did not spin while it was out of view, so it shows at a different angle from the others. The sign "
	                "750 away does not show: the game draws no thing in a land block more than 700 from the camera.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 6.0f, -15.0f}, .look = {0.0f, 2.0f, 0.0f}},
	    .highlights = {{.kind = 1, .offset = {-3.0f, 0.0f}, .challenge = 0, .tip = std::pair {4128u, 3u}},
	                   {.kind = 1, .offset = {4.0f, 5.0f}, .challenge = 0, .tip = std::pair {4129u, 3u}},
	                   {.kind = 1, .offset = {0.0f, -300.0f}, .challenge = 0, .tip = std::pair {4130u, 3u}},
	                   {.kind = 1, .offset = {0.0f, 750.0f}, .challenge = 0, .tip = std::pair {4131u, 3u}}},
	});
	// The scrolls shine with the game's first environment map, which their models' materials add over their colour
	all.push_back({
	    .id = "light.scroll_shine",
	    .name = "Scrolls shine with the environment map",
	    .facet = Facet::Light,
	    .description = "A plain scroll, a silver scroll, a gold scroll and a started gold scroll in a row, close before "
	                   "the camera, on bare land. Turn the camera round them.",
	    .expected = "The silver and gold scrolls, started or not, have a bright sheen added over their colour. The sheen "
	                "moves over them as they spin and as the camera turns, because it follows the way their faces point "
	                "across the view. The plain scroll has no sheen. The sea's mirror shows no sheen.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 4.0f, -9.0f}, .look = {0.0f, 1.5f, 0.0f}},
	    .highlights = {{.kind = 0, .offset = {-4.5f, 0.0f}},
	                   {.kind = 2, .offset = {-1.5f, 0.0f}},
	                   {.kind = 3, .offset = {1.5f, 0.0f}},
	                   {.kind = 3, .offset = {4.5f, 0.0f}, .active = true}},
	});
	// A script's mist, as the Singing Stones' Twinkle tune makes it: white and a tenth opaque. Then the mist grows to
	// twice its size and fades out over ten seconds.
	using Argument = ScriptArgument;
	all.push_back({
	    .id = "effects.script_mist_fade",
	    .name = "A script makes a mist and fades it out",
	    .facet = Facet::Light,
	    .description = "On bare land, a script makes a white mist 3 above the land, of size 0.8, a tenth opaque and its "
	                   "height 0.6 of its width, as the Singing Stones' Twinkle tune on the second land does. Three seconds "
	                   "later the script fades it: its size from 1 to 2 and its alpha from 50 to 0 over ten seconds.",
	    .expected = "The faint white puff of mist shows at once, round seen from above and flatter seen level. When the "
	                "fade starts it jumps to size 1 and a little more opaque, then grows steadily to twice that while it "
	                "fades away, a step each game turn, gone after ten seconds. The log tells that CREATE_MIST gave back "
	                "1 value and that SET_MIST_FADE ran.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 16.0f, -40.0f}, .look = {0.0f, 3.0f, 0.0f}},
	    .commands = {{.kind = Command::Kind::CallNative,
	                  .delaySeconds = 1.0f,
	                  .script = "CREATE_MIST",
	                  .scriptArguments = {{.kind = Argument::Kind::Position, .point = {0.0f, 0.0f}, .height = 3.0f},
	                                      {.kind = Argument::Kind::Number, .number = 0.8f},
	                                      {.kind = Argument::Kind::Number, .number = 255.0f},
	                                      {.kind = Argument::Kind::Number, .number = 255.0f},
	                                      {.kind = Argument::Kind::Number, .number = 255.0f},
	                                      {.kind = Argument::Kind::Number, .number = 0.1f},
	                                      {.kind = Argument::Kind::Number, .number = 0.6f}}},
	                 {.kind = Command::Kind::CallNative,
	                  .delaySeconds = 3.0f,
	                  .script = "SET_MIST_FADE",
	                  .scriptArguments = {{.kind = Argument::Kind::Result},
	                                      {.kind = Argument::Kind::Number, .number = 1.0f},
	                                      {.kind = Argument::Kind::Number, .number = 2.0f},
	                                      {.kind = Argument::Kind::Number, .number = 50.0f},
	                                      {.kind = Argument::Kind::Number, .number = 0.0f},
	                                      {.kind = Argument::Kind::Number, .number = 10.0f}}}},
	});
}
