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
	if (resident.placement == map_cells::Placement::MobileFront)
	{
		return Filing::Mobile;
	}
	return resident.coversOutline ? Filing::Outline : Filing::OneCell;
}

} // namespace openblack::ecs::land_repaint
