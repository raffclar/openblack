/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <map>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "RenderPass.h"
#include "ShaderProgram.h"

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
	[[nodiscard]] const ShaderProgram* GetShader(const std::string& name) const;

	/// Drops the textures bound for the next draws, which a run of draws that kept them leaves bound
	void DiscardBindings() const;
	/// bgfx dropped every binding with the frame
	void FrameEnded() const;
	/// Sets how a view orders its draws, through bgfx
	void SetViewOrder(RenderPass viewId, ViewOrder order) const;
	void SetViewOrder(uint16_t viewId, ViewOrder order) const;

	void SetCamera(RenderPass viewId, const Camera& camera);

	/// Every program's name with the samplers of both its shaders, as their Vulkan builds give them
	[[nodiscard]] static std::vector<std::pair<std::string, std::vector<shader_samplers::Sampler>>> ProgramSamplers();

private:
	using ShaderMap = std::map<std::string, const ShaderProgram*>;

	ShaderMap _shaderPrograms;
	/// The textures bound to the samplers draws leave unset, shared by every program
	std::unique_ptr<SamplerDefaults> _samplerDefaults;
};

} // namespace graphics
} // namespace openblack
