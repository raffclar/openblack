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
/// Where the player's temple stands, well clear of the town centre
constexpr glm::vec2 k_Temple {0.0f, -60.0f};
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
	all.push_back({
	    .id = "objects.village_totem_worship",
	    .name = "A town centre's totem with a temple",
	    .facet = Facet::Objects,
	    .description = "The same Norse town centre of the player's town, with the player's temple standing built "
	                   "beside it, which opens the worship site of the town, which has one villager.",
	    .expected = "Once the town's worship site is built, holding the Action button on the totem takes hold of it: the "
	                "hand goes to the icon, tipped forwards over it in its side hold, and the pointer stays on the hand. "
	                "Sliding the mouse up raises it, a see-through second totem staying at the town's share, with the "
	                "People Worshipping tooltip by the hand. Let go, the share floats up over it in grey and it eases "
	                "there, the second totem waiting where it is going.",
	    .framing = {.shot = Shot::Overview,
	                .include = {k_Centre - glm::vec2(15.0f), k_Centre + glm::vec2(15.0f)},
	                .distance = 0.5f},
	    .objects = {{.type = AbodeInfo::NorseTownCentre, .offset = k_Centre},
	                // A town with no one in it has no worship site
	                {.type = VillagerInfo::NorseHousewifeFemale,
	                 .offset = k_Centre + glm::vec2(10.0f, -10.0f),
	                 .joinTown = true}},
	    .temples = {{.offset = k_Temple}},
	});
}
