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

#include <memory>
#include <span>
#include <string>

#include <glm/vec2.hpp>

namespace openblack::graphics
{
class Texture2D;
}

namespace openblack::gui
{

/// A texture of glyphs from a font's coverage (one byte a pixel, rows of `size.x`): white, the coverage in alpha
[[nodiscard]] std::unique_ptr<graphics::Texture2D> MakeGlyphTexture(std::span<const uint8_t> coverage, glm::u16vec2 size,
                                                                    const std::string& name);

} // namespace openblack::gui
