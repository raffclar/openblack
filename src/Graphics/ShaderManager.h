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
#include <string>
#include <string_view>

#include "RenderPass.h"
#include "ShaderProgram.h"

namespace openblack
{

class Camera;

namespace graphics
{

class ShaderManager
{
public:
	ShaderManager() = default;
	~ShaderManager();

	void LoadShaders();
	[[nodiscard]] const ShaderProgram* GetShader(std::string_view name) const;

	void SetCamera(RenderPass viewId, const Camera& camera);

private:
	using ShaderMap = std::map<std::string, std::unique_ptr<const ShaderProgram>, std::less<>>;

	ShaderMap _shaderPrograms;
};

} // namespace graphics
} // namespace openblack
