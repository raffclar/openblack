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

#include "3D/FrameAnim.h"

namespace openblack::ecs::components
{

/// A mist inside the temple: the game's mist object, the dome of mist.l3d with the smoke's frames, turned to face
/// the camera at its Transform's position and drawn by the island's mist draw. It is a part of a room
/// (TempleInteriorPart), drawn while its room is.
struct MistDome
{
	/// Its frame of the smoke texture, which the room moves on while it is drawn
	graphics::frame_anim::MistClock clock;
	/// ARGB: white at half alpha
	uint32_t colour {0x80FFFFFF};
	/// The dome's scale
	float size {1.0f};
};

} // namespace openblack::ecs::components
