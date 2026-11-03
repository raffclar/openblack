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

/// The requests for a restore of the graphics device that tests and tools send, and the data that the graphics library
/// keeps for a restore. The world and the debug tools use these types without the graphics library.
namespace openblack::graphics::device_restore
{

/// How a test asks for a restore of the device
enum class Request : uint8_t
{
	/// The graphics library makes the device again
	MakeAgain,
	/// The graphics library loses its device, finds the loss and then makes the device again. Direct3D 12 removes its
	/// device. Vulkan has no call for this, thus it acts as if a call gave a lost device.
	Lose,
};

/// The bytes of resource data that the graphics library keeps to make its device again, by kind
struct Kept
{
	uint64_t buffers {0};
	uint64_t dynamicBuffers {0};
	uint64_t shaders {0};
	/// The texels that the textures were made with, with the updates that go into them
	uint64_t textures {0};
	/// The updates of textures that do not go into their texels: textures made without texels, compressed textures
	/// and volume textures
	uint64_t textureChanges {0};

	[[nodiscard]] constexpr uint64_t Total() const noexcept
	{
		return buffers + dynamicBuffers + shaders + textures + textureChanges;
	}
};

} // namespace openblack::graphics::device_restore
