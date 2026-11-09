/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "LandIslandInterface.h"

#include <LNDFile.h>

using namespace openblack;

std::optional<uint8_t> LandIslandInterface::FindBuiltAltitude(const glm::u16vec2& coordinates) const
{
	const auto* cell = FindCell(coordinates);
	if (cell == nullptr)
	{
		return std::nullopt;
	}
	return cell->altitude;
}
