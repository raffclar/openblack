/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The testbed's scenario of a town growing from the buildings it planned

#include <array>
#include <utility>
#include <vector>

#include "TestbedScenarioRegistry.h"

using namespace openblack;
using namespace openblack::testbed_scenarios;

namespace
{
/// Where the town's village centre, storage pit and one built hut stand
constexpr glm::vec2 k_Centre {0.0f, 50.0f};
constexpr glm::vec2 k_Pit {-30.0f, 30.0f};
constexpr glm::vec2 k_Hut {25.0f, 30.0f};
/// Where the town plans three more huts, and a crèche
constexpr std::array<glm::vec2, 3> k_PlannedHuts {{{-15.0f, 5.0f}, {5.0f, 5.0f}, {25.0f, 5.0f}}};
constexpr glm::vec2 k_PlannedCreche {-40.0f, 60.0f};
/// Plenty of wood and food in the pit, so that building is never held up for either
constexpr uint32_t k_PitStock = 5000;
/// The town's grown villagers, too many for its one hut
constexpr size_t k_Couples = 6;
constexpr float k_VillagerSpacing = 3.0f;
} // namespace

void openblack::testbed_scenarios::AddTownGrowthScenarios(std::vector<Scenario>& all)
{
	std::vector<ObjectSetup> objects {
	    {.type = AbodeInfo::CelticTownCentre, .offset = k_Centre},
	    {.type = AbodeInfo::CelticStoragePit, .offset = k_Pit, .storedFood = k_PitStock, .storedWood = k_PitStock},
	    {.type = AbodeInfo::CelticHut, .offset = k_Hut, .yawDegrees = 180.0f},
	};
	for (const auto& place : k_PlannedHuts)
	{
		objects.push_back({.type = AbodeInfo::CelticHut, .offset = place, .yawDegrees = 180.0f, .planned = true});
	}
	objects.push_back({.type = AbodeInfo::CelticCreche, .offset = k_PlannedCreche, .planned = true});
	for (size_t i = 0; i < k_Couples; ++i)
	{
		const float x = (static_cast<float>(i) - (static_cast<float>(k_Couples) * 0.5f)) * k_VillagerSpacing;
		objects.push_back({.type = VillagerInfo::CelticForesterMale, .offset = {x, 20.0f}, .joinTown = true});
		objects.push_back({.type = VillagerInfo::CelticHousewifeFemale, .offset = {x, 17.0f}, .joinTown = true});
	}
	all.push_back({
	    .id = "town.growth_plans",
	    .name = "A town growing from its plans",
	    .facet = Facet::Objects,
	    .description = "A Celtic town of the player's with a village centre, a storage pit full of wood and food and one "
	                   "built hut, and twelve grown villagers, too many for the hut. The town plans three more huts in a "
	                   "row in front and a crèche to the west; none of them stands. Best watched at game speed 4.",
	    .expected = "The town wants homes, so its villagers ask it, at most once a game turn, to start the hut it wants "
	                "most: the first planned hut appears with nothing built and a site, builders fetch wood from the pit "
	                "and raise it stroke by stroke. Each further hut starts only once no site wants more builders, and "
	                "huts stop being started once there is room for a tenth more than the town's people. The crèche is "
	                "started only once the town wants a civic building. A hut destroyed by the inspector "
	                "(edit.destroy) is planned again by the town where it stood, and built again when the town wants it.",
	    .framing = {.shot = Shot::Overview, .include = {k_PlannedCreche, k_Hut, k_PlannedHuts[0]}, .distance = 0.6f},
	    .objects = std::move(objects),
	});
}
