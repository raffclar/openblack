/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "InstanceLightPoint.h"

namespace openblack::graphics::instance_light
{

void SetLightOffset(glm::mat4& model, glm::vec2 offset) noexcept
{
	model[1][3] = offset.x;
	model[2][3] = offset.y;
}

glm::vec2 LightOffset(const glm::mat4& model) noexcept
{
	return {model[1][3], model[2][3]};
}

} // namespace openblack::graphics::instance_light
