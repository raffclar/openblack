/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include "CameraModel.h"
#include "FreeCamera.h"

namespace openblack
{

/// The free camera (see free_camera). Only a saved game of the original game gives this camera. It does not end, and
/// the frame does not lift it above the land.
class FreeCameraModel final: public CameraModel
{
public:
	FreeCameraModel(glm::vec3 origin, glm::vec3 focus, const free_camera::State& state);

	std::optional<CameraInterpolationUpdateInfo> Update(std::chrono::microseconds dt, const Camera& camera) override;
	/// Reads the place of the mouse and whether the hand grips the land
	void HandleActions(std::chrono::microseconds dt) override;
	/// Reads the movement keys after the step of the camera. They move the camera in the next frame.
	void HandleKeys(std::chrono::microseconds dt) override;
	/// It stays where it is when the camera is sent somewhere
	void SetFlight(glm::vec3 /*origin*/, glm::vec3 /*focus*/, float /*rise*/) override {}
	[[nodiscard]] glm::vec3 GetTargetOrigin() const override { return _view.origin; }
	[[nodiscard]] glm::vec3 GetTargetFocus() const override { return _view.focus; }
	[[nodiscard]] bool DrawnAboveLand() const override { return false; }
	[[nodiscard]] const free_camera::State& GetState() const { return _state; }
	/// The hand grips the land in this frame
	void SetLandGrip(bool grips) { _input.gripsLand = grips; }

private:
	free_camera::View _view;
	free_camera::State _state;
	free_camera::Input _input;
};

} // namespace openblack
