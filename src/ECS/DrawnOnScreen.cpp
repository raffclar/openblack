/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "DrawnOnScreen.h"

#include <cmath>

#include "3D/L3DMesh.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/Systems/ObjectDrawListSystemInterface.h"
#include "Graphics/MeshDetail.h"
#include "Locator.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"
#include "Scenery/ObjectDrawList.h"

namespace openblack::ecs
{

bool DrawnOnScreen(const Registry& registry, entt::entity entity)
{
	using components::Mesh;
	using components::Transform;
	if (!Locator::objectDrawListSystem::has_value())
	{
		return true;
	}
	const auto& drawList = Locator::objectDrawListSystem::value();
	if (!drawList.Drawn(registry, entity))
	{
		return false;
	}
	const auto& view = drawList.GetView();
	const auto* transform = registry.TryGet<const Transform>(entity);
	const auto* mesh = registry.TryGet<const Mesh>(entity);
	if (!view.has_value() || transform == nullptr || mesh == nullptr || !Locator::resources::has_value())
	{
		return true;
	}
	const auto& meshes = Locator::resources::value().GetMeshes();
	if (!meshes.Contains(mesh->id))
	{
		return true;
	}
	// The sphere round the box of the thing's model, where the frame draws the thing
	const auto box = meshes.Handle(mesh->id)->GetBoundingBox();
	const auto centre = transform->position + (transform->rotation * (transform->scale * box.Center()));
	const float radius = std::abs(transform->scale.x) * graphics::mesh_detail::HalfDiagonal(box.Size());
	return object_draw_list::SphereOnScreen(*view, transform->position, centre, radius);
}

} // namespace openblack::ecs
