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
#include "Creature/CreatureMode.h"
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
/// Where a temple keeps its player's creature: the pen its mesh marks in front of it, turned and moved with the temple.
/// None when its mesh marks no pen
std::optional<glm::vec3> TemplePenOf(const Transform& transform, const Mesh* mesh)
{
	if (mesh == nullptr || !openblack::Locator::resources::has_value())
	{
		return std::nullopt;
	}
	const auto& meshes = openblack::Locator::resources::value().GetMeshes();
	if (!meshes.Contains(mesh->id))
	{
		return std::nullopt;
	}
	return openblack::creature_mode::TemplePen(transform.position, transform.rotation, transform.scale,
	                                           meshes.Handle(mesh->id)->GetExtraMetrics());
}
} // namespace

std::optional<glm::vec3> creature_home::HomeOf(const Registry& registry, entt::entity creature)
{
	const auto* body = registry.TryGet<const Creature>(creature);
	if (body == nullptr)
	{
		return std::nullopt;
	}
	const auto* leash = registry.TryGet<const CreatureLeash>(creature);
	const auto home = leash != nullptr ? leash->home : std::nullopt;
	std::optional<glm::vec3> temple;
	registry.Each<const Temple, const Transform>(
	    [&registry, &temple, owner = body->owner](entt::entity entity, const Temple& building, const Transform& transform) {
		    if (building.owner == owner && !temple.has_value())
		    {
			    temple = TemplePenOf(transform, registry.TryGet<const Mesh>(entity));
		    }
	    });
	if (!temple.has_value() && !home.has_value())
	{
		return std::nullopt;
	}
	// The temple's pen while the temple stands, whatever home it was given; else that home
	return openblack::creature_mode::PenOf(home, temple, glm::vec3 {});
}
