/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Camera.h"

#include <cmath>

#include <utility>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <glm/gtx/intersect.hpp>
#include <glm/gtx/vec_swizzle.hpp>

#include "3D/LandIslandInterface.h"
#include "ECS/Registry.h"
#include "ECS/Systems/DynamicsSystemInterface.h"
#include "Input/GameActionMapInterface.h"
#include "Locator.h"
#include "ReflectionXZCamera.h"
#include "ScriptCamera.h"
#include "Windowing/WindowingInterface.h"

using namespace openblack;

namespace
{
constexpr auto k_DefaultCameraOriginOffset = glm::vec3(0.0f, 0.0f, 120.0f);
constexpr float k_MaxFrameSeconds = 0.1f; ///< The zoomers' longest step
/// Turns glm's perspective (depth -1 at the near plane, 1 at the far one) into reversed Z: depth' = (1 - depth) / 2, so
/// 1 at the near plane and 0 at the far one. Without the halving the near plane maps to 2 and everything closer than
/// about twice the near distance is clipped away, where the original clips at the near distance itself
constexpr auto k_ReverseZMatrix = glm::mat4(1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 0.f, -0.5f, 0.f, 0.f, 0.f, 0.5f, 1.f);
} // namespace

Camera::Camera(glm::vec3 focus)
    : _model(CameraModel::CreateModel(CameraModel::Model::DefaultWorld))
{
	_origin.SetPosition(focus + k_DefaultCameraOriginOffset);
	_focus.SetPosition(focus);
}

Camera::~Camera() = default;

float Camera::GetHorizontalFieldOfView() const
{
	return _xFov;
}

glm::mat4 Camera::GetRotationMatrix() const
{
	return static_cast<glm::mat3>(glm::transpose(GetViewMatrix(Camera::Interpolation::Current)));
}

glm::mat4 Camera::GetViewMatrix(Interpolation interpolation) const
{
	// the falling spell's drawn camera, over the look-at (which it overwrites)
	if (_drawnView.has_value())
	{
		return *_drawnView;
	}
	// Invert the camera's rotation (transposed) and position (negated) to get the view matrix.
	return glm::lookAt(GetOrigin(interpolation), GetFocus(interpolation), glm::vec3(0.0f, 1.0f, 0.0f));
}

glm::mat4 Camera::GetViewProjectionMatrix(Interpolation interpolation) const
{
	return GetProjectionMatrix() * GetViewMatrix(interpolation);
}

glm::mat4 Camera::GetViewProjectionMatrix(Projection projection, Interpolation interpolation) const
{
	return GetProjectionMatrix(projection) * GetViewMatrix(interpolation);
}

std::optional<ecs::components::Transform> Camera::RaycastMouseToLand(bool includeWater, Interpolation interpolation) const
{
	// get the hit by raycasting to the land down via the mouse
	const auto mousePosition = Locator::gameActionSystem::value().GetMousePosition();
	// headless (no window): a 1x1 screen, as GameActionMap's mouse clamp
	const auto screenSize = Locator::windowing::has_value() ? Locator::windowing::value().GetSize() : glm::ivec2(1, 1);
	return RaycastScreenCoordToLand(static_cast<glm::vec2>(mousePosition) / static_cast<glm::vec2>(screenSize), includeWater,
	                                interpolation);
}

std::optional<ecs::components::Transform> Camera::RaycastScreenCoordToLand(glm::vec2 screenCoord, bool includeWater,
                                                                           Interpolation interpolation) const
{
	// get the hit by raycasting to the land down via the pixel coordinate
	ecs::components::Transform intersectionTransform;
	float intersectDistance = 0.0f;
	glm::vec3 rayOrigin;
	glm::vec3 rayDirection;
	DeprojectScreenToWorld(screenCoord, rayOrigin, rayDirection, interpolation);
	const auto& dynamicsSystem = Locator::dynamicsSystem::value();
	if (auto hit = dynamicsSystem.RayCastClosestHit(rayOrigin, rayDirection, 1e10f))
	{
		intersectionTransform = hit->first;
		return std::make_optional(intersectionTransform);
	}
	if (includeWater && glm::intersectRayPlane(rayOrigin, rayDirection, glm::vec3(0.0f, 0.0f, 0.0f),
	                                           glm::vec3(0.0f, 1.0f, 0.0f), intersectDistance))
	{
		intersectionTransform.position = rayOrigin + rayDirection * intersectDistance;
		intersectionTransform.rotation = glm::mat3(1.0f);
		return std::make_optional(intersectionTransform);
	}
	return std::nullopt;
}

