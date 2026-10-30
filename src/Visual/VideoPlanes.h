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

namespace openblack::graphics
{

/// One decoded picture of a video as its planes: Y full size and U, V half size each way, rows `stride` bytes apart.
/// Empty spans draw black.
struct VideoPlanes
{
	std::span<const uint8_t> y;
	std::span<const uint8_t> u;
	std::span<const uint8_t> v;
	uint32_t yStride {0};
	uint32_t chromaStride {0};
	uint32_t width {0};
	uint32_t height {0};
	/// Goes up with each new picture
	uint32_t serial {0};
};

} // namespace openblack::graphics
