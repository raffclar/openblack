/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "FreeCameraModel.h"

#include "Camera.h"
#include "Input/GameActionMapInterface.h"
#include "Locator.h"
#include "Windowing/WindowingInterface.h"

namespace openblack
{

namespace
{

int32_t MillisecondsOf(std::chrono::microseconds dt)
{
	return static_cast<int32_t>(std::chrono::duration_cast<std::chrono::milliseconds>(dt).count());
}

} // namespace

FreeCameraModel::FreeCameraModel(glm::vec3 origin, glm::vec3 focus, const free_camera::State& state)
    : _view {.origin = origin, .focus = focus}
    , _state(state)
{
}

std::optional<CameraModel::CameraInterpolationUpdateInfo> FreeCameraModel::Update(std::chrono::microseconds dt,
                                                                                  const Camera& camera)
{
	_input.milliseconds = MillisecondsOf(dt);
	_view = free_camera::Step({.origin = camera.GetOrigin(), .focus = camera.GetFocus()}, _state, _input);
	// The camera goes to its new place at once, with no glide
	return CameraInterpolationUpdateInfo {.origin = _view.origin, .focus = _view.focus, .duration = {}};
}

void FreeCameraModel::HandleActions([[maybe_unused]] std::chrono::microseconds dt)
{
	if (Locator::gameActionSystem::has_value())
	{
		_input.mouse = glm::ivec2(Locator::gameActionSystem::value().GetMousePosition());
	}
	if (Locator::windowing::has_value())
	{
		_input.screen = Locator::windowing::value().GetSize();
	}
}

void FreeCameraModel::HandleKeys(std::chrono::microseconds dt)
{
	using input::BindableActionMap;
	if (!Locator::gameActionSystem::has_value())
	{
		return;
	}
	const auto& actions = Locator::gameActionSystem::value();
	free_camera::ReadKeys(_state,
	                      {
	                          .left = actions.Get(BindableActionMap::MOVE_LEFT),
	                          .right = actions.Get(BindableActionMap::MOVE_RIGHT),
	                          .forwards = actions.Get(BindableActionMap::MOVE_FORWARDS),
	                          .backwards = actions.Get(BindableActionMap::MOVE_BACKWARDS),
	                          .shift = actions.Get(BindableActionMap::ROTATE_ON),
	                          .ctrl = actions.Get(BindableActionMap::ZOOM_ON),
	                      },
	                      MillisecondsOf(dt));
}

} // namespace openblack
