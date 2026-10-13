/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The testbed's scenarios of the player's advisors: both coming out of their corners and hovering, clinging to the
// bottom of the screen as they do for a short exchange, pointing and acting, and appearing and vanishing in smoke

#include <vector>

#include "TestbedScenarioRegistry.h"

using namespace openblack;
using namespace openblack::testbed_scenarios;

namespace
{
using Kind = Command::Kind;
using Action = Command::AdvisorAction;

constexpr size_t k_Good = 0;
constexpr size_t k_Evil = 1;
/// A message set of the help that says two texts through a help script
constexpr size_t k_TwoTextsMessage = 97;

Command Tell(size_t advisor, Action action, float delay, glm::vec2 point = glm::vec2(0.0f), uint32_t anim = 0)
{
	return {.kind = Kind::Advisor, .delaySeconds = delay, .point = point, .value = advisor, .advisor = action, .anim = anim};
}

/// Looking over the middle of the map from a little way off
Framing AdvisorsFraming()
{
	return {.shot = Shot::Overview, .include = {{-40.0f, -40.0f}, {40.0f, 40.0f}}, .distance = 1.0f};
}
} // namespace

void testbed_scenarios::AddAdvisorScenarios(std::vector<Scenario>& all)
{
	all.push_back({
	    .id = "advisors.out",
	    .name = "Both advisors come out",
	    .facet = Facet::Advisors,
	    .description = "The good advisor, then the evil one, is told to come out of its corner, as a challenge's script tells "
	                   "it.",
	    .expected = "Each flies in from beyond its edge of the screen, the good one from the right and the evil one from "
	                "the left, to a place in the middle half of the screen in a second, trailing a faint rainbow. There "
	                "they hover a few units in front of the camera, bobbing and drifting on smooth curves, keeping apart "
	                "from each other, from the pointer and from the middle of the screen, leaning as they move across and "
	                "blinking. The good advisor, a white-bearded sage, wears a flickering halo; the evil one is a small red "
	                "imp. Both are lit from beside the camera, not by the land.",
	    .environment = {.dispenserGrid = false},
	    .framing = AdvisorsFraming(),
	    .commands = {Tell(k_Good, Action::Out, 0.5f), Tell(k_Evil, Action::Out, 0.2f)},
	});
	all.push_back({
	    .id = "advisors.cling",
	    .name = "The advisors cling to the bottom of the screen",
	    .facet = Facet::Advisors,
	    .description = "Both advisors are told to cling near the bottom of the screen, as the help does for a short "
	                   "exchange, then to come out again.",
	    .expected = "Each slides off the bottom edge of the screen and holds on to it, only its upper half showing, turned "
	                "to face up from the edge, swinging a little as it clings. After five seconds each lets go and flies "
	                "back into the middle of the screen.",
	    .environment = {.dispenserGrid = false},
	    .framing = AdvisorsFraming(),
	    .commands = {Tell(k_Good, Action::Cling, 0.5f, {0.25f, 0.95f}), Tell(k_Evil, Action::Cling, 0.0f, {0.75f, 0.95f}),
	                 Tell(k_Good, Action::Out, 5.0f), Tell(k_Evil, Action::Out, 0.0f)},
	});
	all.push_back({
	    .id = "advisors.cling_sides",
	    .name = "The advisors cling to the sides of the screen",
	    .facet = Facet::Advisors,
	    .description = "The good advisor is told to cling to the right side of the screen halfway down and the evil one to "
	                   "the left side a little higher, as a script does, then both come out again.",
	    .expected = "Each flies in from beyond its side and holds on to that side of the screen, on any screen shape at the "
	                "real screen's edge, only the half of it nearer the middle showing, turned to face in from the edge and "
	                "swinging a little. The advisors are the same size for the screen's height as on a 4:3 screen. After "
	                "six seconds each lets go and flies back into the screen.",
	    .environment = {.dispenserGrid = false},
	    .framing = AdvisorsFraming(),
	    .commands = {Tell(k_Good, Action::Cling, 0.5f, {1.0f, 0.5f}), Tell(k_Evil, Action::Cling, 0.0f, {0.0f, 0.4f}),
	                 Tell(k_Good, Action::Out, 6.0f), Tell(k_Evil, Action::Out, 0.0f)},
	});
	all.push_back({
	    .id = "advisors.go_home",
	    .name = "The advisors go home off the screen",
	    .facet = Facet::Advisors,
	    .description = "Both advisors come out and fly out towards their sides of the screen, then are sent home; then "
	                   "they come out again and are sent home from the middle of the screen.",
	    .expected = "From near the sides each flies off past its own side of the screen in a second; from the middle each "
	                "flies off past the top or bottom. On every screen shape each leaves the screen completely before it "
	                "is gone: neither vanishes while still in view.",
	    .environment = {.dispenserGrid = false},
	    .framing = AdvisorsFraming(),
	    .commands = {Tell(k_Good, Action::Out, 0.5f), Tell(k_Evil, Action::Out, 0.0f),
	                 Tell(k_Good, Action::Fly, 2.0f, {0.9f, 0.5f}), Tell(k_Evil, Action::Fly, 0.0f, {0.1f, 0.45f}),
	                 Tell(k_Good, Action::Home, 2.5f), Tell(k_Evil, Action::Home, 0.0f), Tell(k_Good, Action::Out, 2.5f),
	                 Tell(k_Evil, Action::Out, 0.0f), Tell(k_Good, Action::Fly, 1.5f, {0.55f, 0.5f}),
	                 Tell(k_Evil, Action::Fly, 0.0f, {0.45f, 0.5f}), Tell(k_Good, Action::Home, 2.0f),
	                 Tell(k_Evil, Action::Home, 0.0f)},
	});
	all.push_back({
	    .id = "advisors.screen_point",
	    .name = "The advisors point at the corners of the screen",
	    .facet = Facet::Advisors,
	    .description = "Both come out; the evil advisor points at a point of the screen near its top right corner and the "
	                   "good one at a point near its bottom left corner, as a script's screen point tells them.",
	    .expected = "Each raises the arm on the side of its point and turns its head towards it, holding it there: the "
	                "evil one towards the real screen's top right corner and the good one towards its bottom left, on any "
	                "screen shape.",
	    .environment = {.dispenserGrid = false},
	    .framing = AdvisorsFraming(),
	    .commands = {Tell(k_Good, Action::Out, 0.5f), Tell(k_Evil, Action::Out, 0.0f),
	                 Tell(k_Evil, Action::PointOnScreen, 2.0f, {0.95f, 0.1f}),
	                 Tell(k_Good, Action::PointOnScreen, 0.0f, {0.05f, 0.9f})},
	});
	all.push_back({
	    .id = "advisors.point_and_act",
	    .name = "The advisors point and act",
	    .facet = Facet::Advisors,
	    .description = "Both come out; the good advisor points at the land in front of the camera and the evil one at a "
	                   "point of the screen; then the good one plays a nod and the evil one laughs, each at a point of the "
	                   "screen, and the evil one turns furious.",
	    .expected = "Each raises the arm on the side of what it points at, the good one turning to the land and the evil "
	                "one to the upper right of the screen, and holds it there, its head turned towards it. Told to act, "
	                "each flies to its point on the screen, plays the anim once and goes back to hovering. The evil one's "
	                "face turns furious and calms down over a few seconds.",
	    .environment = {.dispenserGrid = false},
	    .framing = AdvisorsFraming(),
	    .commands = {Tell(k_Good, Action::Out, 0.2f), Tell(k_Evil, Action::Out, 0.0f),
	                 Tell(k_Good, Action::PointAtLand, 2.0f, {0.0f, 20.0f}),
	                 Tell(k_Evil, Action::PointOnScreen, 0.0f, {0.8f, 0.2f}),
	                 Tell(k_Good, Action::PlayAnim, 4.0f, {0.3f, 0.5f}, 20),
	                 Tell(k_Evil, Action::PlayAnim, 0.0f, {0.7f, 0.5f}, 24), Tell(k_Evil, Action::Feel, 3.0f, {}, 7)},
	});
	all.push_back({
	    .id = "advisors.appear_and_vanish",
	    .name = "The advisors appear and vanish in smoke",
	    .facet = Facet::Advisors,
	    .description = "Both advisors appear, as a help script brings them, and after five seconds vanish.",
	    .expected = "Each appears at once a quarter of the way in from its side, halfway down the screen, fading in out of "
	                "a puff of smoke, grey for the good advisor and dark red for the evil one, which spreads and fades in "
	                "about a second. After five seconds each fades out in another puff.",
	    .environment = {.dispenserGrid = false},
	    .framing = AdvisorsFraming(),
	    .commands = {Tell(k_Good, Action::Appear, 0.5f), Tell(k_Evil, Action::Appear, 0.0f), Tell(k_Good, Action::Vanish, 5.0f),
	                 Tell(k_Evil, Action::Vanish, 0.0f)},
	});
	all.push_back({
	    .id = "advisors.escape_in_cut_scene",
	    .name = "Escape in a script's cut scene",
	    .facet = Facet::Advisors,
	    .description = "A script's cinema bars slide in after half a second and stay in for thirty seconds, then slide out. "
	                   "Press Escape while they are in, then again once they are gone.",
	    .expected = "While the bars are in, Escape does nothing: the menu stays shut and the game runs on. Once they have "
	                "gone, Escape opens the game's menu.",
	    .environment = {.dispenserGrid = false},
	    .framing = AdvisorsFraming(),
	    .commands = {{.kind = Kind::WideScreen, .delaySeconds = 0.5f, .value = 1},
	                 {.kind = Kind::WideScreen, .delaySeconds = 30.0f, .value = 0}},
	});
	all.push_back({
	    .id = "advisors.escape_help_script",
	    .name = "Escape stops a help script holding the dialogue",
	    .facet = Facet::Advisors,
	    .description = "After half a second the help runs one of its messages, two texts the advisors say, through a help "
	                   "script that takes the dialogue. Press Escape while it runs.",
	    .expected = "Escape stops the help script: its text goes, the advisors are cut short and go home, and the game's "
	                "menu opens.",
	    .environment = {.dispenserGrid = false},
	    .framing = AdvisorsFraming(),
	    .commands = {{.kind = Kind::RunHelpMessage, .delaySeconds = 0.5f, .value = k_TwoTextsMessage}},
	});
}