Camera& Camera::SetProjectionMatrixPerspective(float xFov, float aspect, float nearClip, float farClip)
{
	_xFov = glm::radians(xFov);
	_aspect = aspect;
	const float yFov = (glm::atan(glm::tan(_xFov / 2.0f) / aspect)) * 2.0f;
	// The reversed Z one (k_ReverseZMatrix) is what the draws use; the normal one stays for picking and projecting
	_projectionMatrix = glm::perspective(yFov, aspect, nearClip, farClip);
	_projectionMatrixReversedZ = k_ReverseZMatrix * _projectionMatrix;

	return *this;
}

std::chrono::microseconds Camera::GetInterpolatorTime() const
{
	return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::duration<float>(_origin.axis[0].time));
}

glm::vec3 Camera::GetForward() const
{
	// Forward is +1 in openblack but is -1 in OpenGL
	return static_cast<glm::mat3>(GetRotationMatrix()) * glm::vec3(0.0f, 0.0f, 1.0f);
}

glm::vec3 Camera::GetRight() const
{
	return static_cast<glm::mat3>(GetRotationMatrix()) * glm::vec3(1.0f, 0.0f, 0.0f);
}

glm::vec3 Camera::GetUp() const
{
	return static_cast<glm::mat3>(GetRotationMatrix()) * glm::vec3(0.0f, 1.0f, 0.0f);
}

std::unique_ptr<Camera> Camera::Reflect() const
{
	// TODO(bwrsandman): The copy to reflection camera has way too much of Camera including model which is useless
	//                   This also touches on other cameras such as the citadel camera which use a different kind of model
	auto reflectionCamera = std::make_unique<ReflectionXZCamera>();
	(*reflectionCamera).SetOrigin(GetOrigin()).SetFocus(GetFocus()).SetProjectionMatrix(_projectionMatrix);
	return reflectionCamera;
}

void Camera::DeprojectScreenToWorld(glm::vec2 screenCoord, glm::vec3& outWorldOrigin, glm::vec3& outWorldDirection,
                                    Interpolation interpolation) const
{
	const float screenSpaceX = (screenCoord.x - 0.5f) * 2.0f;
	const float screenSpaceY = ((1.0f - screenCoord.y) - 0.5f) * 2.0f;

	// The start of the ray trace is defined to be at mousex,mousey,1 in
	// projection space (z=1 is near, z=0 is far - this gives us better
	// precision) To get the direction of the ray trace we need to use any z
	// between the near and the far plane, so let's use (mousex, mousey, 0.5)
	const glm::vec4 rayStartProjectionSpace = glm::vec4(screenSpaceX, screenSpaceY, 0.0f, 1.0f);
	const glm::vec4 rayEndProjectionSpace = glm::vec4(screenSpaceX, screenSpaceY, 0.5f, 1.0f);

	// Calculate our inverse view projection matrix
	auto inverseViewProj = glm::inverse(GetViewProjectionMatrix(Projection::Normal, interpolation));

	// Get our homogeneous coordinates for our start and end ray positions
	const glm::vec4 hgRayStartWorldSpace = inverseViewProj * rayStartProjectionSpace;
	const glm::vec4 hgRayEndWorldSpace = inverseViewProj * rayEndProjectionSpace;

	glm::vec3 rayStartWorldSpace(hgRayStartWorldSpace.x, hgRayStartWorldSpace.y, hgRayStartWorldSpace.z);
	glm::vec3 rayEndWorldSpace(hgRayEndWorldSpace.x, hgRayEndWorldSpace.y, hgRayEndWorldSpace.z);

	// divide vectors by W to undo any projection and get the 3-space coord
	if (hgRayStartWorldSpace.w != 0.0f)
	{
		rayStartWorldSpace /= hgRayStartWorldSpace.w;
	}

	if (hgRayEndWorldSpace.w != 0.0f)
	{
		rayEndWorldSpace /= hgRayEndWorldSpace.w;
	}

	const glm::vec3 rayDirWorldSpace = glm::normalize(rayEndWorldSpace - rayStartWorldSpace);

	// finally, store the results in the outputs
	outWorldOrigin = rayStartWorldSpace;
	outWorldDirection = rayDirWorldSpace;
}

