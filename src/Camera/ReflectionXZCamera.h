/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include "Camera.h"

namespace openblack
{
// The reflection in the flat plane of water at a height: the sea at 0, or a lake at its level
class ReflectionXZCamera final: public Camera
{
public:
	explicit ReflectionXZCamera(float level = 0.0f);
	[[nodiscard]] glm::mat4 GetViewMatrix(Interpolation interpolation) const final;

private:
	float _level;
};
} // namespace openblack
