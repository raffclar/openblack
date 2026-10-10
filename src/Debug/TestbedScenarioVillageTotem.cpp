/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The testbed's scenario of a town centre's totem

#include <vector>

#include "TestbedScenarioRegistry.h"

using namespace openblack;
using namespace openblack::testbed_scenarios;

namespace
{
/// Where the town centre stands
constexpr glm::vec2 k_Centre {0.0f, 40.0f};
} // namespace

void openblack::testbed_scenarios::AddVillageTotemScenarios(std::vector<Scenario>& all)
{
	all.push_back({
	    .id = "objects.village_totem",
	    .name = "A town centre's totem",
	    .facet = Facet::Objects,
	    .description = "A Norse town centre of the player's town, which has no worship site.",
	    .expected = "The tribe's totem, the column in the middle of the centre, stands at the centre's totem point, turned "
	                "as the centre is, with the hand's icon on its pole (the player has no creature), sunk in the centre "
	                "with no rise: with nowhere to worship the town has no one at worship. Holding the Action button on "
	                "it takes no hold, as there is no worship site.",
	    .framing = {.shot = Shot::Overview,
	                .include = {k_Centre - glm::vec2(15.0f), k_Centre + glm::vec2(15.0f)},
	                .distance = 0.5f},
	    .objects = {{.type = AbodeInfo::NorseTownCentre, .offset = k_Centre}},
	});
}
