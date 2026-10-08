/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include "ECS/Systems/LandPickSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "Locator interface implementations should only be included in Locator.cpp, use interface instead."
#endif

namespace openblack::ecs::systems
{
/// The land's line tests on the current island (Locator::terrainSystem); nothing without one
class LandPickSystem final: public LandPickSystemInterface
{
public:
	[[nodiscard]] std::optional<glm::vec3> LandUnderPixel(glm::vec3 camera, glm::vec3 nearPoint, bool withSea) const override;
	[[nodiscard]] std::optional<glm::vec2> LandAlong(glm::vec3 from, glm::vec3 to) const override;
};
} // namespace openblack::ecs::systems
