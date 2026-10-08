/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "FieldOfView.h"

#include <cmath>

#include <algorithm>

#include <glm/geometric.hpp>
#include <glm/mat4x4.hpp>

#include "3D/AffineMatrix.h"
#include "3D/Billboard.h"
#include "3D/L3DMesh.h"
#include "3D/ObjectMatrix.h"
#include "Camera/Camera.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "Locator.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"
#include "Windowing/WindowingInterface.h"

namespace openblack::field_of_view
{
using graphics::region_on_screen::PointOnScreen;
using graphics::region_on_screen::SphereOnScreen;

namespace
{
/// (pending) openblack has no temple interior state yet
bool InsideTemple()
{
	return false;
}
} // namespace

bool CurrentView(View& out)
{
	if (!Locator::camera::has_value() || !Locator::windowing::has_value())
	{
		return false;
	}
	const auto& camera = Locator::camera::value();
	const auto frame = graphics::billboard::CameraFrame::From(camera);
	// The world-to-clipping matrix (affine::FrameMatrices) and the near plane
	out.worldToClipping = frame.clipMatrices.worldToClipping;
	out.nearW = frame.nearZ;
	out.screen = Locator::windowing::value().GetSize();
	out.eye = camera.GetOrigin(); // the drawn camera
	out.tanHalfFov = std::tan(camera.GetHorizontalFieldOfView() * 0.5f);
	return out.screen.x > 0 && out.screen.y > 0;
}

bool PosInView(const glm::vec3& point)
{
	View view;
	if (InsideTemple() || !CurrentView(view))
	{
		return false;
	}
	return PointOnScreen(view, point);
}

bool ThingInView(entt::entity thing)
{
	auto& registry = Locator::entitiesRegistry::value();
	View view;
	// Inside the temple or no thing -> false
	if (thing == entt::null || !registry.Valid(thing) || InsideTemple() || !CurrentView(view))
	{
		return false;
	}
	const auto* transform = registry.TryGet<const ecs::components::Transform>(thing);
	if (transform == nullptr)
	{
		return false; // (approximate) a thing without a place (a timer): the original would test (0, altitude, 0)
	}
	if (registry.AllOf<ecs::components::Mesh>(thing))
	{
		return ObjectOnScreen(thing);
	}
	// (x, altitude + the height offset, z) of a thing with a position that is not an Object
	return PointOnScreen(view, transform->position);
}

bool ObjectOnScreen(entt::entity thing)
{
	auto& registry = Locator::entitiesRegistry::value();
	View view;
	if (thing == entt::null || !registry.Valid(thing) || !CurrentView(view))
	{
		return false;
	}
	const auto* transform = registry.TryGet<const ecs::components::Transform>(thing);
	const auto* mesh = registry.TryGet<const ecs::components::Mesh>(thing);
	if (transform == nullptr || mesh == nullptr)
	{
		return false;
	}
	// The 3D object's mesh, none -> false
	auto& meshes = Locator::resources::value().GetMeshes();
	if (!meshes.Contains(mesh->id))
	{
		return false;
	}
	const auto box = meshes.Handle(mesh->id)->GetBoundingBox();
	// T(p) R S as the original's Set* write it (only the zeros' signs differ from the glm product it replaces)
	const glm::mat4 model = affine::Model(*transform);
	// The box centre in the original's order; (approximate) the cells are openblack's model's
	const glm::vec3 centre = affine::BoxCentreThroughObject(affine::FromModel(model), box.Center());
	const float scale = std::max({transform->scale.x, transform->scale.y, transform->scale.z});
	const float radius = glm::length(box.Size()) * 0.5f * scale; // (approximate) the bounding radius
	return SphereOnScreen(view, centre, radius, transform->position);
}

} // namespace openblack::field_of_view
