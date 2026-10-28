/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "LandReuse.h"

#include <algorithm>

#include <LNDFile.h>

namespace openblack::land_reuse
{

bool KeepsLand(const Lands& lands)
{
	if (!lands.worldState || !lands.landLoaded || lands.savedLand.empty())
	{
		return false;
	}
	if (lands.openLand.generic_string() != lands.savedLand.generic_string())
	{
		return false;
	}
	// A land of a file can read another landscape: the landscape must be the same too
	return lands.builtByGame || lands.openLandscape.generic_string() == lands.savedLandscape.generic_string();
}

std::vector<uint16_t> ChangedCells(std::span<const lnd::LNDCell> cells, std::span<const uint8_t> built)
{
	std::vector<uint16_t> changed;
	const auto count = std::min(cells.size(), built.size());
	for (size_t i = 0; i < count; ++i)
	{
		if (cells[i].altitude != built[i])
		{
			changed.push_back(static_cast<uint16_t>(i));
		}
	}
	return changed;
}

} // namespace openblack::land_reuse
