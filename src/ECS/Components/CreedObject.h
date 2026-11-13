/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include "Common/FramePace.h"
#include "Creature/CreatureHandGlow.h"

namespace openblack::ecs::components
{

/// A loose object that the game makes as a creed, as the eyes (see creed_objects). The game never draws its own model:
/// it shows only a glow, which is the glow of a creature's creed power over the creed model, with a power of 1. The
/// cursor still finds the object by its own model.
struct CreedObject
{
	/// The glow is this many times the object's size. A script that sets the object's scale sets this value, and the
	/// object keeps its own size.
	float glowScale {1.0f};
	/// The texture of the glow slides at most once a frame of the screen's refresh
	frame_pace::Pace slidePace;
	/// How far the texture of the glow has slid. Only the frames and the draw read it.
	creature_hand_glow::Motion motion {};
};

} // namespace openblack::ecs::components
