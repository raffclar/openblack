/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The testbed's scenarios of the sky: the moon at night, looked at from low on the land towards the east, where it
// keeps its place beside the camera, for trying its phases and its path through the night with the moon debug window;
// a storm's forked bolts coming down round the camera; and, for the experiment, the planet and the aurora from the
// ground to space

#include <cmath>

#include <algorithm>
#include <vector>

#include "Graphics/Atmosphere.h"
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

CameraPlacement testbed_scenarios::Climbing(const CameraPlacement& start, const Climb& climb, float seaLevel, float elapsed)
{
	constexpr float k_LowestHeight = 1.0f;
	const float from = std::max(start.origin.y - seaLevel, k_LowestHeight);
	const float to = std::max(climb.top, from);
	const float share = std::clamp(elapsed / std::max(climb.seconds, 1e-3f), 0.0f, 1.0f);
	const float eased = share * share * (3.0f - (2.0f * share));
	CameraPlacement placement = start;
	placement.origin.y = seaLevel + (from * std::pow(to / from, eased));
	return placement;
}

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

	all.push_back({
	    .id = "sky.storm_bolts",
	    .name = "A storm's forked bolts",
	    .facet = Facet::Sky,
	    .description = "A thunderstorm over the whole island at noon, its forked bolts let down every one to two seconds "
	                   "somewhere within 300 of the camera (the weather window's storm strikes round the camera), which "
	                   "looks down on that ground from 700 up.",
	    .expected = "Each bolt starts as a point of light under the cloud and forks down to points on the ground round "
	                "it, its ribbons laid out again each step; a light flashes on the land where it lands, and the land and "
	                "sky flash dimly. The bolt is gone after 0.4 to 0.8 seconds. Its thunder, small, medium or large, "
	                "comes from the land under it, later the further it is. Bright flashes with thunder of their own come "
	                "every 3 to 15 seconds without a bolt.",
	    .environment = {.weather = Weather::Thunderstorm, .boltSeconds = glm::vec2(1.0f, 2.0f), .dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 700.0f, -410.0f}, .look = {0.0f, 0.0f, -140.0f}},
	    .objects = TreesToTheEast(),
	});

	// EXPERIMENT (not for merge): the lightning inside the volumetric clouds
	all.push_back({
	    .id = "weather.cloud_lightning",
	    .name = "Lightning inside the clouds at night",
	    .facet = Facet::Sky,
	    .description = "A thunderstorm over the whole island at midnight, the clock stopped, under an overcast layer of "
	                   "the experiment's volumetric clouds. The storm flashes every one to two seconds round the camera, "
	                   "which looks up at the bottom of the clouds from 40 up, past the south west corner of the land.",
	    .expected = "Each flash lights the cloud from inside round its channel: a bright blue white core, then less light "
	                "through the cloud farther away, with two to five quick pulses in less than a second. The bottom of "
	                "the cloud over the flash is lit and the clouds round it are lit less. The land under the flash takes "
	                "a little of its light. Seen from over the clouds, the flash shows as a glow through the tops round "
	                "it.",
	    .environment = {.hour = 0.0f,
	                    .clockRuns = false,
	                    .weather = Weather::Thunderstorm,
	                    .boltSeconds = glm::vec2(1.0f, 2.0f),
	                    .dispenserGrid = false,
	                    .cloudCover = static_cast<uint8_t>(4)},
	    .framing = {.shot = Shot::Placed, .eye = {-2560.0f, 40.0f, -3460.0f}, .look = {-2560.0f, 600.0f, -2260.0f}},
	    .objects = TreesToTheEast(),
	});

	// EXPERIMENT (not for merge): the planet, the air and the aurora from space and from the ground
	all.push_back({
	    .id = "space.planet",
	    .name = "The planet from space at night",
	    .facet = Facet::Sky,
	    .description = "The camera is 10000 km over the sea to the south west of the island at 22:30 on a night with a "
	                   "strong aurora, the clock stopped, under the experiment's clear sky. It looks down at the planet "
	                   "past the island.",
	    .expected = "One round planet against the stars. The night side is dark, with the thin blue limb of the air round "
	                "its edge and a faint green glow of the high air along the limb. The aurora is a green ring round the "
	                "magnetic pole, with red over it. There is no second, paler ball inside the planet.",
	    .environment = {.hour = 22.5f,
	                    .clockRuns = false,
	                    .dispenserGrid = false,
	                    .cloudCover = static_cast<uint8_t>(1),
	                    .auroraStrength = graphics::atmosphere::k_StrongAurora},
	    .framing = {.shot = Shot::Placed, .eye = {-3.8e6f, 9.0e6f, -3.8e6f}, .look = {1.4e6f, -1.8e6f, 1.4e6f}},
	    .objects = TreesToTheEast(),
	});

	all.push_back({
	    .id = "space.climb",
	    .name = "A climb from the sea to space",
	    .facet = Facet::Sky,
	    .description = "The camera starts 40 over the sea, 20 km to the south west of the island, at 17:30 with the "
	                   "clock stopped. It climbs straight up to 10000 km in 20 seconds, faster as it gets higher, and "
	                   "keeps its look at the island.",
	    .expected = "The horizon goes down and curves. The sky goes from blue to deep blue to black, the stars come out "
	                "and the thin blue limb of the air shows round the planet. The sea, the clouds and the air stay one "
	                "ball all the way up: no jump in the sea or the haze between 20 km and 200 km up, where the world's "
	                "flat cap moves to the island, and no second ball inside the planet from space.",
	    .environment = {.hour = 17.5f, .clockRuns = false, .dispenserGrid = false},
	    .framing = {.shot = Shot::Placed,
	                .eye = {-14000.0f, 40.0f, -14000.0f},
	                .look = {0.0f, 0.0f, 0.0f},
	                .climb = Climb {.top = 9.5e6f, .seconds = 20.0f}},
	    .objects = TreesToTheEast(),
	});

	all.push_back({
	    .id = "sky.aurora_night",
	    .name = "A strong aurora over the island",
	    .facet = Facet::Sky,
	    .description = "Midnight on a night with a strong aurora, the clock stopped, under the experiment's clear sky. "
	                   "The camera stands 40 up on the land and looks west and up, where the curtains hang at midnight.",
	    .expected = "Green curtains of the aurora fill the western sky and come up over the island, with thin upright "
	                "rays, slow folds and a fast flicker. Their bottom edge is sharp with a little purple in it, and high "
	                "up they go red. They show behind the few trees to the east and in the sea's reflection.",
	    .environment = {.hour = 0.0f,
	                    .clockRuns = false,
	                    .dispenserGrid = false,
	                    .cloudCover = static_cast<uint8_t>(1),
	                    .auroraStrength = graphics::atmosphere::k_StrongAurora},
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 40.0f, -600.0f}, .look = {-1400.0f, 500.0f, -1000.0f}},
	    .objects = TreesToTheEast(),
	});
}
