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

namespace openblack::ecs::components
{

/// A bank of mist: a mist object
/// (Data\Landscape\mist.l3d with the smoke material) at the land's height + the script's altitude, drawn
/// facing the camera like the sky clouds. Its Transform holds the position.
struct Mist
{
	float size;      ///< the mesh scale
	uint32_t colour; ///< ARGB; the alpha is colour >> 24
	/// set when the script's last value is not 1 (then k is that value): scale size / (1 + (k - 1)
	/// (1 - |dy| / |d|)) and the sky light (from above, ambient 210); without it, the plain size, the colour times the
	/// land light under it and the models' light
	bool edgeShrink;
	float k;     ///< 3 by default
	int counter; ///< += int(time_inc * 0.255) modulo 900; the atlas frame is frame_anim::MistCell
	float counterRemainder;
};

} // namespace openblack::ecs::components
