/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "VillageTotemArchetype.h"

#include <glm/glm.hpp>

#include "3D/AllMeshes.h"
#include "3D/L3DMesh.h"
#include "3D/LandIslandInterface.h"
#include "Audio/Sound.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/SoundTag.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/VillageTotem.h"
#include "ECS/Registry.h"
#include "ECS/Systems/VillageTotemSystemInterface.h"
#include "ECS/VillageTotem.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::ecs::archetypes;
using namespace openblack::ecs::components;

namespace
{
/// The town centre's model marks where its totem stands with its seventh point
constexpr size_t k_TotemPoint = 6;
} // namespace

entt::entity VillageTotemArchetype::Create(entt::entity townCentre, const GAbodeInfo& info)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto tribe = static_cast<size_t>(info.tribeType);
	const auto& statues = Locator::infoConstants::value().totemStatue;
	auto& meshes = Locator::resources::value().GetMeshes();
	const auto centreMesh = resources::HashIdentifier(info.meshId);
	if (info.tribeType == Tribe::NONE || tribe >= statues.size() || !meshes.Contains(centreMesh))
	{
		return entt::null;
	}
	const auto& points = meshes.Handle(centreMesh)->GetExtraMetrics();
	if (points.size() <= k_TotemPoint)
	{
		return entt::null;
	}
	const auto& centre = registry.Get<const Transform>(townCentre);
	auto point = centre.position + centre.rotation * (glm::vec3(points[k_TotemPoint][3]) * centre.scale);
	// The town centre's model follows the land, its points with it: raised by how much higher the land is there than
	// under the centre
	if (Locator::terrainSystem::has_value())
	{
		const auto& land = Locator::terrainSystem::value();
		point.y += land.GetHeightAt({point.x, point.z}) - land.GetHeightAt({centre.position.x, centre.position.z});
	}

	const auto totem = registry.Create();
	registry.Assign<Transform>(totem, point, centre.rotation, centre.scale);
	registry.Assign<Mesh>(totem, resources::HashIdentifier(statues.at(tribe).plinth), static_cast<int8_t>(0),
	                      static_cast<int8_t>(0));
	// It grinds while it eases to a new height
	registry.Assign<SoundTag>(totem, static_cast<entt::id_type>(audio::SoundId::G_TotumMove), glm::vec3(0.0f), false);

	const auto icon = registry.Create();
	registry.Assign<Transform>(icon, point + glm::vec3(0.0f, village_totem::k_IconAbovePlinth, 0.0f), centre.rotation,
	                           centre.scale);
	registry.Assign<Mesh>(icon, resources::HashIdentifier(MeshId::BuildingSpellHand), static_cast<int8_t>(0),
	                      static_cast<int8_t>(0));

	registry.Assign<VillageTotem>(totem, VillageTotem {.townCentre = townCentre, .icon = icon, .restY = point.y});
	if (Locator::villageTotemSystem::has_value())
	{
		Locator::villageTotemSystem::value().AddToPlayer(totem);
	}
	return totem;
}
