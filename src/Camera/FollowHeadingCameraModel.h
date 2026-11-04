/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <functional>
#include <optional>

#include "CameraModel.h"
#include "FollowHeading.h"

namespace openblack
{

/// The camera that follows a thing and turns with the way it faces (see follow_heading). Only a saved game of the
/// original game gives this camera. It ends when its thing is gone, and the world camera then takes over.
class FollowHeadingCameraModel final: public CameraModel
{
public:
	/// Finds the thing in a frame; none once it is gone
	using ThingLookup = std::function<std::optional<follow_heading::Thing>()>;

	/// The held heading is the one that all the cameras of this kind share (Camera::HeldFollowHeading). It must stay
	/// while the model lives.
	FollowHeadingCameraModel(ThingLookup thing, const follow_heading::Settings& settings, float& heldHeading, glm::vec3 origin,
	                         glm::vec3 focus);

	std::optional<CameraInterpolationUpdateInfo> Update(std::chrono::microseconds dt, const Camera& camera) override;
	/// The mouse and the keys do not move this camera
	void HandleActions(std::chrono::microseconds /*dt*/) override {}
	/// It keeps its thing in view, also when the camera is sent somewhere
	void SetFlight(glm::vec3 /*origin*/, glm::vec3 /*focus*/, float /*rise*/) override {}
	[[nodiscard]] glm::vec3 GetTargetOrigin() const override { return _origin; }
	[[nodiscard]] glm::vec3 GetTargetFocus() const override { return _focus; }
	[[nodiscard]] bool IsStillValid() const override;
	[[nodiscard]] const follow_heading::Settings& GetSettings() const { return _settings; }

private:
	ThingLookup _thing;
	follow_heading::Settings _settings;
	std::reference_wrapper<float> _heldHeading;
	glm::vec3 _origin;
	glm::vec3 _focus;
};

} // namespace openblack
