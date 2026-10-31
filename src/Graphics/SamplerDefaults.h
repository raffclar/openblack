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

#include "GraphicsHandle.h"
#include "ShaderSamplers.h"

namespace openblack::graphics
{

/// The white textures that keep every sampler a draw's program declares bound. The draws set their textures through
/// their programs, which note each stage set in the bound stages of their encoder; when a program submits, the samplers
/// it declares that are still unset get a white texture of their kind from here.
class SamplerDefaults
{
public:
	SamplerDefaults();
	~SamplerDefaults();
	SamplerDefaults(const SamplerDefaults&) = delete;
	SamplerDefaults& operator=(const SamplerDefaults&) = delete;

	[[nodiscard]] TextureHandle Texture(shader_samplers::DefaultTexture texture) const;

private:
	std::array<TextureHandle, 4> _textures;
};

} // namespace openblack::graphics
