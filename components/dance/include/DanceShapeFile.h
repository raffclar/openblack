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

#include <span>
#include <vector>

#include "DanceFile.h"

/// The shape that dancers stand in, from the .dan files of the Data folder (such as Arc.dan): little-endian, the count
/// of points, then the x and z of each point, 65535 to one radius
namespace openblack::dance
{

struct DanceShapePoint
{
	int32_t x {0};
	int32_t z {0};
};

struct DanceShapeFile
{
	std::vector<DanceShapePoint> points;

	/// Reads a shape from its bytes, which must all be read
	DanceResult Open(std::span<const uint8_t> buffer);
};

} // namespace openblack::dance