bool Camera::ProjectWorldToScreen(glm::vec3 worldPosition, glm::vec4 viewport, glm::vec3& outScreenPosition,
                                  Interpolation interpolation) const
{
	if (glm::all(glm::epsilonEqual(worldPosition, GetOrigin(), glm::epsilon<float>())))
	{
		outScreenPosition = glm::vec3(glm::lerp(glm::xy(viewport), glm::zw(viewport), {0.5f, 0.5f}), 0.0f);
		return true; // Right at the camera position
	}
	outScreenPosition =
	    glm::project(worldPosition, GetViewMatrix(interpolation), GetProjectionMatrix(Projection::Normal), viewport);
	if (outScreenPosition.x < viewport.x || outScreenPosition.y < viewport.y || glm::round(outScreenPosition.x) > viewport.z ||
	    glm::round(outScreenPosition.y) > viewport.w)
	{
		return false; // Outside viewport bounds
	}
	if (outScreenPosition.z > 1.0f)
	{
		return false; // Clipped
	}
	if (outScreenPosition.z < 0.0f)
	{
		return false; // Behind Camera
	}
	outScreenPosition.y = viewport.w - (outScreenPosition.y - viewport.y) + viewport.y;
	return true;
}

void Camera::Update(std::chrono::microseconds dt)
{
	const auto updateInfo = _model->Update(dt, *this);
	// (approximate) the original's dt is the camera's integer ms step times 0.001
	UpdateZoomers(updateInfo, std::chrono::duration<float>(dt).count());
}

void Camera::UpdateZoomers(const std::optional<CameraModel::CameraInterpolationUpdateInfo>& updateInfo, float seconds)
{
	if (updateInfo)
	{
		// from the current value and speed to the target with speed 0
		const float duration = std::chrono::duration<float>(updateInfo->duration).count();
		_origin.SetDestinationWithTime(updateInfo->origin, duration);
		_focus.SetDestinationWithTime(updateInfo->focus, duration);
	}
	const float dt = std::min(seconds, k_MaxFrameSeconds);
	_origin.Update(dt);
	_focus.Update(dt);
	// a position destination outside the world's sphere is pulled back to it, in every camera mode: the clamp keeps the
	// open world's camera on the world disc. A camera model with a lens of its own, as the temple's, looks at a scene of
	// its own away from the disc (the temple sits at the origin), so its camera is left where it is
	if (_model->GetLens().has_value())
	{
		return;
	}
	const glm::vec3 centre(script_camera::k_DiscCentre, 0.0f, script_camera::k_DiscCentre);
	const auto d = _origin.GetDestination() - centre;
	if (const float d2 = glm::dot(d, d); d2 > script_camera::k_DiscRadiusSquared)
	{
		_origin.SetDestinationWithTime(d / (std::sqrt(d2) * script_camera::k_DiscScale) + centre, script_camera::k_DiscSeconds);
	}
}

void Camera::HandleActions(std::chrono::microseconds dt)
{
	_model->HandleActions(dt);
}

std::unique_ptr<CameraModel> Camera::SetModel(std::unique_ptr<CameraModel> model)
{
	std::swap(_model, model);
	return model;
}

