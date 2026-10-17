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

#include <bgfx/bgfx.h>
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>

#include "Graphics/SunShadows.h"
#include "GraphicsHandle.h"

namespace openblack::graphics
{

/// EXPERIMENT (not for merge): what the renderer keeps for the shadows of the sun and the moon: the maps of the sun's
/// visibility over the land (an old one, a new one and their blend), the maps of the environment's shadows in two sets
/// of two cascades, the map of the moving things' shadows, and the target that darkens the scene's colour. It releases
/// them when it goes.
class SunShadowView
{
public:
	SunShadowView() = default;
	~SunShadowView();
	SunShadowView(const SunShadowView&) = delete;
	SunShadowView& operator=(const SunShadowView&) = delete;

	/// The target of a map of the sun's visibility: 0 and 1 are the two sets, 2 their blend. Made when first asked for.
	[[nodiscard]] bgfx::FrameBufferHandle VisibilityTarget(uint8_t index);
	[[nodiscard]] TextureHandle Visibility(uint8_t index) const;
	/// The target of a map of the environment's shadows, of a set (0 or 1) and a cascade
	[[nodiscard]] bgfx::FrameBufferHandle EnvironmentTarget(uint8_t set, uint8_t cascade);
	[[nodiscard]] TextureHandle Environment(uint8_t set, uint8_t cascade) const;
	/// The target of the map of the moving things' shadows
	[[nodiscard]] bgfx::FrameBufferHandle MovingTarget();
	[[nodiscard]] TextureHandle Moving() const;
	/// A target of the scene's colour alone, with no depth, so that a pass can read the scene's depth as it draws.
	/// It is made again when the scene's target changes, which it knows by the texture and the size.
	[[nodiscard]] bgfx::FrameBufferHandle SceneColour(TextureHandle colour, glm::u16vec2 size);

	/// When the maps of the environment are made
	[[nodiscard]] sun_shadows::ShadowClock& Clock() { return _clock; }
	/// The view-projection that each set's cascades were made with, and the moving things' map's
	std::array<std::array<glm::mat4, 2>, 2> environmentMatrices {};
	glm::mat4 movingMatrix {1.0f};
	/// Whether each set of maps has been made once
	std::array<bool, 2> environmentReady {false, false};

private:
	std::array<bgfx::TextureHandle, 3> _visibility {{BGFX_INVALID_HANDLE, BGFX_INVALID_HANDLE, BGFX_INVALID_HANDLE}};
	std::array<bgfx::FrameBufferHandle, 3> _visibilityTargets {{BGFX_INVALID_HANDLE, BGFX_INVALID_HANDLE, BGFX_INVALID_HANDLE}};
	std::array<bgfx::TextureHandle, 4> _environment {
	    {BGFX_INVALID_HANDLE, BGFX_INVALID_HANDLE, BGFX_INVALID_HANDLE, BGFX_INVALID_HANDLE}};
	std::array<bgfx::FrameBufferHandle, 4> _environmentTargets {
	    {BGFX_INVALID_HANDLE, BGFX_INVALID_HANDLE, BGFX_INVALID_HANDLE, BGFX_INVALID_HANDLE}};
	bgfx::TextureHandle _moving = BGFX_INVALID_HANDLE;
	bgfx::FrameBufferHandle _movingTarget = BGFX_INVALID_HANDLE;
	bgfx::FrameBufferHandle _sceneColour = BGFX_INVALID_HANDLE;
	uint16_t _sceneColourTexture {UINT16_MAX};
	glm::u16vec2 _sceneColourSize {0, 0};
	sun_shadows::ShadowClock _clock;
};

} // namespace openblack::graphics
