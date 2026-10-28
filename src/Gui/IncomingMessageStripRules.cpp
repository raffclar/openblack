/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "IncomingMessageStripRules.h"

#include <cmath>

#include "ScreenFit.h"

namespace openblack::gui::incoming_message_strip
{

namespace
{
/// The cells of the pictures leave out half a texel at each side, of a 256 texel texture
constexpr float k_Inset = 1.0f / 512.0f;
constexpr float k_Shrink = 1.0f / 256.0f;
/// The speech bubbles are 48 texels square, the envelopes 32 texels square
constexpr float k_SpeechSize = 0.1875f;
constexpr float k_EnvelopeSize = 0.125f;
/// The weather pictures are cells of a grid of 4 by 4 cells, from the second row on. Each one leaves out one texel at
/// its top and left side and three at its other sides.
constexpr float k_WeatherCell = 0.25f;
constexpr float k_WeatherBorder = 1.0f / 256.0f;
constexpr float k_WeatherSize = 0.2421875f;
constexpr int k_FirstWeatherCell = 4;

[[nodiscard]] Picture Cell(Sheet sheet, glm::vec2 corner, float size) noexcept
{
	const auto uvMin = corner + k_Inset;
	return {.sheet = sheet, .uvMin = uvMin, .uvMax = uvMin + (size - k_Shrink)};
}
} // namespace

glm::vec2 Origin(glm::u16vec2 resolution) noexcept
{
	const auto space = FrontEndSpace(resolution.x, resolution.y);
	const auto bottom = space.ToScreen({0.0f, static_cast<float>(k_LayoutBottom)}).y;
	return {-space.offset.x, static_cast<float>(resolution.y) - bottom};
}

int LayoutWidth(glm::u16vec2 resolution) noexcept
{
	const auto space = FrontEndSpace(resolution.x, resolution.y);
	return static_cast<int>(std::floor(static_cast<float>(resolution.x) / space.scale));
}

Picture PictureOf(incoming_messages::ButtonLook look, bool hovered) noexcept
{
	using incoming_messages::Look;
	const float speechU = hovered ? 0.5f : 0.125f;
	switch (look.look)
	{
	case Look::Speech:
		return Cell(Sheet::Buttons, {speechU, 0.25f}, k_SpeechSize);
	case Look::SpeechArrow:
		return Cell(Sheet::Buttons, {speechU, 0.4375f}, k_SpeechSize);
	case Look::EnvelopeArrow:
		return Cell(Sheet::Buttons, {0.875f, 0.375f}, k_EnvelopeSize);
	case Look::Envelope:
		return Cell(Sheet::Buttons, {0.875f, 0.5f}, k_EnvelopeSize);
	case Look::Weather:
		break;
	}
	const int cell = k_FirstWeatherCell + look.picture;
	const glm::vec2 corner {(static_cast<float>(cell & 3) * k_WeatherCell) + k_WeatherBorder,
	                        (static_cast<float>(cell >> 2) * k_WeatherCell) + k_WeatherBorder};
	return Cell(Sheet::Bubbles, corner, k_WeatherSize);
}

} // namespace openblack::gui::incoming_message_strip
