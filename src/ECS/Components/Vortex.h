/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <vector>

#include <glm/vec3.hpp>

#include "Enums.h"

namespace openblack::ecs::components
{

/// A swirling vortex between the lands: the way out of a land, the way into the next, or the volcano's glowing mouth
struct Vortex
{
	VortexType type {VortexType::In};
	VortexStateType state {VortexStateType::FadeIn};
	/// The game turn its state began on
	uint32_t stateStartTurn {0};
	/// Its middle: where it was made, on the land
	glm::vec3 centre {0.0f};
	/// How far it has levelled the ground so far; below 0 before it first does
	float levelApplied {-1.0f};
	/// The levelled square's heights as they were when it first levelled them, x major, and their average
	std::vector<uint8_t> groundHeights;
	float groundAverage {0.0f};
};

} // namespace openblack::ecs::components
