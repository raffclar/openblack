/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "FightCameraModel.h"

#include "Locator.h"
#include "Windowing/WindowingInterface.h"

namespace openblack
{
namespace
{
/// The flight to the fight takes this long, as the player's camera's flights do
constexpr float k_FlightSeconds = 1.5f;
} // namespace

FightCameraModel::FightCameraModel(std::unique_ptr<CameraModel> playerModel, const fight_view::View& start)
    : _playerModel(std::move(playerModel))
    , _view(start)
{
}

void FightCameraModel::SetFighters(const fight_view::Fighter& first, const fight_view::Fighter& second)
{
	_first = first;
	_second = second;
}

std::optional<CameraModel::CameraInterpolationUpdateInfo> FightCameraModel::Update(std::chrono::microseconds dt,
                                                                                   [[maybe_unused]] const Camera& camera)
{
	const auto seconds = std::chrono::duration<float>(dt).count();
	// It flies to the fight first, and follows the fighters once there
	if (!_flightSent)
	{
		_flightSent = true;
		return CameraInterpolationUpdateInfo {
		    .origin = _view.origin,
		    .focus = _view.focus,
		    .duration = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::duration<float>(k_FlightSeconds)),
		};
	}
	if (_flownSeconds < k_FlightSeconds)
	{
		_flownSeconds += seconds;
		_input = {};
		return std::nullopt;
	}
	if (!_first.has_value() || !_second.has_value())
	{
		return std::nullopt;
	}
	auto input = _input;
	input.zoom = fight_view::ZoomDistance(_input.zoom, _view.origin.y - _view.focus.y);
	const auto step = _tracker.Follow(*_first, *_second, input, seconds);
	_input = {};
	_view = step.view;
	_zoomedOut = step.zoomedOut;
	// The fight's own easing moves it, so the camera goes straight there
	return CameraInterpolationUpdateInfo {.origin = _view.origin, .focus = _view.focus, .duration = dt};
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
