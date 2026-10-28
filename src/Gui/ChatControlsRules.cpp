/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ChatControlsRules.h"

#include <cmath>

#include <algorithm>

#include "ScreenFit.h"

namespace openblack::gui::chat_controls
{

namespace
{
/// The cells of the pictures leave out half a texel at each side, of a 256 texel texture
constexpr float k_Inset = 1.0f / 512.0f;
constexpr float k_Shrink = 1.0f / 256.0f;
/// The speech bubbles are 48 texels square
constexpr float k_SpeechSize = 0.1875f;
} // namespace

DialogRect PlayerListRect(int width, int height, int contentHeight) noexcept
{
	const int most = height / 2;
	const int content = contentHeight + k_ListMargin;
	const int listHeight = content > k_LeastListHeight ? std::min(content, most) : k_LeastListHeight;
	const glm::ivec2 min {width - k_PlayerListWidth, k_ButtonSize};
	return {.min = min, .max = min + glm::ivec2 {k_PlayerListWidth, listHeight}};
}

DialogRect QuickListRect(int width, int height) noexcept
{
	const int listWidth = width - k_QuickListGap <= k_QuickListMostWidth ? width - k_QuickListGap : k_QuickListMostWidth;
	const glm::ivec2 min {width - listWidth, k_ButtonSize};
	return {.min = min, .max = min + glm::ivec2 {listWidth, height / 2}};
}

glm::vec2 Origin(glm::u16vec2 resolution) noexcept
{
	const auto space = FrontEndSpace(resolution.x, resolution.y);
	return -space.offset;
}

glm::ivec2 LayoutSize(glm::u16vec2 resolution) noexcept
{
	const auto space = FrontEndSpace(resolution.x, resolution.y);
	return {static_cast<int>(std::floor(static_cast<float>(resolution.x) / space.scale)),
	        static_cast<int>(std::floor(static_cast<float>(resolution.y) / space.scale))};
}

Picture PictureOf(Look look, bool lit) noexcept
{
	glm::vec2 corner {0.0f};
	switch (look)
	{
	case Look::Speech:
		corner = {lit ? 0.5f : 0.125f, 0.25f};
		break;
	case Look::NoSpeech:
		corner = {lit ? 0.6875f : 0.3125f, 0.25f};
		break;
	case Look::ExclaimArrow:
		corner = {lit ? 0.6875f : 0.3125f, 0.4375f};
		break;
	case Look::RightArrow:
		return {};
	}
	const auto uvMin = corner + k_Inset;
	return {.uvMin = uvMin, .uvMax = uvMin + (k_SpeechSize - k_Shrink)};
}

bool ShowsNext(bool shownBefore, const ShowFacts& facts) noexcept
{
	bool shown = shownBefore;
	if (!facts.connected || (facts.singlePlayer && !facts.open))
	{
		shown = false;
	}
	if (!facts.inGame)
	{
		return shown;
	}
	if (facts.open || facts.incoming)
	{
		shown = true;
	}
	if (facts.creatureInteracting)
	{
		shown = false;
	}
	if (facts.music && !facts.creatureInteracting)
	{
		shown = true;
	}
	return shown;
}

uint32_t PlayerColour(uint32_t godColour) noexcept
{
	// Each of red, green and blue goes half of the way to white. The colour has no alpha.
	constexpr uint32_t k_White = k_HeadingColour;
	uint32_t colour = 0;
	for (uint32_t shift = 0; shift < 24; shift += 8)
	{
		const uint32_t from = (godColour >> shift) & 0xFFu;
		const uint32_t to = (k_White >> shift) & 0xFFu;
		colour |= (((from * 128u) + (to * 128u)) >> 8u) << shift;
	}
	return colour;
}

glm::vec4 ToColour(uint32_t bits) noexcept
{
	return {static_cast<float>((bits >> 16u) & 0xFFu) / 255.0f, static_cast<float>((bits >> 8u) & 0xFFu) / 255.0f,
	        static_cast<float>(bits & 0xFFu) / 255.0f, 1.0f};
}

} // namespace openblack::gui::chat_controls
