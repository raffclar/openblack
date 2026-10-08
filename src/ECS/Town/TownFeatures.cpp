/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "TownFeatures.h"

#include <cmath>

#include <limits>

#include "3D/MapCoords.h"
#include "ECS/Components/Town.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/Registry.h"
#include "ECS/Town/TownPlacement.h"
#include "ECS/Town/TownQueries.h"
#include "ECS/Town/TownStores.h"
#include "ECS/Trees.h"
#include "Enums.h"
#include "Locator.h"

// The town features of a map load (TownFeatures.h)

namespace openblack::ecs::town_features
{
namespace
{
glm::vec3 Metres(const map_coords::MapCoords& coords)
{
	const auto m = map_coords::ToMetres(coords);
	return {m.x, 0.0f, m.y};
}

/// (openblack) a metre value that map_coords::ToFixed (MakeScenicForest's FromMetres) turns back into `fixed`
/// exactly: ToFixed(ToMetres(c)) can come back one unit lower, so it is nudged one float at a time
float ExactMetres(int32_t fixed)
{
	float m = map_coords::ToMetres(fixed);
	for (int i = 0; i < 8 && map_coords::ToFixed(m) != fixed; ++i)
	{
		const float towards =
		    map_coords::ToFixed(m) < fixed ? std::numeric_limits<float>::max() : std::numeric_limits<float>::lowest();
		m = std::nextafter(m, towards);
	}
	return m;
}
} // namespace

glm::vec3 ForestReference(entt::entity town)
{
	// The storage pit -> its position (not its arrive point)
	if (const auto pit = town_queries::GetStoragePit(town); pit != entt::null)
	{
		return Metres(object::MapCoordsOf(pit));
	}
	// The temporary wood pot's nearest edge to the town's position (a pit-less town gets its MagicWood pot here)
	const auto from = object::MapCoordsOf(town);
	return Metres(town_stores::GetTemporaryResourceStorePotOrPos(town, from, ResourceType::Wood).pos);
}

void AssignTownFeatures()
{
	auto& registry = Locator::entitiesRegistry::value();
	// the town rectangles as AddStructureToTown / RemoveStructureFromTown left them (town_placement)
	const auto towns = town_queries::TownsNewestFirst();
	// MakeScenicForest for every town at the town rectangle's centre, altitude 0. Trees' MakeScenicForest takes metres
	// and makes its MapCoords again (map_coords::FromMetres): ExactMetres gives the metres that come back as the same
	// MapCoords
	for (const auto town : towns)
	{
		const auto id = registry.Get<const components::Town>(town).id;
		const auto centre = town_placement::GetTownAreaCentre(town);
		MakeScenicForest(id, glm::vec3(ExactMetres(centre.x), 0.0f, ExactMetres(centre.z)));
	}
	// AssignForestsToTown for every town, after all the scenic forests
	for (const auto town : towns)
	{
		const auto id = registry.Get<const components::Town>(town).id;
		AssignForestsToTown(id, ForestReference(town));
	}
}
} // namespace openblack::ecs::town_features
