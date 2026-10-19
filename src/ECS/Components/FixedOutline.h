/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include "ECS/FixedOutlines.h"

namespace openblack::ecs::components
{

/// The outline on the ground of a thing that stays put, made when the thing goes into the map's cells and kept until it
/// comes out again. Only things that the game gives an outline have one.
struct FixedOutline
{
	fixed_outlines::Outline outline;
};

} // namespace openblack::ecs::components
