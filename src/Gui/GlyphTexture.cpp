/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "GlyphTexture.h"

#include <utility>

#include "Graphics/GraphicsDeviceInterface.h"
#include "Gui/GameFont.h"
#include "Locator.h"
#include "Resources/Rereads.h"

std::vector<uint8_t> openblack::gui::GlyphTexels(std::span<const uint8_t> coverage)
{
	std::vector<uint8_t> texels(coverage.size() * 4, 0xFF);
	for (size_t i = 0; i < coverage.size(); ++i)
	{
		texels[(i * 4) + 3] = coverage[i];
	}
	return texels;
}

std::unique_ptr<openblack::graphics::Texture2D> openblack::gui::MakeGlyphTexture(std::span<const uint8_t> coverage,
                                                                                 glm::u16vec2 size, const std::string& name,
                                                                                 graphics::Texture2D::TexelSource source)
{
	const auto texels = GlyphTexels(coverage);
	auto texture = std::make_unique<graphics::Texture2D>(name);
	texture->Create(size.x, size.y, 1, graphics::TextureFormat::RGBA8, graphics::Wrapping::ClampEdge, graphics::Filter::Linear,
	                Locator::graphicsDevice::value().Copy(texels.data(), static_cast<uint32_t>(texels.size())));
	texture->SetTexelSource(std::move(source));
	return texture;
}

openblack::graphics::Texture2D::TexelSource openblack::gui::FontGlyphSource(std::filesystem::path met,
                                                                            std::filesystem::path fnt, bool full)
{
	return [met = std::move(met), fnt = std::move(fnt), full](resources::Rereads& rereads) {
		const auto metrics = rereads.File(met);
		const auto bitmaps = rereads.File(fnt);
		const auto font = GameFont::Load(metrics, bitmaps);
		if (!font)
		{
			return std::vector<uint8_t> {};
		}
		return GlyphTexels(full ? font->GetFullAtlas() : font->GetAtlas());
	};
}
