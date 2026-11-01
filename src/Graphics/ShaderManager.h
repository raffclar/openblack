/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "DrawEncoder.h"
#include "DrawWindow.h"
#include "ShaderProgram.h"
#include "Visual/RenderPass.h"

namespace openblack
{

class Camera;

namespace graphics
{

class SamplerDefaults;

/// How a view orders its draws, as bgfx's view modes do
enum class ViewOrder : uint8_t
{
	/// By program, then by depth, ties in the order they came
	ByProgram,
	/// In the order they came
	Sequential,
	/// The nearest (smallest depth) first
	DepthAscending,
	/// The farthest (greatest depth) first
	DepthDescending,
};

class ShaderManager
{
public:
	ShaderManager();
	~ShaderManager();

	void LoadShaders();
	[[nodiscard]] const ShaderProgram* GetShader(std::string_view name) const;
	/// Calls `visit(program)` for each program loaded
	template <typename Visit>
	void ForEachProgram(const Visit& visit) const
	{
		for (const auto& [name, program] : _shaderPrograms)
		{
			visit(*program);
		}
	}

	/// Drops the textures bound for the next draws, which a run of draws that kept them leaves bound
	void DiscardBindings() const;
	/// Where the game's thread draws: bgfx's own encoder
	[[nodiscard]] DrawEncoder& GetMainEncoder() const { return *_mainEncoder; }
	/// bgfx dropped every binding with the frame
	void FrameEnded() const;
	/// Sets how a view orders its draws, through bgfx
	void SetViewOrder(RenderPass viewId, ViewOrder order) const;
	void SetViewOrder(uint16_t viewId, ViewOrder order) const;

	void SetCamera(RenderPass viewId, const Camera& camera);

	/// Under Vulkan, lets only the draws first to first + count - 1 into the views go to the GPU. Gives false if the
	/// renderer does not need the window, which then lets all draws go.
	bool OpenDrawWindow(uint32_t first, uint32_t count, std::span<const uint16_t> views) const;
	/// Stops the draw window. Gives the number of draws into its views while it was open, if it was open.
	[[nodiscard]] std::optional<uint32_t> CloseDrawWindow() const;
	[[nodiscard]] bool IsDrawWindowOpen() const { return _drawWindow && _drawWindow->IsOpen(); }

	/// Every program's name with the samplers of both its shaders, as their Vulkan builds give them
	[[nodiscard]] static std::vector<std::pair<std::string, std::vector<shader_samplers::Sampler>>> ProgramSamplers();

private:
	/// By name. A name is found with no copy of it into a string.
	using ShaderMap = std::map<std::string, const ShaderProgram*, std::less<>>;

	ShaderMap _shaderPrograms;
	/// The textures bound to the samplers draws leave unset, shared by every program
	std::unique_ptr<SamplerDefaults> _samplerDefaults;
	/// The game thread's encoder, which the programs draw with when they are given no other
	std::unique_ptr<DrawEncoder> _mainEncoder;
	/// Under Vulkan, the window that can divide a frame's draws between some frames
	std::unique_ptr<DrawWindow> _drawWindow;
};

} // namespace graphics
} // namespace openblack
