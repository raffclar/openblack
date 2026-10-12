/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ScriptTexts.h"

#include <cmath>

#include <algorithm>
#include <limits>

using namespace openblack::gui;

namespace
{
/// A float cut to a whole number as the game's conversion does, out of range giving the lowest int
int32_t Truncate(float value)
{
	if (!std::isfinite(value) || value >= 2147483648.0f || value < -2147483648.0f)
	{
		return std::numeric_limits<int32_t>::min();
	}
	return static_cast<int32_t>(value);
}
} // namespace

script_text::ScreenText script_text::Write(std::u16string_view words, glm::vec2 corner, glm::vec2 extent, float size,
                                           float fadeSeconds, glm::u8vec3 colour)
{
	ScreenText text {
	    .words = std::u16string(words.substr(0, std::min(words.size(), k_MaxCharacters))),
	    .corner = corner,
	    .extent = extent,
	    .size = size,
	    .colour = colour,
	    .alpha = 0.0f,
	    .target = 1.0f,
	    .rate = 0.0f,
	};
	// The words end at their first NUL, as the game copies them
	if (const auto end = text.words.find(u'\0'); end != std::u16string::npos)
	{
		text.words.resize(end);
	}
	if (fadeSeconds == 0.0f)
	{
		text.alpha = 1.0f;
	}
	else
	{
		text.rate = 1.0f / fadeSeconds;
	}
	return text;
}

void script_text::FadeOut(ScreenText& text, float seconds)
{
	text.target = 0.0f;
	if (seconds == 0.0f)
	{
		text.alpha = 0.0f;
		text.rate = 0.0f;
		return;
	}
	text.rate = 1.0f / seconds;
}

bool script_text::Step(ScreenText& text, int32_t milliseconds)
{
	const auto seconds = static_cast<float>(milliseconds) * 0.001f;
	// Up towards the target, stopping at it; then down towards it, stopping at it
	if (text.alpha < text.target)
	{
		text.alpha += seconds * text.rate;
		if (text.alpha > text.target)
		{
			text.alpha = text.target;
		}
	}
	if (text.alpha > text.target)
	{
		text.alpha -= seconds * text.rate;
		if (text.alpha < text.target)
		{
			text.alpha = text.target;
		}
	}
	return text.alpha > k_GoneAlpha;
}

uint8_t script_text::AlphaByte(float alpha)
{
	return static_cast<uint8_t>(std::clamp(Truncate(alpha * 255.0f), 0, 255));
}

glm::u8vec3 script_text::ColourFromScript(float red, float green, float blue)
{
	const auto byte = [](float value) { return static_cast<uint8_t>(static_cast<uint32_t>(Truncate(value)) & 0xFFu); };
	return {byte(red), byte(green), byte(blue)};
}

script_text::ScreenBox script_text::Place(const ScreenText& text, glm::vec2 screen)
{
	const auto min = text.corner * screen;
	return {.min = min, .max = min + (text.extent * screen), .size = text.size * screen.y};
}

std::vector<script_text::DrawnLine> script_text::Lay(const ScreenBox& box, std::span<const std::u16string_view> lines,
                                                     const WidthFn& width)
{
	std::vector<DrawnLine> drawn;
	const auto boxWidth = box.max.x - box.min.x;
	if (boxWidth < box.size * 0.5f || box.max.y - box.min.y < box.size)
	{
		return drawn;
	}
	auto top = box.min.y;
	for (const auto& line : lines)
	{
		drawn.push_back({
		    .words = line,
		    .at = {((boxWidth - width(line)) * 0.5f) + box.min.x, top},
		    .topShare = box.max.y < top ? 0.0f : 1.0f,
		    .bottomShare = box.max.y < top + box.size ? 0.0f : 1.0f,
		});
		top += box.size;
		if (box.max.y < top)
		{
			break;
		}
	}
	return drawn;
}
