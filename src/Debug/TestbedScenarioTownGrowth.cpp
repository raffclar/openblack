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
/// Four couples, room enough in three huts
constexpr size_t k_MendingCouples = 4;
/// Two fields east of the town
constexpr std::array<glm::vec2, 2> k_Fields {{{55.0f, 40.0f}, {55.0f, 15.0f}}};
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
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {k_PlannedCreche, k_Hut, k_PlannedHuts[0]}, .distance = 0.6f},
	    .objects = std::move(objects),
	});

	// A town that rebuilds a finished building it lost, unasked, as a repair
	std::vector<ObjectSetup> mending {
	    {.type = AbodeInfo::CelticTownCentre, .offset = k_Centre},
	    {.type = AbodeInfo::CelticStoragePit, .offset = k_Pit, .storedFood = k_PitStock, .storedWood = k_PitStock},
	    {.type = AbodeInfo::CelticHut, .offset = k_Hut, .yawDegrees = 180.0f},
	    {.type = AbodeInfo::CelticHut, .offset = k_PlannedHuts[0], .yawDegrees = 180.0f},
	    {.type = AbodeInfo::CelticHut, .offset = k_PlannedHuts[2], .yawDegrees = 180.0f},
	    {.type = AbodeInfo::CelticHut, .offset = k_PlannedHuts[1], .yawDegrees = 180.0f, .planned = true, .wasBuilt = true},
	};
	for (size_t i = 0; i < k_MendingCouples; ++i)
	{
		const float x = (static_cast<float>(i) - (static_cast<float>(k_MendingCouples) * 0.5f)) * k_VillagerSpacing;
		mending.push_back({.type = VillagerInfo::CelticForesterMale, .offset = {x, 20.0f}, .joinTown = true});
		mending.push_back({.type = VillagerInfo::CelticHousewifeFemale, .offset = {x, 17.0f}, .joinTown = true});
	}
	all.push_back({
	    .id = "town.mending",
	    .name = "A town rebuilding a building it lost",
	    .facet = Facet::Objects,
	    .description = "A Celtic town with a village centre, a full storage pit and three built huts for its four "
	                   "couples. Where the middle hut of the front row stood is the town's plan "
	                   "of it, a hut that had been finished and went from the land. Best watched at game speed 4.",
	    .expected = "At its first turn the town puts the lost hut up for mending before any villager asks it to (its "
	                "askedToBuildTurn stays empty): it appears at "
	                "once with nothing built and a site, and builders fetch wood from the pit and raise it again. A "
	                "built hut hurt with the inspector gets a site at once, as before; the town puts up no more than "
	                "one building a turn of its own accord.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {k_Pit, k_Hut, k_PlannedHuts[0]}, .distance = 0.6f},
	    .objects = std::move(mending),
	});

	// A town whose store has run out of food, with fields to farm
	std::vector<ObjectSetup> hungry {
	    {.type = AbodeInfo::CelticTownCentre, .offset = k_Centre},
	    {.type = AbodeInfo::CelticStoragePit, .offset = k_Pit, .storedFood = 0, .storedWood = k_PitStock},
	    {.type = AbodeInfo::CelticHut, .offset = k_Hut, .yawDegrees = 180.0f},
	    {.type = AbodeInfo::CelticHut, .offset = k_PlannedHuts[0], .yawDegrees = 180.0f},
	    {.type = AbodeInfo::CelticHut, .offset = k_PlannedHuts[2], .yawDegrees = 180.0f},
	};
	for (const auto& place : k_Fields)
	{
		hungry.push_back({.type = FieldTypeInfo::Wheat, .offset = place});
	}
	for (size_t i = 0; i < k_MendingCouples; ++i)
	{
		const float x = (static_cast<float>(i) - (static_cast<float>(k_MendingCouples) * 0.5f)) * k_VillagerSpacing;
		hungry.push_back({.type = VillagerInfo::CelticForesterMale, .offset = {x, 20.0f}, .joinTown = true});
		hungry.push_back({.type = VillagerInfo::CelticHousewifeFemale, .offset = {x, 17.0f}, .joinTown = true});
	}
	all.push_back({
	    .id = "town.food_jobs",
	    .name = "A town out of food, with fields",
	    .facet = Facet::Objects,
	    .description = "A Celtic town with a village centre, three huts for its four couples, a storage "
	                   "pit with no food in it (wood plenty) and two bare wheat fields to the east. Best watched at game "
	                   "speed 4.",
	    .expected = "The town wants food. Its villagers deciding what to do are offered to its desires, the most wanted "
	                "first, and take up farming the fields, sowing them, then harvesting and carrying the food to the "
	                "pit.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {k_Pit, k_Hut, k_Fields[1]}, .distance = 0.6f},
	    .objects = std::move(hungry),
	});
}
