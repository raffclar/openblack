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
// a storm's forked bolts coming down round the camera; and, for the experiment, the lightning and the thunderstorm
// cells of the volumetric clouds, the planet and the aurora from the ground to space, the noctilucent clouds, the blue
// hour and the jellyfish in the deep sea

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

	// EXPERIMENT (not for merge): the thunderstorm cells of the volumetric clouds
	all.push_back({
	    .id = "weather.cumulonimbus",
	    .name = "A thunderstorm cell from 8 km away",
	    .facet = Facet::Sky,
	    .description = "A thunderstorm over the island at 16:30, the clock stopped. The camera stands on the sea 8500 west "
	                   "of the middle of the land, under a clear sky (the experiment's clear cover), and looks up at the "
	                   "storm. The storm flashes every one to two seconds.",
	    .expected = "One or two tall towers stand over the island, from a dark flat bottom about 1 km up to the "
	                "tropopause, about 10 km up. A flat anvil spreads out downwind at the top, with a sharp edge upwind "
	                "and a soft plume downwind. A dome rises over the top of the tallest tower. A dark grey haze of rain "
	                "comes down under each tower. Each flash lights the tower from inside and the bolt comes down from "
	                "the bottom of a tower, never from the clear air.",
	    .environment = {.hour = 16.5f,
	                    .clockRuns = false,
	                    .weather = Weather::Thunderstorm,
	                    .boltSeconds = glm::vec2(1.0f, 2.0f),
	                    .dispenserGrid = false,
	                    .cloudCover = static_cast<uint8_t>(1)},
	    .framing = {.shot = Shot::Placed, .eye = {-8500.0f, 30.0f, 0.0f}, .look = {0.0f, 4500.0f, 0.0f}},
	    .objects = TreesToTheEast(),
	});

	// EXPERIMENT (not for merge): the planet, the air and the aurora from space and from the ground
	all.push_back({
	    .id = "space.planet",
	    .name = "The planet from space at night",
	    .facet = Facet::Sky,
	    .description = "The camera is 10000 km over the sea to the south west of the island at 22:30 on a night with a "
	                   "strong aurora, the clock stopped, under the experiment's clear sky. It looks at the middle of the "
	                   "planet and stays there.",
	    .expected = "One round planet against the stars. The night side is dark, with the thin blue limb of the air round "
	                "its edge and a faint green glow of the high air along the limb. The aurora is a green ring round the "
	                "magnetic pole, with red over it. There is no second, paler ball inside the planet.",
	    .environment = {.hour = 22.5f,
	                    .clockRuns = false,
	                    .dispenserGrid = false,
	                    .cloudCover = static_cast<uint8_t>(1),
	                    .auroraStrength = graphics::atmosphere::k_StrongAurora},
	    .framing = {.shot = Shot::Placed, .eye = {-3.8e6f, 9.0e6f, -3.8e6f}, .look = {0.0f, -6.05e6f, 0.0f}, .hold = true},
	    .objects = TreesToTheEast(),
	});

	all.push_back({
	    .id = "space.climb",
	    .name = "A climb from the sea to space",
	    .facet = Facet::Sky,
	    .description = "The camera starts 40 over the sea, 20 km to the south west of the island, at 17:30 with the "
	                   "clock stopped, under scattered clouds. It climbs straight up to 10000 km in 20 seconds, faster "
	                   "as it gets higher, keeps its look at the island, and stays at the top.",
	    .expected = "The horizon goes down and curves. The sky goes from blue to deep blue to black, the stars come out "
	                "and the thin blue limb of the air shows round the planet. The sea, the clouds and the air stay one "
	                "ball all the way up: no jump in the sea or the haze between 20 km and 200 km up, where the world's "
	                "flat cap moves to the island, and no second ball inside the planet from space.",
	    .environment = {.hour = 17.5f, .clockRuns = false, .dispenserGrid = false, .cloudCover = static_cast<uint8_t>(2)},
	    .framing = {.shot = Shot::Placed,
	                .eye = {-14000.0f, 40.0f, -14000.0f},
	                .look = {0.0f, 0.0f, 0.0f},
	                .climb = Climb {.top = 9.5e6f, .seconds = 20.0f},
	                .hold = true},
	    .objects = TreesToTheEast(),
	});

	all.push_back({
	    .id = "sky.cloud_regions",
	    .name = "The regions of cloud from 100 km up",
	    .facet = Facet::Sky,
	    .description = "The camera is 100 km over the sea, 28 km to the south west of the island, at noon with the clock "
	                   "stopped, under scattered clouds. It looks down at the island and stays there.",
	    .expected = "The clouds are not one even carpet. There are clear areas, groups of thick cloud, streets of cells in "
	                "lines along the wind, open cells with rings of cloud, and areas of small cells and of large cells. "
	                "The edges of the far clouds are soft. Near the island the clouds are the same as before.",
	    .environment = {.hour = 12.0f, .clockRuns = false, .dispenserGrid = false, .cloudCover = static_cast<uint8_t>(2)},
	    .framing = {.shot = Shot::Placed, .eye = {-20000.0f, 94900.0f, -20000.0f}, .look = {0.0f, 0.0f, 0.0f}, .hold = true},
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
	                "up they go red.",
	    .environment = {.hour = 0.0f,
	                    .clockRuns = false,
	                    .dispenserGrid = false,
	                    .cloudCover = static_cast<uint8_t>(1),
	                    .auroraStrength = graphics::atmosphere::k_StrongAurora},
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 40.0f, -600.0f}, .look = {-1400.0f, 500.0f, -1000.0f}},
	    .objects = TreesToTheEast(),
	});

	// EXPERIMENT (not for merge): the noctilucent clouds and the blue hour of the air model
	all.push_back({
	    .id = "sky.noctilucent",
	    .name = "Noctilucent clouds at dusk",
	    .facet = Facet::Sky,
	    .description = "The clock stands at 18:45, with the sun 9 degrees under the horizon, under a clear sky. The "
	                   "noctilucent clouds show at full strength whatever the night. The camera is low on the land and "
	                   "looks west, towards the sun, a little up.",
	    .expected = "Thin silver-blue bands and small billows of cloud fill a low part of the western sky, over the last "
	                "glow of the sunset, and the darker sky round them. They are brightest near the horizon and towards "
	                "the sun. They drift slowly. They are gone overhead, where the earth's shadow covers them.",
	    .environment = {.hour = 18.75f,
	                    .clockRuns = false,
	                    .dispenserGrid = false,
	                    .cloudCover = static_cast<uint8_t>(1),
	                    .noctilucent = 1.0f},
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 10.0f, 0.0f}, .look = {-400.0f, 70.0f, 0.0f}},
	    .objects = TreesToTheEast(),
	});

	// EXPERIMENT (not for merge): the jellyfish in the deep sea
	all.push_back({
	    .id = "sea.jellyfish_deep",
	    .name = "Jellyfish in the deep sea at night",
	    .facet = Facet::Sky,
	    .description = "Midnight, the clock stopped, under a clear sky. The camera is 31 under the sea in the testbed's "
	                   "deep sea in the north-east corner of the map, where the sea bed is about 40 deep, and looks "
	                   "level at a place 4 units away where a jellyfish rises.",
	    .expected = "Dark water, with the pink and orange glow of jellyfish round the camera. They beat their bells and "
	                "drift slowly up. One rises through the middle of the view 4 units away about once in 50 seconds; "
	                "others show farther away. Their glow keeps its colour and fades with distance.",
	    .environment = {.hour = 0.0f, .clockRuns = false, .dispenserGrid = false, .cloudCover = static_cast<uint8_t>(1)},
	    .framing =
	        {.shot = Shot::Placed, .eye = {1906.1f, -31.0f, 1907.85f}, .look = {1908.92f, -31.0f, 1910.65f}, .hold = true},
	    .objects = TreesToTheEast(),
	});

	all.push_back({
	    .id = "sky.ozone_twilight",
	    .name = "The blue hour",
	    .facet = Facet::Sky,
	    .description = "The clock stands at 18:30, with the sun 6 degrees under the horizon, under a clear sky with no "
	                   "noctilucent clouds. The camera is low on the land and looks west, about 20 degrees up.",
	    .expected = "The upper sky is a deep blue, not grey or yellow: the ozone takes the red and the green "
	                "out of the sun's light that goes through the high air. Low in the west a band of warm glow stays "
	                "over the horizon. The east is darker, with the earth's shadow low over the horizon.",
	    .environment = {.hour = 18.5f,
	                    .clockRuns = false,
	                    .dispenserGrid = false,
	                    .cloudCover = static_cast<uint8_t>(1),
	                    .noctilucent = 0.0f},
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 10.0f, 0.0f}, .look = {-400.0f, 150.0f, 0.0f}},
	    .objects = TreesToTheEast(),
	});
	// EXPERIMENT (not for merge): the camera that goes round the planet
	all.push_back({
	    .id = "space.orbit_navigation",
	    .name = "The hand turns the planet from space",
	    .facet = Facet::Sky,
	    .description = "Noon with the clock stopped. The camera is 15000 km over the sea a little to the south west of "
	                   "the island and looks down at the planet's middle. Drag the planet with the hand: it turns as a "
	                   "globe under the hand. Drag it down the screen four times (inspector input.path, from the middle "
	                   "of the screen 90 pixels down) to go north over the pole. Then turn the wheel towards the screen, "
	                   "one notch at a time, to go down to the island.",
	    .expected = "The planet stays in the middle of the screen. The point that the hand grips stays under the cursor, "
	                "and the camera goes round the planet past the pole with no flip and no roll. Each wheel notch takes "
	                "the camera a fifth nearer to the sea. From 400 km to 200 km up the view turns from the planet's middle "
	                "to the normal camera, with no jump, and the camera comes down at the point under the middle of the "
	                "screen.",
	    .environment = {.hour = 12.0f, .clockRuns = false, .dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {-300000.0f, 1.5e7f, -300000.0f}, .look = {0.0f, 0.0f, 0.0f}},
	    .objects = TreesToTheEast(),
	});
	// EXPERIMENT (not for merge): the planet from far round it, where the eye is under the sea's level
	all.push_back({
	    .id = "space.far_side",
	    .name = "The planet from its far side",
	    .facet = Facet::Sky,
	    .description = "Noon with the clock stopped. The camera is 15000 km over the planet, 150 degrees round from the "
	                   "island over the north pole, and looks at the planet's middle. The eye is far under the sea's "
	                   "level. Drag the planet with the hand to turn it, and turn the wheel to go down to the point under "
	                   "the middle of the screen.",
	    .expected = "The whole planet shows: the sea ball, the clouds from the planet's weather map, the air's thin limb "
	                "and the night side, which is dark with faint clouds in the moon's light. The sun is behind the "
	                "planet, so its glare shows only past the limb. Under the water the view is not blue. The wheel "
	                "takes the camera down to the open sea at that point, where it is night, with no island in sight.",
	    .environment = {.hour = 12.0f, .clockRuns = false, .dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {7173000.0f, -2.361e7f, 7173000.0f}, .look = {2560.0f, -6046079.0f, 2560.0f}},
	    .objects = TreesToTheEast(),
	});
	// EXPERIMENT (not for merge): the camera low over the open sea far from the island
	all.push_back({
	    .id = "space.far_sea",
	    .name = "The open sea 600 km from the island",
	    .facet = Facet::Sky,
	    .description = "Noon with the clock stopped. The camera is 300 units over the open sea, 600 km north of the island, "
	                   "and looks along the sea.",
	    .expected = "No island shows: it is far over the horizon. The sea curves away to its own horizon as round the "
	                "island, and the waves near the eye are sharp, with no noise.",
	    .environment = {.hour = 12.0f, .clockRuns = false, .dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {426824.0f, 300.0f, 426824.0f}, .look = {428238.0f, 0.0f, 428238.0f}},
	    .objects = TreesToTheEast(),
	});
}
