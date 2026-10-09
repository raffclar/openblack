/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureHome.h"

#include "3D/L3DMesh.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureLeash.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack::ecs;
using namespace openblack::ecs::components;

namespace
{
/// Where a temple keeps its player's creature: the first place its mesh marks, turned and moved with the temple, or the
/// temple itself when its mesh marks none
glm::vec3 TempleCreatureHome(const Transform& transform, const Mesh* mesh)
{
	if (mesh != nullptr && openblack::Locator::resources::has_value())
	{
		const auto& meshes = openblack::Locator::resources::value().GetMeshes();
		if (meshes.Contains(mesh->id) && !meshes.Handle(mesh->id)->GetExtraMetrics().empty())
		{
			const auto local = glm::vec3(meshes.Handle(mesh->id)->GetExtraMetrics().front()[3]);
			return transform.position + (transform.rotation * (local * transform.scale));
		}
	}
	return transform.position;
}
} // namespace

std::optional<glm::vec3> creature_home::HomeOf(const Registry& registry, entt::entity creature)
{
	if (const auto* leash = registry.TryGet<const CreatureLeash>(creature); leash != nullptr && leash->home.has_value())
	{
		return leash->home;
	}
	const auto* body = registry.TryGet<const Creature>(creature);
	if (body == nullptr)
	{
		return std::nullopt;
	}
	std::optional<glm::vec3> temple;
	registry.Each<const Temple, const Transform>(
	    [&registry, &temple, owner = body->owner](entt::entity entity, const Temple& building, const Transform& transform) {
		    if (building.owner == owner && !temple.has_value())
		    {
			    temple = TempleCreatureHome(transform, registry.TryGet<const Mesh>(entity));
		    }
	    });
	return temple;
}
