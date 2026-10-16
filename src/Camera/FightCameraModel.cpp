/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "FightCameraModel.h"

#include <algorithm>
#include <chrono>

#include <glm/geometric.hpp>
#include <spdlog/spdlog.h>

#include "3D/LandIslandInterface.h"
#include "Camera.h"
#include "CameraZones.h"
#include "DefaultWorldCameraModel.h"
#include "ECS/Systems/CameraZoneSystemInterface.h"
#include "Locator.h"
#include "Windowing/WindowingInterface.h"

namespace openblack
{
namespace
{
/// The camera's curves move by at most this many seconds a step
constexpr float k_LongestStep = 0.1f;

/// A flight from where the camera is to a view: kept inside the fence, through a point on the way, with a woosh when it
/// starts far away
void Fly(fight_view::Chase& chase, const fight_view::View& from, const fight_view::View& speed, const fight_view::View& to,
         float rise)
{
	const auto ground = [](glm::vec2 point) {
		return Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetHeightAt(point) : 0.0f;
	};
	const auto keepInside = [](const glm::vec3& origin, const glm::vec3& focus) {
		if (!Locator::cameraZoneSystem::has_value())
		{
			return origin;
		}
		const auto& zones = Locator::cameraZoneSystem::value().GetZones();
		return camera_zones::FlightOriginInsideFence(zones.fence, zones.fenceOn, origin, focus);
	};
	const auto flight = fight_view::PlanFlight(from.origin, to, rise, ground, keepInside);
	chase.Fly(from, speed, flight.to, flight.way);
	SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Fight view flight of {:.1f}{}", glm::distance(from.origin, flight.to.origin),
	                    flight.woosh ? ", with a woosh" : "");
	if (flight.woosh)
	{
		DefaultWorldCameraModel::Woosh();
	}
}
} // namespace

FightCameraModel::FightCameraModel(std::unique_ptr<CameraModel> playerModel, const fight_view::View& start)
    : _playerModel(std::move(playerModel))
    , _view(start)
{
}

void FightCameraModel::Return(const fight_view::View& from, const fight_view::View& speed)
{
	_started = true;
	_tracker.Resnap();
	_landHeld = false;
	_sinceFlight = 0.0f;
	Fly(_chase, from, speed, _chase.GetHeld(), fight_view::k_ReturnWayRise);
}

void FightCameraModel::SetFighters(const fight_view::Fighter& first, const fight_view::Fighter& second)
{
	_first = first;
	_second = second;
}

std::optional<CameraModel::CameraInterpolationUpdateInfo> FightCameraModel::Update(std::chrono::microseconds dt,
                                                                                   const Camera& camera)
{
	const auto seconds = std::chrono::duration<float>(dt).count();
	// The fight's own easing uses the whole step; the camera's curves move by at most a tenth of a second a step
	const auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(dt).count();
	const auto step = std::min(static_cast<float>(milliseconds) * 0.001f, k_LongestStep);
	// It flies to the fight first from where the camera is, and follows the fighters once there
	if (!_started)
	{
		_started = true;
		_sinceFlight = 0.0f;
		Fly(_chase, {.origin = camera.GetOrigin(), .focus = camera.GetFocus()},
		    {.origin = camera.GetOriginVelocity(), .focus = camera.GetFocusVelocity()}, _view, fight_view::k_StartWayRise);
	}
	// Any camera control the player uses stops the flight
	if (_asked.controlsUsed)
	{
		_chase.StopFlight();
	}
	_sinceFlight += seconds;
	// While the player holds the land, the player's own camera moves it and the view does not follow the fighters. A drag
	// that turns or tilts the camera turns or tilts the view too.
	if (_asked.landHeld && _playerModel != nullptr)
	{
		_tracker.Dragged(_asked.dragTurn, _asked.dragTilt);
		_input = {};
		_landHeld = true;
		return _playerModel->Update(dt, camera);
	}
	// Let go, the view goes on from where the camera is and how fast it moves
	if (_landHeld)
	{
		_landHeld = false;
		_chase.Resume({.origin = camera.GetOrigin(), .focus = camera.GetFocus()},
		              {.origin = camera.GetOriginVelocity(), .focus = camera.GetFocusVelocity()});
	}
	std::optional<fight_view::View> wanted;
	if (_first.has_value() && _second.has_value())
	{
		auto input = _input;
		input.zoom = fight_view::ZoomDistance(_input.zoom, _view.origin.y - _view.focus.y);
		const auto followed = _tracker.Follow(*_first, *_second, input, seconds);
		_view = followed.view;
		_zoomedOut = followed.zoomedOut;
		wanted = _view;
	}
	_input = {};
	const bool flying = _chase.Flying();
	const auto shown = _chase.Step(wanted, camera.GetModelSeconds(), step);
	// The count since the flight starts again as the flight starts its last part
	if (flying && !_chase.Flying())
	{
		_sinceFlight = 0.0f;
	}
	// The camera goes straight to where its own curves put it
	return CameraInterpolationUpdateInfo {
	    .origin = shown.origin, .focus = shown.focus, .duration = std::chrono::microseconds::zero()};
}

void FightCameraModel::HandleActions(std::chrono::microseconds dt)
{
	// The player's camera reads the turning, tilting and zooming the player asks for
	if (_playerModel == nullptr)
	{
		return;
	}
	_playerModel->HandleActions(dt);
	const auto asked = _playerModel->GetAsked();
	_asked = asked;
	_input.turn += asked.turn;
	_input.tilt += asked.tilt;
	_input.zoom += asked.zoom;
	_input.screenWidth = Locator::windowing::has_value() ? static_cast<float>(Locator::windowing::value().GetSize().x) : 1.0f;
}

void FightCameraModel::SetFlight([[maybe_unused]] glm::vec3 origin, [[maybe_unused]] glm::vec3 focus)
{
	// Watching a fight, the camera stays on it
}

} // namespace openblack
