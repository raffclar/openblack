/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <vector>

#include <glm/vec2.hpp>

#include "3D/Rain.h"

namespace openblack
{
class LandIslandInterface;
}

/// The land blocks that the rain and the snow use: the blocks in view whose box middle is near the eye
namespace openblack::weather_blocks
{

/// The corners of the land blocks in view whose box middle is nearer than the reach to the eye, in the order of the
/// land's blocks. When the reflection shows, a box goes down to the water's level.
[[nodiscard]] std::vector<glm::vec2> InView(const LandIslandInterface& island, const rain::View& view, bool reflected);

} // namespace openblack::weather_blocks
