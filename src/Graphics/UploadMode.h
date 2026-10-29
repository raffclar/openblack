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

namespace openblack::graphics
{

/// What a loaded resource makes from the data that it reads
enum class UploadMode : uint8_t
{
	/// The resource keeps its data and makes its buffers and textures on the graphics device
	Device,
	/// The resource keeps only its data and makes no object on the graphics device. A world with no device, such as the
	/// dedicated server, uses this mode.
	DataOnly,
};

} // namespace openblack::graphics
