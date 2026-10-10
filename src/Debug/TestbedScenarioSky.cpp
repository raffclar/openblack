/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The testbed's scenarios of the sky: the moon at night, looked at from low on the land towards the east, where it
// keeps its place beside the camera, for trying its phases and its path through the night with the moon debug window

#include <vector>

#include "TestbedScenarioRegistry.h"

using namespace openblack;
using namespace openblack::testbed_scenarios;

namespace
{
/// A camera low on the land looking a little north of east and a little up, so the moon at midnight, 4000 east and 950
/// up, stands right of the middle of the screen, clear of the scenario window
const Framing k_LookingEast {.shot = Shot::Placed, .eye = {0.0f, 10.0f, 0.0f}, .look = {400.0f, 105.0f, 90.0f}};

/// A few trees to the east, dark against the sky, which the moon shows behind
std::vector<ObjectSetup> TreesToTheEast()
{
	return {
	    {.type = TreeInfo::Conifer, .offset = {60.0f, -12.0f}},
	    {.type = TreeInfo::Oak, .offset = {75.0f, 10.0f}},
	    {.type = TreeInfo::Conifer, .offset = {90.0f, 30.0f}},
	};
}
} // namespace

void testbed_scenarios::AddSkyScenarios(std::vector<Scenario>& all)
{
	all.push_back({
	    .id = "sky.moon_midnight",
	    .name = "The moon at midnight",
	    .facet = Facet::Sky,
	    .description = "The clock stands at midnight under a clear sky, the camera low on the land looking east at the "
	                   "moon. Open the Moon debug window to step through its phases.",
	    .expected = "A half sphere of moon, about 270 across, hangs high in the east at its full strength, with a soft "
	                "glow about it, tinted the night's moon colour. Its lit part matches the real moon tonight; jumping the "
	                "date to a full, new or quarter moon in the Moon window turns its face, and scripts are told 0 at a "
	                "full moon and 1 at a new moon once it has shown with that phase. It stays the same size and place as "
	                "the camera moves, and its mirror image shows in the sea. The trees in front of it hide it where they "
	                "stand against it.",
	    .environment = {.hour = 0.0f, .clockRuns = false, .dispenserGrid = false},
	    .framing = k_LookingEast,
	    .objects = TreesToTheEast(),
	});

	all.push_back({
	    .id = "sky.moon_night",
	    .name = "The moon through a night",
	    .facet = Facet::Sky,
	    .description = "The clock runs from 19:00 under a clear sky, the camera low on the land looking east.",
	    .expected = "No moon until about 19:20 script time, when it fades in low in the south east; it is at its full "
	                "strength from about 21:00, highest at midnight due east, and fades out after 03:00, gone by about "
	                "04:40 low in the north east. Its phase does not change through the night.",
	    .environment = {.hour = 19.0f, .clockRuns = true, .dispenserGrid = false},
	    .framing = k_LookingEast,
	    .objects = TreesToTheEast(),
	});
}
