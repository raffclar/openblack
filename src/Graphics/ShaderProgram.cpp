/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ShaderProgram.h"

#include <mutex>
#include <string>
#include <unordered_set>

#include <spdlog/spdlog.h>

#include "FileSystem/FileSystemInterface.h"
#include "Graphics/GraphicsHandleBgfx.h"
#include "Texture2D.h"

namespace openblack::graphics
{
namespace
{
// A missing uniform is set every frame: warning each time filled the log with gigabytes. Once per shader and name.
bool FirstMissing(const std::string& shader, std::string_view name)
{
	static std::mutex mutex;
	static std::unordered_set<std::string> warned;
	const std::lock_guard lock(mutex);
	return warned.insert(shader + '/' + std::string(name)).second;
}
} // namespace

ShaderProgram::ShaderProgram(const std::string& name, ShaderHandle vertexShader, ShaderHandle fragmentShader)
    : _name(name)
    , _program(BGFX_INVALID_HANDLE)
{
	uint16_t numShaderUniforms = 0;
	bgfx::UniformInfo info = {};
	std::vector<bgfx::UniformHandle> uniforms;

	numShaderUniforms = bgfx::getShaderUniforms(toBgfx(vertexShader));
	uniforms.resize(numShaderUniforms);
	bgfx::getShaderUniforms(toBgfx(vertexShader), uniforms.data(), numShaderUniforms);
	for (uint16_t i = 0; i < numShaderUniforms; ++i)
	{
		bgfx::getUniformInfo(uniforms[i], info);
		_uniforms.emplace(std::string(info.name), fromBgfx(uniforms[i]));
	}

	numShaderUniforms = bgfx::getShaderUniforms(toBgfx(fragmentShader));
	uniforms.resize(numShaderUniforms);
	bgfx::getShaderUniforms(toBgfx(fragmentShader), uniforms.data(), numShaderUniforms);
	for (uint16_t i = 0; i < numShaderUniforms; ++i)
	{
		bgfx::getUniformInfo(uniforms[i], info);
		_uniforms.emplace(std::string(info.name), fromBgfx(uniforms[i]));
	}

	_program = fromBgfx(bgfx::createProgram(toBgfx(vertexShader), toBgfx(fragmentShader), true));
	bgfx::setName(toBgfx(vertexShader), (name + "_vs").c_str());
	bgfx::setName(toBgfx(fragmentShader), (name + "_fs").c_str());
	bgfx::frame();
}

ShaderProgram::~ShaderProgram()
{
	if (bgfx::isValid(toBgfx(_program)))
	{
		bgfx::destroy(toBgfx(_program));
	}
}

void ShaderProgram::SetTextureSampler(const char* samplerName, uint8_t bindPoint, const Texture2D& texture,
                                      uint32_t flags) const
{
	auto uniform = _uniforms.find(samplerName);
	if (uniform != _uniforms.cend())
	{
		bgfx::setTexture(bindPoint, toBgfx(uniform->second), toBgfx(texture.GetNativeHandle()), flags);
	}
	else
	{
		if (FirstMissing(_name, samplerName))
		{
			SPDLOG_LOGGER_WARN(spdlog::get("graphics"), "Could not find texture sampler {} in {} Shader (warned once)",
			                   samplerName, _name);
		}
	}
}

void ShaderProgram::SetTextureSampler(const char* samplerName, uint8_t bindPoint, const graphics::TextureHandle& texture) const
{
	auto uniform = _uniforms.find(samplerName);
	if (uniform != _uniforms.cend())
	{
		bgfx::setTexture(bindPoint, toBgfx(uniform->second), toBgfx(texture));
	}
	else
	{
		if (FirstMissing(_name, samplerName))
		{
			SPDLOG_LOGGER_WARN(spdlog::get("graphics"), "Could not find texture sampler {} in {} Shader (warned once)",
			                   samplerName, _name);
		}
	}
}

void ShaderProgram::SetUniformValue(const char* uniformName, const void* value, uint16_t num) const
{
	auto uniform = _uniforms.find(uniformName);
	if (uniform != _uniforms.cend())
	{
		bgfx::setUniform(toBgfx(uniform->second), value, num);
	}
	else
	{
		if (FirstMissing(_name, uniformName))
		{
			SPDLOG_LOGGER_WARN(spdlog::get("graphics"), "Could not find uniform {} in {} Shader (warned once)", uniformName,
			                   _name);
		}
	}
}

} // namespace openblack::graphics
