/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ScriptCameraModel.h"

#include <cmath>

#include <algorithm>

#include <glm/geometric.hpp>

using namespace openblack;
using namespace openblack::script_camera;

namespace
{
using Zoomers = std::array<camera::Zoomer, 3>;

void Place(Zoomers& zoomers, const glm::vec3& point)
{
	for (glm::length_t i = 0; i < 3; ++i)
	{
		zoomers.at(static_cast<size_t>(i)).SetPosition(point[i]);
	}
}

void Send(Zoomers& zoomers, const glm::vec3& point, float seconds)
{
	for (glm::length_t i = 0; i < 3; ++i)
	{
		zoomers.at(static_cast<size_t>(i)).SetDestination(point[i], 0.0f, seconds);
	}
}

glm::vec3 ValueOf(const Zoomers& zoomers)
{
	return {zoomers[0].Value(), zoomers[1].Value(), zoomers[2].Value()};
}

glm::vec3 DestinationOf(const Zoomers& zoomers)
{
	return {zoomers[0].Destination(), zoomers[1].Destination(), zoomers[2].Destination()};
}

float DistanceSquared(const glm::vec3& a, const glm::vec3& b)
{
	const auto d = a - b;
	return glm::dot(d, d);
}
} // namespace

std::optional<glm::vec3> script_camera::PullIntoWorld(const glm::vec3& destination)
{
	const auto fromCentre = destination - glm::vec3(k_WorldCentre, 0.0f, k_WorldCentre);
	const float distanceSquared = glm::dot(fromCentre, fromCentre);
	if (!(k_WorldRadiusSquared < distanceSquared))
	{
		return std::nullopt;
	}
	const float scale = 1.0f / (std::sqrt(distanceSquared) * k_WorldScale);
	return glm::vec3(fromCentre.x * scale + k_WorldCentre, fromCentre.y * scale, fromCentre.z * scale + k_WorldCentre);
}

View script_camera::Drawn(const View& camera, const std::function<float(float x, float z)>& groundAt)
{
	auto view = camera;
	// A camera where it looks would look nowhere
	if (DistanceSquared(view.origin, view.focus) < k_ArrivedDistanceSquared)
	{
		view.origin.x -= 1.0f;
		view.origin.y += 1.0f;
	}
	const float below = groundAt(view.origin.x, view.origin.z) + k_GroundClearance - view.origin.y;
	if (below > 0.0f)
	{
		view.origin.y += below;
		view.focus.y += below;
	}
	return view;
}

ScriptCameraModel::ScriptCameraModel(const glm::vec3& origin, const glm::vec3& focus, GroundHeight groundAt)
    : _groundAt(std::move(groundAt))
{
	Place(_origin, origin);
	Place(_focus, focus);
}

void ScriptCameraModel::SetOrigin(const glm::vec3& origin)
{
	Place(_origin, origin);
}

void ScriptCameraModel::SetFocus(const glm::vec3& focus)
{
	Place(_focus, focus);
}

void ScriptCameraModel::MoveOrigin(const glm::vec3& origin, float seconds)
{
	Send(_origin, origin, seconds);
}

void ScriptCameraModel::MoveFocus(const glm::vec3& focus, float seconds)
{
	Send(_focus, focus, seconds);
}

bool ScriptCameraModel::Arrived() const
{
	return DistanceSquared(DestinationOf(_origin), ValueOf(_origin)) < k_ArrivedDistanceSquared &&
	       DistanceSquared(DestinationOf(_focus), ValueOf(_focus)) < k_ArrivedDistanceSquared;
}

std::optional<CameraModel::CameraInterpolationUpdateInfo> ScriptCameraModel::Update(std::chrono::microseconds dt,
                                                                                    const Camera& /*camera*/)
{
	const float seconds = std::min(std::chrono::duration<float>(dt).count(), k_MaxFrameSeconds);
	for (auto& zoomer : _origin)
	{
		zoomer.Update(seconds);
	}
	for (auto& zoomer : _focus)
	{
		zoomer.Update(seconds);
	}
	const auto view = Drawn({.origin = ValueOf(_origin), .focus = ValueOf(_focus)}, _groundAt);
	// Sent out of the world, it turns back to its edge
	if (const auto pulled = PullIntoWorld(DestinationOf(_origin)); pulled.has_value())
	{
		Send(_origin, *pulled, k_WorldReturnSeconds);
	}
	// The camera is drawn where the coordinates are, without a glide of its own
	return CameraInterpolationUpdateInfo {
	    .origin = view.origin, .focus = view.focus, .duration = std::chrono::microseconds::zero()};
}

void ScriptCameraModel::HandleActions(std::chrono::microseconds /*dt*/)
{
	// The player has no say while a script has the camera
}

void ScriptCameraModel::SetFlight(glm::vec3 /*origin*/, glm::vec3 /*focus*/)
{
	// Nor do the player's flights to places
}

glm::vec3 ScriptCameraModel::GetTargetOrigin() const
{
	return DestinationOf(_origin);
}

glm::vec3 ScriptCameraModel::GetTargetFocus() const
{
	return DestinationOf(_focus);
}

std::chrono::seconds ScriptCameraModel::GetIdleTime() const
{
	return std::chrono::seconds::zero();
}
