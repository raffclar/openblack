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

#include "Visual/LandDetail.h"

namespace openblack
{
class Camera;
}

namespace openblack::ecs::systems
{

/// How the land's blocks are drawn in a frame: their mesh, morph, texture and coarser sides. The draw list's test of the
/// triangles that a block shows and the land's draw use the same classification of the frame, as the game does.
class LandDetailSystemInterface
{
public:
	virtual ~LandDetailSystemInterface() = default;

	/// The detail of the land's blocks seen from a camera, with the detail settings of now, in the order of the land's
	/// blocks. A second call in a frame with the same camera gives the same detail and does not classify again.
	[[nodiscard]] virtual const graphics::land_detail::FrameDetail& For(const Camera& camera) = 0;
	/// How many times the system classified the blocks
	[[nodiscard]] virtual uint64_t Classifications() const = 0;
};

} // namespace openblack::ecs::systems
