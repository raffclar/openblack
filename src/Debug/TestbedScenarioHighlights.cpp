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
	                "the sign stands still at its size. Each but the plain scroll has a soft white glow before it, "
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
}
