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

#include <array>
#include <optional>

#include <bgfx/bgfx.h>
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "GraphicsHandle.h"

namespace openblack::graphics
{

/// EXPERIMENT (not for merge): what the renderer keeps for the volumetric fog: the grid of the light that each cell
/// scatters (two, this frame's and the last frame's, which it blends with), the grid of the fog summed from the eye,
/// and the fog's state over time. Each grid lies in one texture, its slices in tiles. It releases them when it goes.
class VolumetricFogView
{
public:
	VolumetricFogView() = default;
	~VolumetricFogView();
	VolumetricFogView(const VolumetricFogView&) = delete;
	VolumetricFogView& operator=(const VolumetricFogView&) = delete;

	/// The grid of the light scattered that this frame writes, and the last frame's. Each call to Flip swaps them.
	[[nodiscard]] bgfx::FrameBufferHandle ScatterTarget();
	[[nodiscard]] TextureHandle Scatter() const;
	[[nodiscard]] TextureHandle PreviousScatter() const;
	void Flip() { _current = static_cast<uint8_t>(1 - _current); }
	/// The grid of the fog summed from the eye: rgb the light that it adds, a how much of the scene shows through
	[[nodiscard]] bgfx::FrameBufferHandle SumTarget();
	[[nodiscard]] TextureHandle Sum() const;

	/// The size of each grid's texture
	[[nodiscard]] static glm::u16vec2 Size();

	/// How wet the land is after rain
	float wetness {0.0f};
	/// How far the wind has moved the mist
	glm::vec2 drift {0.0f};
	/// The seconds of the last frame, the view-projection and the eye of the last frame
	std::optional<float> lastSeconds;
	std::optional<glm::mat4> previousViewProj;
	glm::vec3 previousEye {0.0f};
	uint32_t frame {0};

private:
	void Make();

	std::array<bgfx::TextureHandle, 2> _scatter {{BGFX_INVALID_HANDLE, BGFX_INVALID_HANDLE}};
	std::array<bgfx::FrameBufferHandle, 2> _scatterTargets {{BGFX_INVALID_HANDLE, BGFX_INVALID_HANDLE}};
	bgfx::TextureHandle _sum = BGFX_INVALID_HANDLE;
	bgfx::FrameBufferHandle _sumTarget = BGFX_INVALID_HANDLE;
	uint8_t _current {0};
};

} // namespace openblack::graphics
