/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <chrono>

#include <glm/vec2.hpp>

#include "ECS/WindRules.h"

namespace openblack::ecs::systems
{

/// EXPERIMENT (not for merge): the one wind that the clouds, the fog, the waves, the spray, the foam and the plants
/// read (see wind). It follows the weather at the camera.
class WindSystemInterface
{
public:
	virtual ~WindSystemInterface() = default;

	/// Once a frame, with the frame's real time: the wind eases towards the weather at the camera
	virtual void Update(std::chrono::milliseconds frameRealTime) = 0;
	/// The wind as it is now
	[[nodiscard]] virtual const wind::Wind& GetWind() const = 0;
	/// The seconds that the wind has blown, which its gusts move with
	[[nodiscard]] virtual float GetSeconds() const = 0;
	/// The wind's velocity at a point across the ground now, with its gust, in world units a second
	[[nodiscard]] virtual glm::vec2 VelocityAt(glm::vec2 point) const = 0;
};

} // namespace openblack::ecs::systems
