/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The testbed's scenarios of the vortices between the lands: a way out of a land opening on the lake's bank, levelling
// the bank and the water's edge towards their average, and closing again; and the volcano's mouth, which leaves the land
// as it is

#include "3D/FlatLand.h"
#include "TestbedScenarioRegistry.h"

using namespace openblack;
using namespace openblack::testbed_scenarios;

namespace
{
/// On the lake's south bank: the levelled square takes in the plane, the bank and the shallows
constexpr glm::vec2 k_Lake = flat_land::k_LakeCentre - flat_land::k_MapMiddle;
constexpr glm::vec2 k_OnTheBank = k_Lake - glm::vec2(0.0f, flat_land::k_LakeHalfExtent.y + 5.0f);
/// Out on the plane, where the land is already flat
constexpr glm::vec2 k_OnThePlane {-120.0f, 60.0f};
} // namespace

void testbed_scenarios::AddVortexScenarios(std::vector<Scenario>& all)
{
	all.push_back({
	    .id = "story.vortex_levels_the_bank",
	    .name = "A vortex out of the land levels the lake's bank",
	    .facet = Facet::Miracles,
	    .description = "A way out of a land opens on the lake's south bank after a second, as a land's script opens one, "
	                   "and another out on the flat plane; both are told to fade out 20 seconds after they open.",
	    .expected = "For 2 seconds after each opens nothing happens; then over 5 seconds the bank and the water's edge in "
	                "an 11 by 11 cell square round the first are pulled towards the square's average height, the slope "
	                "easing out as it goes, wholly within 50 of the middle and less towards 56; the plane's stays flat. "
	                "Fading out does not undo it: the levelled bank stays after the vortices have gone, 7 seconds after "
	                "they were told to fade out. The vortices themselves are not drawn yet.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {k_OnTheBank, k_OnThePlane, k_Lake}, .distance = 0.8f},
	    .vortices = {{.type = VortexType::In, .offset = k_OnTheBank, .delaySeconds = 1.0f, .fadeOutAfterSeconds = 20.0f},
	                 {.type = VortexType::In, .offset = k_OnThePlane, .delaySeconds = 1.0f, .fadeOutAfterSeconds = 20.0f}},
	});

	all.push_back({
	    .id = "story.volcano_vortex_keeps_the_land",
	    .name = "The volcano's mouth leaves the land as it is",
	    .facet = Facet::Miracles,
	    .description = "The glowing mouth of a volcano is opened on the lake's bank, as the last land's script opens it.",
	    .expected = "It starts fully open and the bank under it keeps its slope: the volcano's mouth never levels the "
	                "land. It is not drawn yet.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {k_OnTheBank, k_Lake}, .distance = 0.8f},
	    .vortices = {{.type = VortexType::Volcano, .offset = k_OnTheBank, .delaySeconds = 1.0f}},
	});
}
