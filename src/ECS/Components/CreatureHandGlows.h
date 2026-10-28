/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <array>
#include <optional>

#include <glm/mat4x4.hpp>

#include "Common/FramePace.h"
#include "Creature/CreatureHandGlow.h"

namespace openblack::ecs::components
{

/// The glows on a creature's body (see creature_hand_glow): their powers, and where the renderer draws each one now
struct CreatureHandGlows
{
	creature_hand_glow::Glows glows;
	/// A creature in the world: the textures slide, and the circling glow turns, at most once a frame of the screen's
	/// refresh
	frame_pace::Pace slidePace;
	frame_pace::Pace turnPace;

	/// A glow as the renderer draws it: the matrix of its model, how far its shell reaches out and how far its texture
	/// has slid
	struct Placed
	{
		glm::mat4 model {1.0f};
		float zoom {0.0f};
		glm::vec2 slide {0.0f};
	};
	/// Each glow where its creature's pose put it last, none for a place without a glow or before the first pose
	std::array<std::optional<Placed>, creature_hand_glow::k_PlaceCount> placed;
	/// How far each glow has turned and slid in the frames. Only the frames and the draw read it. The frames start it
	/// again while a place has no glow.
	creature_hand_glow::Motions motion {};
};

} // namespace openblack::ecs::components