const glm::mat4& Camera::GetProjectionMatrix() const
{
	return GetProjectionMatrix(_cameraProjection);
}

const glm::mat4& Camera::GetProjectionMatrix(Projection projection) const
{
	switch (projection)
	{
	case Projection::Normal:
		return _projectionMatrix;
	case Projection::ReversedZ:
		return _projectionMatrixReversedZ;
	default:
		assert(false);
		std::unreachable();
	}
}

glm::vec3 Camera::GetOrigin(Interpolation interpolation) const
{
	switch (interpolation)
	{
	case Interpolation::Current:
		return _origin.GetCurrentValue() + _originDrawOffset; // the shake, drawn only
	case Interpolation::Start:
		return _origin.GetStartValue();
	case Interpolation::Target:
		return _origin.GetDestination();
	default:
		assert(false);
		std::unreachable();
	}
}

glm::vec3 Camera::GetFocus(Interpolation interpolation) const
{
	switch (interpolation)
	{
	case Interpolation::Current:
		return _focus.GetCurrentValue() + _focusDrawOffset; // the shake, drawn only
	case Interpolation::Start:
		return _focus.GetStartValue();
	case Interpolation::Target:
		return _focus.GetDestination();
	default:
		assert(false);
		std::unreachable();
	}
}

glm::vec3 Camera::GetOriginVelocity(Interpolation interpolation) const
{
	switch (interpolation)
	{
	case Interpolation::Current:
		return _origin.GetSpeed();
	case Interpolation::Start:
		return _origin.GetStartSpeed();
	case Interpolation::Target:
		return _origin.GetDestinationSpeed();
	default:
		assert(false);
		std::unreachable();
	}
}

glm::vec3 Camera::GetFocusVelocity(Interpolation interpolation) const
{
	switch (interpolation)
	{
	case Interpolation::Current:
		return _focus.GetSpeed();
	case Interpolation::Start:
		return _focus.GetStartSpeed();
	case Interpolation::Target:
		return _focus.GetDestinationSpeed();
	default:
		assert(false);
		std::unreachable();
	}
}

glm::vec3 Camera::GetRotation() const
{
	// Extract the yaw, pitch, and roll angles from the rotation matrix
	float yaw;
	float pitch;
	float roll;

	// Use the GLM function `extractEulerAngleYXZ` to get the Euler angles
	glm::extractEulerAngleZYX(GetRotationMatrix(), roll, yaw, pitch);
	// Fix angles to make sure roll is 0, it tends to flip to 180 and -180
	if (roll > glm::radians(90.0f) || roll < glm::radians(-90.0f))
	{
		pitch -= glm::radians(180.0f) * glm::sign(pitch);
		yaw = glm::radians(180.0f) - yaw;
	}
	return {pitch, yaw, 0.0f};
}

Camera& Camera::SetOrigin(const glm::vec3& position)
{
	_origin.SetPosition(position); // on each axis

	return *this;
}

Camera& Camera::SetFocus(const glm::vec3& position)
{
	_focus.SetPosition(position);

	return *this;
}

Camera& Camera::SetProjectionMatrix(const glm::mat4& projection)
{
	_projectionMatrix = projection;
	_projectionMatrixReversedZ = k_ReverseZMatrix * _projectionMatrix;

	return *this;
}
Camera::Projection Camera::GetCameraProjection() const
{
	return _cameraProjection;
}

void Camera::CopyViewTo(Camera& out) const
{
	out._origin = _origin;
	out._focus = _focus;
	out._originDrawOffset = _originDrawOffset;
	out._focusDrawOffset = _focusDrawOffset;
	out._drawnView = _drawnView;
	out._xFov = _xFov;
	out._aspect = _aspect;
	out._projectionMatrix = _projectionMatrix;
	out._projectionMatrixReversedZ = _projectionMatrixReversedZ;
	out._cameraProjection = _cameraProjection;
}
