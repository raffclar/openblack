/*******************************************************************************
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

namespace openblack::graphics::alpha_test
{

/// When the exact product of two alphas is the reference, the float product can be a very small amount lower. The test
/// takes this margin, on the scale from 0 to 255, off the reference.
constexpr float k_Margin = 1.0f / 128.0f;

/// The game's alpha test: a pixel shows only when its alpha from 0 to 1, times 255, is the reference or more. A
/// reference of 0 lets all pixels through, thus it is the same as no test. The interface shaders do the same test.
[[nodiscard]] constexpr bool Shows(float alpha, uint8_t reference) noexcept
{
	return alpha * 255.0f >= static_cast<float>(reference) - k_Margin;
}

/// The reference of the font pages. All text of the game, on the screen and in the world, draws with it. A pixel of a
/// glyph shows only when the alpha of the glyph times the alpha of the text is 5 of 255 or more.
constexpr uint8_t k_FontPageReference = 5;

/// The value of the uniform u_alphaRef of the interface shaders for a reference
[[nodiscard]] constexpr std::array<float, 4> Uniform(uint8_t reference) noexcept
{
	return {static_cast<float>(reference), 0.0f, 0.0f, 0.0f};
}

} // namespace openblack::graphics::alpha_test
