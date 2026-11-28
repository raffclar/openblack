/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "LandRepaintRules.h"

namespace openblack::ecs::land_repaint
{

Filing FilingOf(const components::MapCellResident& resident)
{
	// The game keeps a thing that moves, and the plain things that stay put, in one cell, with no paint
	if (resident.placement == map_cells::Placement::MobileFront || resident.placement == map_cells::Placement::FixedBack)
	{
		return Filing::Plain;
	}
	return resident.coversOutline ? Filing::Outline : Filing::OneCell;
}

} // namespace openblack::ecs::land_repaint
