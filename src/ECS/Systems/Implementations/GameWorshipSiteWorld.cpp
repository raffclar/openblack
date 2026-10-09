/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "GameWorshipSiteWorld.h"

#include <entt/core/hashed_string.hpp>

#include "3D/L3DMesh.h"
#include "3D/LandIslandInterface.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Villager.h"
#include "ECS/Registry.h"
#include "ECS/RegistryContext.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;

namespace
{
/// The worship site's model, laid out about the temple it stands at
constexpr auto k_SiteMesh = entt::hashed_string("temple/b_worship_l3d");
} // namespace

ecs::Registry& GameWorshipSiteWorld::Entities()
{
	return Locator::entitiesRegistry::value();
}

int32_t GameWorshipSiteWorld::LandNumber() const
{
	return Locator::entitiesRegistry::value().Context().mapScriptGlobals.landNumber;
}

std::optional<glm::vec3> GameWorshipSiteWorld::SitePoint(uint32_t index) const
{
	if (!Locator::resources::has_value())
	{
		return std::nullopt;
	}
	auto& meshes = Locator::resources::value().GetMeshes();
	if (!meshes.Contains(k_SiteMesh.value()))
	{
		return std::nullopt;
	}
	const auto& points = meshes.Handle(k_SiteMesh.value())->GetExtraMetrics();
	if (index >= points.size())
	{
		return std::nullopt;
	}
	return glm::vec3(points.at(index)[3]);
}

entt::id_type GameWorshipSiteWorld::SiteMesh() const
{
	return k_SiteMesh.value();
}

entt::id_type GameWorshipSiteWorld::AltarMesh(Tribe tribe) const
{
	const auto& sites = Locator::infoConstants::value().worshipSite;
	const auto row = static_cast<size_t>(tribe);
	return resources::HashIdentifier(row < sites.size() ? sites.at(row).meshType : MeshId::Dummy);
}

float GameWorshipSiteWorld::LandHeightAt(glm::vec2 point) const
{
	return Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetHeightAt(point) : 0.0f;
}

uint32_t GameWorshipSiteWorld::PopulationOf(entt::entity town) const
{
	const auto& registry = Locator::entitiesRegistry::value();
	auto population = static_cast<uint32_t>(registry.Get<const Town>(town).homelessVillagers.size());
	registry.Each<const Villager>([town, &population](entt::entity /*unused*/, const Villager& villager) {
		if (villager.town == town)
		{
			++population;
		}
	});
	return population;
}
