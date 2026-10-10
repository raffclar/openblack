/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "TipBubbleSystem.h"

#include <algorithm>

#include <glm/geometric.hpp>
#include <glm/mat4x4.hpp>

#include "3D/L3DMesh.h"
#include "Camera/Camera.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/Systems/CinematicDirectorSystemInterface.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"
#include "Windowing/WindowingInterface.h"

using namespace openblack;
using namespace openblack::ecs::systems;
using namespace openblack::help::tip_bubble;

namespace
{
/// The bubble points at three quarters of the sign's height
constexpr float k_AnchorHeightShare = 0.75f;

/// The sign's model's box, in its own space
struct ModelBox
{
	glm::vec3 centre {0.0f};
	glm::vec3 halfSize {0.0f};
};

std::optional<ModelBox> BoxOf(const ecs::components::Mesh& mesh)
{
	if (!Locator::resources::has_value())
	{
		return std::nullopt;
	}
	const auto& meshes = Locator::resources::value().GetMeshes();
	if (!meshes.Contains(mesh.id))
	{
		return std::nullopt;
	}
	const auto box = meshes.Handle(mesh.id)->GetBoundingBox();
	return ModelBox {.centre = box.Center(), .halfSize = box.Size() * 0.5f};
}

/// A script task has the cinema bars in
bool InScriptCutScene()
{
	if (!Locator::cinematicDirectorSystem::has_value())
	{
		return false;
	}
	const auto& director = Locator::cinematicDirectorSystem::value();
	return director.IsWideScreenOn() && director.GetWideScreenOwner() != 0;
}

/// The sign, while it is still there
const ecs::components::Transform* SignTransform(entt::entity sign)
{
	if (sign == entt::null || !Locator::entitiesRegistry::has_value())
	{
		return nullptr;
	}
	const auto& registry = Locator::entitiesRegistry::value();
	return registry.Valid(sign) ? registry.TryGet<const ecs::components::Transform>(sign) : nullptr;
}

/// Whether the sign's model is on the screen, as the bubble judges it
bool SignOnScreen(entt::entity sign, const ecs::components::Transform& transform)
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* mesh = registry.TryGet<const ecs::components::Mesh>(sign);
	const auto box = mesh != nullptr ? BoxOf(*mesh) : std::nullopt;
	if (!box.has_value() || !Locator::camera::has_value() || !Locator::windowing::has_value())
	{
		return false;
	}
	const auto& camera = Locator::camera::value();
	const auto screen = glm::vec2(Locator::windowing::value().GetSize());
	const auto viewProjection = camera.GetViewProjectionMatrix();
	const float scale = transform.scale.x;
	const View view {
	    .eye = camera.GetOrigin(),
	    .project =
	        [&viewProjection, screen](const glm::vec3& point) {
		        const auto clip = viewProjection * glm::vec4(point, 1.0f);
		        return glm::vec3((clip.x / clip.w + 1.0f) * screen.x * 0.5f, (1.0f - clip.y / clip.w) * screen.y * 0.5f,
		                         clip.w);
	        },
	    .nearClip = camera.GetNearClip(),
	    .pixelsPerUnit = camera.GetProjectionMatrix()[0][0] * screen.x * 0.5f,
	    .screen = screen,
	};
	const Ball ball {
	    .centre = transform.position + (transform.rotation * (box->centre * scale)),
	    .radius = scale * glm::length(box->halfSize),
	    .origin = transform.position,
	};
	return OnScreen(ball, view);
}
} // namespace

void TipBubbleSystem::Show(entt::entity sign, uint32_t text)
{
	_sign = sign;
	_text = text;
	_scroll.lines = k_StartScroll;
	_scroll.contentHeight = 0.0f;
}

void TipBubbleSystem::Hide()
{
	_sign = entt::null;
}

void TipBubbleSystem::ProcessTurn()
{
	const auto* transform = SignTransform(_sign);
	if (transform != nullptr && SignOnScreen(_sign, *transform))
	{
		_displayTime = k_DisplayTime;
	}
	else
	{
		_sign = entt::null;
	}
}

void TipBubbleSystem::UpdateFrame(float gameMilliseconds)
{
	// The game's time counts in hundredths of a turn of 100 ms: one a millisecond
	if (IsUp())
	{
		_displayTime = std::max(_displayTime - (gameMilliseconds * k_DisplayTimePerHundredth), 0.0f);
	}
}

void TipBubbleSystem::Hover()
{
	_displayTime = k_DisplayTime;
}

bool TipBubbleSystem::IsUp() const
{
	return _sign != entt::null && !InScriptCutScene();
}

std::optional<glm::vec3> TipBubbleSystem::GetAnchor() const
{
	const auto* transform = SignTransform(_sign);
	if (transform == nullptr)
	{
		return std::nullopt;
	}
	const auto* mesh = Locator::entitiesRegistry::value().TryGet<const ecs::components::Mesh>(_sign);
	const auto box = mesh != nullptr ? BoxOf(*mesh) : std::nullopt;
	// The sign's height is its model's, scaled
	const float height = box.has_value() ? box->halfSize.y * transform->scale.x * 2.0f : 0.0f;
	return transform->position + glm::vec3(0.0f, height * k_AnchorHeightShare, 0.0f);
}

void TipBubbleSystem::Reset()
{
	_sign = entt::null;
	_text = 0;
}
