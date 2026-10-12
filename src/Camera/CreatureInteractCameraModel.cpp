/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureInteractCameraModel.h"

#include <glm/geometric.hpp>
#include <glm/gtx/euler_angles.hpp>

using namespace openblack;

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

void Advance(Zoomers& zoomers, float seconds)
{
	for (auto& zoomer : zoomers)
	{
		zoomer.Update(seconds);
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
} // namespace

creature_interact::View creature_interact::ViewOf(const glm::vec3& position, float height, float heading)
{
	const auto focus = position + glm::vec3(0.0f, k_FocusHeightShare * height, 0.0f);
	const auto ahead = glm::vec3(0.0f, k_Up - (k_UpLessPerHeight * height), -k_Ahead);
	const auto offset = glm::normalize(ahead) * (k_DistanceHeights * height);
	return {.origin = focus + (glm::mat3(glm::eulerAngleY(heading)) * offset), .focus = focus};
}

bool creature_interact::WooshesFlyingBack(const glm::vec3& from, const glm::vec3& to)
{
	return glm::distance(from, to) > k_WooshDistance;
}

CreatureInteractCameraModel::CreatureInteractCameraModel(glm::vec3 origin, glm::vec3 focus)
{
	Place(_origin, origin);
	Place(_focus, focus);
}

std::optional<CameraModel::CameraInterpolationUpdateInfo> CreatureInteractCameraModel::Update(std::chrono::microseconds dt,
                                                                                              const Camera& /*camera*/)
{
	// Each frame it sets off afresh for where the creature now wants it
	if (_target.has_value())
	{
		Send(_origin, _target->origin, creature_interact::k_EaseSeconds);
		Send(_focus, _target->focus, creature_interact::k_EaseSeconds);
	}
	const auto seconds = std::chrono::duration<float>(dt).count();
	Advance(_origin, seconds);
	Advance(_focus, seconds);
	return CameraInterpolationUpdateInfo {
	    .origin = ValueOf(_origin), .focus = ValueOf(_focus), .duration = std::chrono::microseconds::zero()};
}

void CreatureInteractCameraModel::SetFlight(glm::vec3 origin, glm::vec3 focus)
{
	_target = creature_interact::View {.origin = origin, .focus = focus};
}

glm::vec3 CreatureInteractCameraModel::GetTargetOrigin() const
{
	return DestinationOf(_origin);
}

glm::vec3 CreatureInteractCameraModel::GetTargetFocus() const
{
	return DestinationOf(_focus);
}
