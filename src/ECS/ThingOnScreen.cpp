/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ThingOnScreen.h"

#include <glm/geometric.hpp>
#include <glm/gtc/matrix_inverse.hpp>

#include "3D/L3DMesh.h"
#include "3D/MapCoords.h"
#include "Camera/Camera.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"
#include "Windowing/WindowingInterface.h"

using namespace openblack;
using namespace openblack::ecs::components;

std::optional<screen_pick::View> ecs::thing_on_screen::ScreenView()
{
	if (!Locator::camera::has_value() || !Locator::windowing::has_value())
	{
		return std::nullopt;
	}
	const auto size = Locator::windowing::value().GetSize();
	if (size.x <= 0 || size.y <= 0)
	{
		return std::nullopt;
	}
	const auto& camera = Locator::camera::value();
	return screen_pick::FourByThreeMiddle({
	    .worldToClip = camera.GetViewProjectionMatrix(),
	    .resolution = glm::vec2(size),
	    .near = camera.GetNearClip(),
	    .xScale = camera.GetProjectionMatrix()[0][0],
	    .camera = glm::vec3(glm::inverse(camera.GetViewMatrix(Camera::Interpolation::Current))[3]),
	});
}

/// Whether a thing shows on the screen: a thing with a model by its model's bounding sphere, anything else by the point
/// it stands at
bool ecs::thing_on_screen::OnScreen(const screen_pick::View& view, entt::entity thing)
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* transform = registry.TryGet<const Transform>(thing);
	if (transform == nullptr)
	{
		return false;
	}
	if (const auto* mesh = registry.TryGet<const Mesh>(thing); mesh != nullptr)
	{
		if (!Locator::resources::has_value() || !Locator::resources::value().GetMeshes().Contains(mesh->id))
		{
			return false;
		}
		const auto box = Locator::resources::value().GetMeshes().Handle(mesh->id)->GetBoundingBox();
		// The sphere about the model's box: its centre placed as the thing is, its radius the half diagonal times the
		// thing's size
		const auto centre = transform->position + transform->rotation * (transform->scale * box.Center());
		const float radius = glm::length(box.Size() * 0.5f) * transform->scale.x;
		return screen_pick::SphereOnScreen(view, centre, radius, transform->position);
	}
	// Where it stands, across the land as a map position holds it
	const auto at = glm::vec3(map_coords::Quantise(transform->position.x), transform->position.y,
	                          map_coords::Quantise(transform->position.z));
	return screen_pick::PointOnScreen(view, at);
}
