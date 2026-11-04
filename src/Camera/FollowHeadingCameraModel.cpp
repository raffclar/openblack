/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "FollowHeadingCameraModel.h"

#include <utility>

#include "Camera.h"

namespace openblack
{

FollowHeadingCameraModel::FollowHeadingCameraModel(ThingLookup thing, const follow_heading::Settings& settings,
                                                   float& heldHeading, glm::vec3 origin, glm::vec3 focus)
    : _thing(std::move(thing))
    , _settings(settings)
    , _heldHeading(heldHeading)
    , _origin(origin)
    , _focus(focus)
{
}

std::optional<CameraModel::CameraInterpolationUpdateInfo>
FollowHeadingCameraModel::Update([[maybe_unused]] std::chrono::microseconds dt, const Camera& camera)
{
	const auto thing = _thing();
	if (!thing.has_value())
	{
		return std::nullopt;
	}
	const auto view = follow_heading::Step(_settings, *thing, camera.GetModelSeconds(), _heldHeading.get());
	_origin = view.origin;
	_focus = view.focus;
	return CameraInterpolationUpdateInfo {
	    .origin = view.origin,
	    .focus = view.focus,
	    .duration = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::duration<float>(view.seconds)),
	};
}

bool FollowHeadingCameraModel::IsStillValid() const
{
	return _thing().has_value();
}

} // namespace openblack
