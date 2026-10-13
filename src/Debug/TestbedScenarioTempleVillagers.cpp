/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The testbed's scenarios of the villagers around the player's temple, with their debug window open

#include <array>
#include <vector>

#include "TempleVillagers.h"
#include "TestbedScenarioRegistry.h"

using namespace openblack;
using namespace openblack::testbed_scenarios;

namespace
{
/// Where the town centre stands
constexpr glm::vec2 k_Centre {0.0f, 40.0f};
/// Where the player's temple stands, near enough for the town's people to walk to its worship site
constexpr glm::vec2 k_Temple {0.0f, -30.0f};
/// The town's belief in the player, so that it stays theirs rather than going back to the neutral player
constexpr float k_TownBelief = 1000.0f;
/// The main room of the temple, by the temple's rooms
constexpr size_t k_MainRoom = 3;

/// A Celtic town centre of the player's, two huts and eight of its people about it
std::vector<ObjectSetup> Town()
{
	std::vector<ObjectSetup> objects {
	    {.type = AbodeInfo::CelticTownCentre, .offset = k_Centre},
	    {.type = AbodeInfo::CelticHut, .offset = k_Centre + glm::vec2(-16.0f, 6.0f), .yawDegrees = 160.0f},
	    {.type = AbodeInfo::CelticHut, .offset = k_Centre + glm::vec2(16.0f, 6.0f), .yawDegrees = 200.0f},
	};
	constexpr std::array<VillagerInfo, 4> k_People {VillagerInfo::CelticHousewifeFemale, VillagerInfo::CelticFarmerMale,
	                                                VillagerInfo::CelticForesterMale, VillagerInfo::CelticShepherdMale};
	for (size_t i = 0; i < 8; ++i)
	{
		const auto across = static_cast<float>(i % 4) * 5.0f - 7.5f;
		const auto down = i < 4 ? -10.0f : -15.0f;
		objects.push_back(
		    {.type = k_People.at(i % k_People.size()), .offset = k_Centre + glm::vec2(across, down), .joinTown = true});
	}
	return objects;
}
} // namespace

void openblack::testbed_scenarios::AddTempleVillagerScenarios(std::vector<Scenario>& all)
{
	all.push_back({
	    .id = "debug.temple_villagers",
	    .name = "Villagers around the temple",
	    .facet = Facet::Movement,
	    .description = "The player's temple standing south of a Celtic town of the player's with eight people. The town's "
	                   "worship site is put up built and its totem raised to half its people, and the Temple Villagers "
	                   "debug window opens.",
	    .expected = "Half the town sets off for the worship site by the temple, through its gate, and joins the dance round "
	                "the altar. The window lists them going from on the way to worshipping, their states and walks, and "
	                "marks them, their walks and the site's places in the world. Making villagers, sending them to "
	                "worship, the share slider, pausing and stepping all work from it.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview,
	                .include = {k_Temple - glm::vec2(30.0f), k_Centre + glm::vec2(20.0f)},
	                .distance = 0.6f},
	    .objects = Town(),
	    .temples = {{.offset = k_Temple}},
	    .townBeliefInPlayer = k_TownBelief,
	    .worshipShare = 0.5f,
	    .debugWindow = debug::gui::k_TempleVillagersWindow,
	});
	all.push_back({
	    .id = "debug.temple_villagers_inside",
	    .name = "Villagers inside the temple",
	    .facet = Facet::Movement,
	    .description = "The same town and temple, the whole town sent to worship; once they have had time to reach the "
	                   "site the player goes into the temple's main room with the Temple Villagers debug window open.",
	    .expected = "Those worshipping come in with the player and wander the main room's floors, walking from point to "
	                "point and now and then standing a few seconds. The window's Inside the temple list shows each, "
	                "walking or standing and where it is going, and marks them and the floors they walk.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview,
	                .include = {k_Temple - glm::vec2(30.0f), k_Centre + glm::vec2(20.0f)},
	                .distance = 0.6f},
	    .objects = Town(),
	    .temples = {{.offset = k_Temple}},
	    .commands = {{.kind = Command::Kind::EnterTemple, .delaySeconds = 60.0f, .value = k_MainRoom}},
	    .townBeliefInPlayer = k_TownBelief,
	    .worshipShare = 1.0f,
	    .debugWindow = debug::gui::k_TempleVillagersWindow,
	});
}
