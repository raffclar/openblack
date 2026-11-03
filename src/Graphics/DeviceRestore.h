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

#include <bgfx/bgfx.h>

/// The recovery from a lost graphics device. The graphics library makes the device again with all the textures,
/// buffers, shaders and render targets that it has, and each one keeps its handle. Only the pictures that the GPU drew
/// into render targets are gone, thus the renderer draws again the pictures that it keeps from one frame to the next.
namespace openblack::graphics::device_restore
{

/// Whether the game continues after a fatal error of the graphics library. It continues only after a lost device,
/// because the library makes the device again. All other fatal errors stop the game.
[[nodiscard]] constexpr bool ContinuesAfter(bgfx::Fatal::Enum code) noexcept
{
	return code == bgfx::Fatal::DeviceLost;
}

/// Finds each new restore of the device one time, from the count of restores that the graphics library gives
class Watch
{
public:
	/// True one time for each change of the count
	[[nodiscard]] constexpr bool Saw(uint32_t count) noexcept
	{
		if (count == _seen)
		{
			return false;
		}
		_seen = count;
		return true;
	}

private:
	uint32_t _seen {0};
};

} // namespace openblack::graphics::device_restore
