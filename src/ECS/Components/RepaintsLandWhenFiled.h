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

/// The model of the thing asks the land to paint the blocks under it again each time the thing goes into the map's
/// cells or comes out of them, also when the thing moves (see ecs::land_repaint). The beach temple puzzle puts this on
/// its largest ring when it starts. A ring that the puzzle makes again does not have it. A save does not keep it, as the
/// game keeps it on the model, which it makes again after a load.
struct RepaintsLandWhenFiled
{
	/// The count of the thing's filings when it got the flag. The filing at that time asked for no paint as it went in.
	uint32_t afterFiling {0};
};

} // namespace openblack::ecs::components
