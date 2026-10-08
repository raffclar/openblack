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

#include <functional>
#include <map>
#include <string>

#include "GraphicsHandle.h"

namespace openblack::graphics
{
class Texture2D;

class ShaderProgram
{
public:
	enum class Type
	{
		Vertex,
		Fragment,
		Compute,
	};

	ShaderProgram() = delete;
	ShaderProgram(const std::string& name, ShaderHandle vertexShader, ShaderHandle fragmentShader);
	ShaderProgram(const ShaderProgram&) = delete;
	ShaderProgram& operator=(const ShaderProgram&) = delete;
	~ShaderProgram();

	/// flags: bgfx sampler flags, UINT32_MAX = the texture's own
	void SetTextureSampler(const char* samplerName, uint8_t bindPoint, const Texture2D& texture,
	                       uint32_t flags = UINT32_MAX) const;
	void SetTextureSampler(const char* samplerName, uint8_t bindPoint, const graphics::TextureHandle& texture) const;
	/// num: array elements (1 for plain uniforms)
	void SetUniformValue(const char* uniformName, const void* value, uint16_t num = 1) const;

	[[nodiscard]] ProgramHandle GetRawHandle() const { return _program; }

private:
	std::string _name;
	ProgramHandle _program;
	// std::less<> finds a uniform by its name without building a string each time
	std::map<std::string, UniformHandle, std::less<>> _uniforms;
};

} // namespace openblack::graphics
