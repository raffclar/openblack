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

struct IndexBufferHandle
{
	uint32_t id = 0;
};

struct FrameBufferHandle
{
	uint32_t id = 0;
};

struct VertexBufferHandle
{
	uint32_t id = 0;
};

struct VertexLayoutHandle
{
	uint32_t id = 0;
};

struct DynamicVertexBufferHandle
{
	uint32_t id = 0;
};

struct DynamicIndexBufferHandle
{
	uint32_t id = 0;
};

struct TextureHandle
{
	uint32_t id = 0;
};

struct UniformHandle
{
	uint32_t id = 0;
};

struct ShaderHandle
{
	uint32_t id = 0;
};

struct ProgramHandle
{
	uint32_t id = 0;
};

/// The number of a handle that the device did not make, for example when it has no free handles
constexpr uint32_t k_InvalidHandleId = 0xFFFF;

/// A handle that the device did not make
template <typename Handle>
[[nodiscard]] constexpr Handle InvalidHandle() noexcept
{
	return Handle {k_InvalidHandleId};
}

/// Whether the device made the handle. The device keeps the number of a handle in 16 bits.
template <typename Handle>
[[nodiscard]] constexpr bool IsValidHandle(Handle handle) noexcept
{
	return static_cast<uint16_t>(handle.id) != k_InvalidHandleId;
}

} // namespace openblack::graphics
