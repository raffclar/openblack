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

#include <string>

#include <glm/vec3.hpp>

namespace openblack::ecs::components
{

/// The mark that floats up over a creature's head as it learns: a share of the way to knowing a skill or a miracle in
/// yellow, a blinking light bulb and a soft yellow glow. It grows, rises and fades, then goes.
struct LearningMarker
{
	/// The share, as the text that shows it ("50%")
	std::string text;
	/// The middle of its glow, which rises
	glm::vec3 position {0.0f};
	/// Its size as it starts, which it grows from to twice as big
	float size {1.0f};
	/// Milliseconds it has shown for
	float milliseconds {0.0f};
	/// The game turn that made it. It goes at the turn when its life is over, the same turn on every computer.
	uint32_t madeTurn {0};
};

} // namespace openblack::ecs::components
