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

#include <filesystem>
#include <memory>
#include <span>
#include <string>
#include <vector>

#include <glm/vec2.hpp>

#include "Graphics/Texture2D.h"

namespace openblack::gui
{

/// The texels of a texture of glyphs from a font's coverage: white, the coverage in alpha
[[nodiscard]] std::vector<uint8_t> GlyphTexels(std::span<const uint8_t> coverage);

/// A texture of glyphs from a font's coverage (one byte a pixel, rows of `size.x`): white, the coverage in alpha. With a
/// source, the graphics device keeps no copy of the texels, and the texture gives them again from the source after a
/// restore of the device.
[[nodiscard]] std::unique_ptr<graphics::Texture2D> MakeGlyphTexture(std::span<const uint8_t> coverage, glm::u16vec2 size,
                                                                    const std::string& name,
                                                                    graphics::Texture2D::TexelSource source = {});

/// A source of the glyph texels of a font that reads the font's metrics and bitmaps again. With `full`, it gives the
/// glyphs at full height.
[[nodiscard]] graphics::Texture2D::TexelSource FontGlyphSource(std::filesystem::path met, std::filesystem::path fnt, bool full);

} // namespace openblack::gui
