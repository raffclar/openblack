/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "GestureInput.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>

#include <glm/geometric.hpp>
#include <glm/gtx/intersect.hpp>

#include "Camera/Camera.h"
#include "ECS/Components/Transform.h"
#include "ECS/Systems/HandMagicStateInterface.h"
#include "Game.h"
#include "Input/GameCursor.h"
#include "Locator.h"
#include "PowerUpSystem.h"
#include "Windowing/WindowingInterface.h"

using namespace openblack;
using namespace openblack::magic::gestures;

namespace
{
/// Past this many ms of mouse events a position message goes
constexpr float k_MessageMs = 28.0f;

/// The mouse sampling, the camera-moved flag and a scripted stroke being played; never cleared on a land load
struct GestureSamplingState
{
	std::optional<glm::vec3> lastCamera;
	bool cameraMoving {false};
	std::optional<glm::ivec2> lastMouse;
	float mouseMs {0.0f};
	std::vector<glm::ivec2> stroke;
	size_t strokeNext {0};
};

/// This module's state (Locator::handMagicState)
GestureSamplingState& Sampling()
{
	if (!Locator::handMagicState::has_value())
	{
		std::fputs("magic::gestures::sampling: no hand magic state in the locator (Locator::handMagicState)\n", stderr);
		std::abort();
	}
	return Locator::handMagicState::value().Get<GestureSamplingState>();
}

glm::vec2 ScreenSize()
{
	if (!Locator::windowing::has_value())
	{
		return {1.0f, 1.0f};
	}
	const auto size = Locator::windowing::value().GetSize();
	return {static_cast<float>(std::max(1, size.x)), static_cast<float>(std::max(1, size.y))};
}

void Ray(glm::vec2 pixel, glm::vec3& origin, glm::vec3& direction)
{
	Locator::camera::value().DeprojectScreenToWorld(pixel / ScreenSize(), origin, direction);
}
} // namespace

float sampling::ScreenRatio()
{
	const auto size = ScreenSize();
	return size.x / size.y;
}

std::optional<glm::vec3> sampling::ScreenToLand(glm::vec2 pixel)
{
	if (!Locator::camera::has_value())
	{
		return std::nullopt;
	}
	// as Game::Update finds the point under the cursor: the land or sea under the pixel, as the game finds it
	const auto land = Locator::camera::value().RaycastScreenCoordToLand(pixel / ScreenSize(), true);
	return land.has_value() ? std::optional(land->position) : std::nullopt;
}

Projection sampling::CurrentProjection()
{
	Projection projection;
	if (!Locator::camera::has_value())
	{
		return projection;
	}
	const auto& camera = Locator::camera::value();
	projection.screenToLand = [](glm::vec2 pixel) { return ScreenToLand(pixel); };
	projection.rayDirection = [](glm::vec2 pixel) {
		glm::vec3 origin;
		glm::vec3 direction;
		Ray(pixel, origin, direction);
		return glm::normalize(direction);
	};
	projection.cameraPosition = camera.GetOrigin();
	// the camera's yaw: (cos, -sin) is the camera's right on the ground (inferred)
	const auto forward = camera.GetForward();
	const glm::vec2 right(-forward.z, forward.x);
	projection.yawAxis = glm::length(right) > 1e-5f ? glm::normalize(right) : glm::vec2(1.0f, 0.0f);
	return projection;
}

bool sampling::CameraMoving()
{
	return Sampling().cameraMoving;
}

void sampling::PlayStroke(std::vector<glm::ivec2> pixels)
{
	Sampling().stroke = std::move(pixels);
	Sampling().strokeNext = 0;
	Sampling().mouseMs = 0.0f;
}

bool sampling::PlayingStroke()
{
	return Sampling().strokeNext < Sampling().stroke.size();
}

void sampling::Update(float realSeconds)
{
	if (Locator::camera::has_value())
	{
		const auto position = Locator::camera::value().GetOrigin();
		Sampling().cameraMoving = Sampling().lastCamera.has_value() && *Sampling().lastCamera != position;
		Sampling().lastCamera = position;
	}
	if (PlayingStroke())
	{
		Sampling().mouseMs += realSeconds * 1000.0f;
		while (Sampling().mouseMs > k_MessageMs && PlayingStroke())
		{
			Sampling().mouseMs -= k_MessageMs;
			FeedSample(Sampling().stroke[Sampling().strokeNext++]);
		}
		if (!PlayingStroke())
		{
			Sampling().lastMouse = Sampling().stroke.back();
		}
		return;
	}
	if (!Locator::inputState::has_value())
	{
		return;
	}
	const auto mouse = input::GameCursor();
	const bool moved = !Sampling().lastMouse.has_value() || *Sampling().lastMouse != mouse;
	Sampling().lastMouse = mouse;
	// (approximate: a frame in which the mouse moved stands for a mouse event, and its time for the event's int ms)
	if (!moved)
	{
		return; // no mouse event, no message
	}
	Sampling().mouseMs += realSeconds * 1000.0f; // += the event's ms
	if (Sampling().mouseMs > k_MessageMs)
	{
		Sampling().mouseMs = 0.0f; // reset, not subtracted
		FeedSample(mouse);
	}
}
