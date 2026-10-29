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

#include <memory>

#include "RendererInterface.h"

namespace openblack::graphics
{

/// The number of objects that the graphics library keeps now: the textures, the buffers, the frame buffers, the shaders,
/// the programs, the uniforms and the occlusion queries
[[nodiscard]] uint32_t GraphicsObjectCount();

/// The renderer of a world that nobody sees, such as the dedicated server. It has no window, no device, no shaders and
/// no buffers or textures, and it draws nothing.
///
/// The pictures that the world asks for never come and never fail, as with a device that has no screen. The graphics
/// library starts with no device only to keep the calls of other parts safe: the server makes no object with it. The
/// library makes some objects of its own as it starts, for its debug text and its transient buffers.
class HeadlessRenderer final: public RendererInterface
{
	/// Only Create makes a renderer, after the library started
	struct Started
	{
		explicit Started() = default;
	};

public:
	/// Starts the graphics library with no device on this thread. Gives none when the library does not start.
	[[nodiscard]] static std::unique_ptr<HeadlessRenderer> Create() noexcept;

	explicit HeadlessRenderer(Started started) noexcept;
	HeadlessRenderer(const HeadlessRenderer&) = delete;
	HeadlessRenderer& operator=(const HeadlessRenderer&) = delete;
	HeadlessRenderer(HeadlessRenderer&&) = delete;
	HeadlessRenderer& operator=(HeadlessRenderer&&) = delete;
	~HeadlessRenderer() noexcept override;

	void ConfigureView(RenderPass /*viewId*/, glm::u16vec2 /*resolution*/, uint32_t /*clearColor*/) const noexcept override {}
	void Reset(glm::u16vec2 /*resolution*/) const noexcept override {}
	void DrawScene(const DrawSceneDesc& /*drawDesc*/) const noexcept override {}
	void Frame() noexcept override;
	void BakeLandShadows(bool /*drawVegetation*/) const noexcept override {}
	void PrepareCreatureSkins(size_t /*count*/) const noexcept override {}
	[[nodiscard]] bool WarmUpScene(const DrawSceneDesc& /*drawDesc*/, uint32_t /*part*/) const noexcept override
	{
		return false;
	}
	void ReleaseGpuWorkMemory() noexcept override {}
	void RequestScreenshot(const std::filesystem::path& /*filepath*/) noexcept override {}
	[[nodiscard]] std::optional<std::string> TakeScreenshotFailure(const std::filesystem::path& /*filepath*/) noexcept override
	{
		return std::nullopt;
	}
	void RequestScreenPicture(const std::string& /*name*/, ScreenPicturer /*picturer*/) noexcept override {}
	[[nodiscard]] std::optional<std::vector<uint8_t>> TakeScreenPicture(const std::string& /*name*/) noexcept override
	{
		return std::nullopt;
	}
	[[nodiscard]] uint32_t GetPipelinesMade() const noexcept override { return 0; }
	[[nodiscard]] bool GetDebug() const noexcept override { return _debug; }
	void SetDebug(bool value) noexcept override { _debug = value; }
	[[nodiscard]] bool GetProfile() const noexcept override { return _profile; }
	void SetProfile(bool value) noexcept override { _profile = value; }
	void DrawMesh(const L3DMesh& /*mesh*/, const L3DMeshSubmitDesc& /*desc*/, uint8_t /*subMeshIndex*/) const noexcept override
	{
	}
	/// A shader manager with no shaders
	[[nodiscard]] ShaderManager& GetShaderManager() const noexcept override { return *_shaderManager; }
	void ShowInterfaceCreatures(interface_creatures::Request /*request*/) noexcept override {}
	void ShowMenuCreature(interface_creatures::MenuCreature /*creature*/) noexcept override {}
	void DrawFilmCreature(const DrawSceneDesc& /*desc*/, const interface_creatures::FilmCreature& /*creature*/) const override
	{
	}

private:
	std::unique_ptr<ShaderManager> _shaderManager;
	bool _debug {false};
	bool _profile {false};
};

} // namespace openblack::graphics
