/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "GlyphTexture.h"

#include <bgfx/bgfx.h>

#include "Graphics/Texture2D.h"

std::unique_ptr<openblack::graphics::Texture2D> openblack::gui::MakeGlyphTexture(std::span<const uint8_t> coverage,
                                                                                 glm::u16vec2 size, const std::string& name)
{
	const auto* texels = bgfx::alloc(static_cast<uint32_t>(coverage.size() * 4));
	for (size_t i = 0; i < coverage.size(); ++i)
	{
		texels->data[i * 4] = 0xFF;
		texels->data[(i * 4) + 1] = 0xFF;
		texels->data[(i * 4) + 2] = 0xFF;
		texels->data[(i * 4) + 3] = coverage[i];
	}
	auto texture = std::make_unique<graphics::Texture2D>(name);
	texture->Create(size.x, size.y, 1, graphics::TextureFormat::RGBA8, graphics::Wrapping::ClampEdge, graphics::Filter::Linear,
	                texels);
	return texture;
}
