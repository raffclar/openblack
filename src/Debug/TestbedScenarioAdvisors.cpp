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
}
