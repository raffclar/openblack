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

#include "3D/LandIslandInterface.h"
#include "Camera.h"
#include "Locator.h"
#include "Windowing/WindowingInterface.h"

namespace openblack
{
namespace
{
/// The camera's curves move by at most this many seconds a step
constexpr float k_LongestStep = 0.1f;
} // namespace

FightCameraModel::FightCameraModel(std::unique_ptr<CameraModel> playerModel, const fight_view::View& start,
                                   const glm::vec3& way)
    : _playerModel(std::move(playerModel))
    , _view(start)
    , _way(way)
{
}

void FightCameraModel::Return(const fight_view::View& from, const fight_view::View& speed)
{
	_started = true;
	_tracker.Resnap();
	const auto to = _chase.GetHeld();
	const auto way = fight_view::WayPoint(from.origin, to.origin, fight_view::k_ReturnWayRise, [](glm::vec2 point) {
		return Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetHeightAt(point) : 0.0f;
	});
	_chase.Fly(from, speed, to, way);
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
		_chase.Fly({.origin = camera.GetOrigin(), .focus = camera.GetFocus()},
		           {.origin = camera.GetOriginVelocity(), .focus = camera.GetFocusVelocity()}, _view, _way);
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
	const auto shown = _chase.Step(wanted, camera.GetModelSeconds(), step);
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
