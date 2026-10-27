/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <limits>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "GraphicsHandle.h"
#include "ShaderSamplers.h"
#include "UniformTable.h"

namespace openblack::graphics
{
class DrawWindow;
struct DrawEncoder;
class SamplerDefaults;
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
	/// The samplers are those the program's shaders declare, which it keeps bound on every submit with the defaults.
	/// The main encoder is the one of the game's thread: the calls that name no encoder use it. The window, when there
	/// is one, can hold back the draws of the main encoder.
	ShaderProgram(const std::string& name, ShaderHandle vertexShader, ShaderHandle fragmentShader,
	              std::vector<shader_samplers::Sampler> samplers, const SamplerDefaults& samplerDefaults,
	              DrawEncoder& mainEncoder, DrawWindow* window = nullptr);
	~ShaderProgram();
	ShaderProgram(const ShaderProgram&) = delete;
	ShaderProgram& operator=(const ShaderProgram&) = delete;

	void SetTextureSampler(const char* samplerName, uint8_t bindPoint, const Texture2D& texture) const
	{
		SetTextureSampler(_main, samplerName, bindPoint, texture);
	}
	void SetTextureSampler(const char* samplerName, uint8_t bindPoint, const graphics::TextureHandle& texture) const
	{
		SetTextureSampler(_main, samplerName, bindPoint, texture);
	}
	void SetUniformValue(const char* uniformName, const void* value) const { SetUniformValue(_main, uniformName, value); }
	/// The first count elements of an array uniform
	void SetUniformArray(const char* uniformName, const void* values, uint16_t count) const
	{
		SetUniformArray(_main, uniformName, values, count);
	}
	[[nodiscard]] bool HasUniform(const char* uniformName) const { return _uniforms.Contains(uniformName); }
	/// A uniform's handle, for the draws that set it again and again without looking it up by name each time
	[[nodiscard]] std::optional<UniformHandle> FindUniform(std::string_view uniformName) const
	{
		return _uniforms.Find(uniformName);
	}
	/// The same as by name, with a handle of this program's from FindUniform
	void SetTextureSampler(UniformHandle sampler, uint8_t bindPoint, const Texture2D& texture) const
	{
		SetTextureSampler(_main, sampler, bindPoint, texture);
	}
	void SetTextureSampler(UniformHandle sampler, uint8_t bindPoint, const graphics::TextureHandle& texture) const
	{
		SetTextureSampler(_main, sampler, bindPoint, texture);
	}
	/// The same, with sampler flags that replace the texture's own, such as how it wraps
	void SetTextureSampler(UniformHandle sampler, uint8_t bindPoint, const Texture2D& texture, uint32_t samplerFlags) const
	{
		SetTextureSampler(_main, sampler, bindPoint, texture, samplerFlags);
	}
	void SetUniformValue(UniformHandle uniform, const void* value) const { SetUniformValue(_main, uniform, value); }

	/// Draws with this program in the given view. Each sampler it declares that no texture was set for since the
	/// bindings were last discarded gets a white texture of its kind first, so that no sampler is left unbound. If the
	/// draw window holds back the draw, the draw discards the same state and does not go to the GPU.
	void Submit(uint16_t viewId, uint32_t depth = 0, uint8_t discardFlags = k_DiscardAll) const
	{
		Submit(_main, viewId, depth, discardFlags);
	}

	/// The same calls into the encoder given, which can be the encoder of another thread
	void SetTextureSampler(DrawEncoder& to, const char* samplerName, uint8_t bindPoint, const Texture2D& texture) const;
	void SetTextureSampler(DrawEncoder& to, const char* samplerName, uint8_t bindPoint,
	                       const graphics::TextureHandle& texture) const;
	void SetUniformValue(DrawEncoder& to, const char* uniformName, const void* value) const;
	void SetUniformArray(DrawEncoder& to, const char* uniformName, const void* values, uint16_t count) const;
	void SetTextureSampler(DrawEncoder& to, UniformHandle sampler, uint8_t bindPoint, const Texture2D& texture) const;
	void SetTextureSampler(DrawEncoder& to, UniformHandle sampler, uint8_t bindPoint,
	                       const graphics::TextureHandle& texture) const;
	void SetTextureSampler(DrawEncoder& to, UniformHandle sampler, uint8_t bindPoint, const Texture2D& texture,
	                       uint32_t samplerFlags) const;
	void SetUniformValue(DrawEncoder& to, UniformHandle uniform, const void* value) const;
	/// The draw window holds back only the draws of the main encoder
	void Submit(DrawEncoder& to, uint16_t viewId, uint32_t depth = 0, uint8_t discardFlags = k_DiscardAll) const;

	[[nodiscard]] ProgramHandle GetRawHandle() const { return _program; }

	/// Gives the programs that have the same shaders but declare fewer bone matrices. A null program means this
	/// program.
	void SetBoneVariants(const ShaderProgram* singleBone, const ShaderProgram* fewBones) noexcept;
	/// Gives the program for a mesh with the given number of bone matrices. Of the programs with the same shaders, it
	/// is the one that declares the fewest bones that hold them. Every draw uploads all the bones that its vertex
	/// shader declares, and Vulkan keeps the uniforms of all the draws of a frame in one buffer of fixed size.
	[[nodiscard]] const ShaderProgram& ForBones(std::size_t bones) const noexcept;

	/// bgfx's BGFX_DISCARD_ALL, everything set for a draw dropped after it
	static constexpr uint8_t k_DiscardAll = 0xFF;

private:
	void WarnMissing(std::string_view name) const;
	void Bind(DrawEncoder& to, uint8_t bindPoint, UniformHandle sampler, TextureHandle texture,
	          uint32_t samplerFlags = std::numeric_limits<uint32_t>::max()) const;

	std::string _name;
	ProgramHandle _program;
	UniformTable _uniforms;
	std::vector<shader_samplers::Sampler> _samplers;
	/// The uniform of each of the samplers, in the same order
	std::vector<UniformHandle> _samplerUniforms;
	const SamplerDefaults& _samplerDefaults;
	DrawEncoder& _main;
	DrawWindow* _window;
	/// The programs for a mesh with one bone and with a few bones; null for this program
	const ShaderProgram* _singleBone {nullptr};
	const ShaderProgram* _fewBones {nullptr};
	/// The uniforms set on this shader that it doesn't have, already warned about
	mutable std::set<std::string, std::less<>> _warnedMissing;
	mutable std::mutex _warnedLock;
};

/// One of a program's uniforms looked up by name once, for the draws that set it again and again. Set without the
/// program having it, it is set by name, which warns of it once.
class ProgramUniform
{
public:
	ProgramUniform(const ShaderProgram& program, const char* name)
	    : _program(program)
	    , _name(name)
	    , _handle(program.FindUniform(name))
	{
	}

	void Set(const void* value) const
	{
		if (_handle)
		{
			_program.SetUniformValue(*_handle, value);
		}
		else
		{
			_program.SetUniformValue(_name, value);
		}
	}
	template <typename Texture>
	void Set(uint8_t bindPoint, const Texture& texture) const
	{
		if (_handle)
		{
			_program.SetTextureSampler(*_handle, bindPoint, texture);
		}
		else
		{
			_program.SetTextureSampler(_name, bindPoint, texture);
		}
	}

private:
	const ShaderProgram& _program;
	const char* _name;
	std::optional<UniformHandle> _handle;
};

} // namespace openblack::graphics
