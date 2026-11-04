/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "SamplerDefaults.h"

#include <algorithm>
#include <optional>

#include <bgfx/bgfx.h>

#include "GraphicsHandleBgfx.h"

namespace openblack::graphics
{

namespace
{
using shader_samplers::DefaultTexture;

constexpr uint32_t k_White = 0xFFFFFFFF;

const bgfx::Memory* WhiteTexels(uint32_t count)
{
	const auto* memory = bgfx::alloc(count * static_cast<uint32_t>(sizeof(k_White)));
	auto* texels = reinterpret_cast<uint32_t*>(memory->data);
	std::fill_n(texels, count, k_White);
	return memory;
}
} // namespace

SamplerDefaults::SamplerDefaults()
{
	constexpr auto k_Format = bgfx::TextureFormat::RGBA8;
	constexpr auto k_Flags = BGFX_SAMPLER_NONE;
	auto& white2D = _textures[static_cast<size_t>(DefaultTexture::White2D)];
	white2D = fromBgfx(bgfx::createTexture2D(1, 1, false, 1, k_Format, k_Flags, WhiteTexels(1)));
	bgfx::setName(toBgfx(white2D), "Default White 2D");
	// Two layers, as a texture of one layer is an ordinary 2D texture, which Direct3D won't sample as an array
	auto& white2DArray = _textures[static_cast<size_t>(DefaultTexture::White2DArray)];
	white2DArray = fromBgfx(bgfx::createTexture2D(1, 1, false, 2, k_Format, k_Flags, WhiteTexels(2)));
	bgfx::setName(toBgfx(white2DArray), "Default White 2D Array");
	auto& whiteCube = _textures[static_cast<size_t>(DefaultTexture::WhiteCube)];
	whiteCube = fromBgfx(bgfx::createTextureCube(1, false, 1, k_Format, k_Flags, WhiteTexels(6)));
	bgfx::setName(toBgfx(whiteCube), "Default White Cube");
	auto& white3D = _textures[static_cast<size_t>(DefaultTexture::White3D)];
	white3D = fromBgfx(bgfx::createTexture3D(1, 1, 1, false, k_Format, k_Flags, WhiteTexels(1)));
	bgfx::setName(toBgfx(white3D), "Default White 3D");
	for (const auto& texture : _textures)
	{
		if (bgfx::isValid(toBgfx(texture)))
		{
			bgfx::setDeviceRestoreByApplication(toBgfx(texture));
		}
	}
}

void SamplerDefaults::GiveTexelsAgain() const
{
	const auto valid = [this](DefaultTexture texture) {
		const auto handle = toBgfx(_textures.at(static_cast<size_t>(texture)));
		return bgfx::isValid(handle) ? std::optional(handle) : std::nullopt;
	};
	if (const auto white2D = valid(DefaultTexture::White2D))
	{
		bgfx::updateTexture2D(*white2D, 0, 0, 0, 0, 1, 1, WhiteTexels(1));
	}
	if (const auto white2DArray = valid(DefaultTexture::White2DArray))
	{
		for (uint16_t layer = 0; layer < 2; ++layer)
		{
			bgfx::updateTexture2D(*white2DArray, layer, 0, 0, 0, 1, 1, WhiteTexels(1));
		}
	}
	if (const auto whiteCube = valid(DefaultTexture::WhiteCube))
	{
		constexpr uint8_t k_Sides = 6;
		for (uint8_t side = 0; side < k_Sides; ++side)
		{
			bgfx::updateTextureCube(*whiteCube, 0, side, 0, 0, 0, 1, 1, WhiteTexels(1));
		}
	}
	if (const auto white3D = valid(DefaultTexture::White3D))
	{
		bgfx::updateTexture3D(*white3D, 0, 0, 0, 0, 1, 1, 1, WhiteTexels(1));
	}
}

SamplerDefaults::~SamplerDefaults()
{
	for (const auto& texture : _textures)
	{
		if (bgfx::isValid(toBgfx(texture)))
		{
			bgfx::destroy(toBgfx(texture));
		}
	}
}

TextureHandle SamplerDefaults::Texture(DefaultTexture texture) const
{
	const auto handle = _textures.at(static_cast<size_t>(texture));
	// Without 3D textures there are no 3D samplers to bind, but the 2D texture stands in rather than nothing
	return bgfx::isValid(toBgfx(handle)) ? handle : _textures[static_cast<size_t>(DefaultTexture::White2D)];
}

} // namespace openblack::graphics
