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
#include <future>
#include <optional>
#include <vector>

#include <bgfx/bgfx.h>
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>

#include "Graphics/VolumetricClouds.h"
#include "GraphicsHandle.h"

namespace openblack::graphics
{

/// EXPERIMENT (not for merge): what the renderer keeps for the volumetric clouds: the noise textures, the targets of
/// the rays at half the screen's size, the history that the frames blend into, the map of the shadow, and the weather
/// as it eases over time. It releases its textures and targets when it goes.
class VolumetricCloudsView
{
public:
	/// Starts to make the noise on other threads
	VolumetricCloudsView();
	~VolumetricCloudsView();
	VolumetricCloudsView(const VolumetricCloudsView&) = delete;
	VolumetricCloudsView& operator=(const VolumetricCloudsView&) = delete;

	/// Whether the noise is made, and its textures are ready. The first frames have no clouds until then.
	[[nodiscard]] bool Ready();
	/// The noise textures, once they are ready
	[[nodiscard]] TextureHandle BaseNoise() const;
	[[nodiscard]] TextureHandle DetailNoise() const;
	[[nodiscard]] TextureHandle WeatherNoise() const;
	[[nodiscard]] TextureHandle BlueNoise() const;

	/// Makes the targets of the rays again when the screen changes size. Gives the size of the targets.
	glm::u16vec2 Fit(glm::u16vec2 screen);
	/// The target of the rays: the clouds' colour and where they are
	[[nodiscard]] bgfx::FrameBufferHandle MarchTarget() const { return _marchTarget; }
	[[nodiscard]] TextureHandle MarchColour() const;
	[[nodiscard]] TextureHandle MarchDepth() const;
	/// The history that this frame writes, and the one of the last frame that it reads. Each call to Flip swaps them.
	[[nodiscard]] bgfx::FrameBufferHandle HistoryTarget() const { return _historyTargets.at(_history); }
	[[nodiscard]] TextureHandle History() const;
	[[nodiscard]] TextureHandle PreviousHistory() const;
	void Flip() { _history = 1 - _history; }
	/// Whether the previous history holds the clouds of the last frame
	[[nodiscard]] bool HistoryValid() const { return _historyValid; }
	void SetHistoryValid(bool valid) { _historyValid = valid; }

	/// The map of the shadow, made the first time it is asked for
	[[nodiscard]] bgfx::FrameBufferHandle ShadowTarget();
	[[nodiscard]] TextureHandle Shadow() const;

	/// The weather of the clouds as it eases
	[[nodiscard]] volumetric_clouds::CloudClimate& Climate() { return _climate; }
	/// The seconds of the last frame, to find how much time a frame takes
	std::optional<float> lastSeconds;
	/// The view-projection of the last frame, which the history was drawn with
	std::optional<glm::mat4> previousViewProj;
	/// The frames drawn, for the jitter
	uint32_t frame {0};

private:
	void ReleaseTargets();

	/// The noise as the threads make it
	std::future<std::vector<uint8_t>> _baseTexels;
	std::future<std::vector<uint8_t>> _detailTexels;
	std::future<std::vector<uint8_t>> _weatherTexels;
	std::future<std::vector<uint8_t>> _blueTexels;

	bgfx::TextureHandle _base = BGFX_INVALID_HANDLE;
	bgfx::TextureHandle _detail = BGFX_INVALID_HANDLE;
	bgfx::TextureHandle _weather = BGFX_INVALID_HANDLE;
	bgfx::TextureHandle _blue = BGFX_INVALID_HANDLE;
	glm::u16vec2 _size {0, 0};
	std::array<bgfx::TextureHandle, 2> _march {{BGFX_INVALID_HANDLE, BGFX_INVALID_HANDLE}};
	bgfx::FrameBufferHandle _marchTarget = BGFX_INVALID_HANDLE;
	std::array<bgfx::TextureHandle, 2> _histories {{BGFX_INVALID_HANDLE, BGFX_INVALID_HANDLE}};
	std::array<bgfx::FrameBufferHandle, 2> _historyTargets {{BGFX_INVALID_HANDLE, BGFX_INVALID_HANDLE}};
	uint8_t _history {0};
	bool _historyValid {false};
	bgfx::TextureHandle _shadow = BGFX_INVALID_HANDLE;
	bgfx::FrameBufferHandle _shadowTarget = BGFX_INVALID_HANDLE;
	volumetric_clouds::CloudClimate _climate;
};

} // namespace openblack::graphics
