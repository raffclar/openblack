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

#include "Creature/CreatureGesture.h"

namespace openblack::ecs::components
{

/// A creature that draws a gesture in the air with its hand, or did so before. The counter of the drawing stays after
/// the drawing ends, as the next drawing reads it.
struct CreatureGestureDrawing
{
	enum class Stage : uint8_t
	{
		/// It draws nothing
		None,
		/// It waits for its body to be free, then it starts to point at the start of the shape
		Start,
		/// It moves the point along the shape
		Draw,
		/// The point ended: it waits for its arm to come down
		End,
	};
	enum class Outcome : uint8_t
	{
		Running,
		Done,
		Failed,
	};
	Stage stage {Stage::None};
	Outcome outcome {Outcome::Running};
	uint32_t gesture {0};
	/// The drawing is a move of a dance: when it is done, the move becomes a dance
	bool inDance {false};
	/// The points of the shape, evenly along its length, and the share of it that one turn draws
	std::vector<glm::vec3> shape;
	float sharePerTurn {0.0f};
	creature_gesture::Drawing drawing;
	/// The turn that the counter of the drawing last counted
	uint32_t countedTurn {0};
	/// The glowing chain behind the hand
	uint32_t chain {0};
};

} // namespace openblack::ecs::components
