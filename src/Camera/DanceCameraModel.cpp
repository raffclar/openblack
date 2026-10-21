/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "DanceCameraModel.h"

using namespace openblack;

DanceCameraModel::DanceCameraModel(entt::entity dance, glm::vec3 origin, glm::vec3 focus)
    : _dance(dance)
    , _origin(origin)
    , _focus(focus)
{
}

std::optional<CameraModel::CameraInterpolationUpdateInfo> DanceCameraModel::Update(std::chrono::microseconds /*dt*/,
                                                                                   const Camera& /*camera*/)
{
	return std::nullopt;
}
