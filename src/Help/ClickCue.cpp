/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ClickCue.h"

#include <cmath>

#include <algorithm>

namespace openblack::help::click_cue
{

namespace
{
/// The cue's right edge is this far in from the screen's
constexpr int k_RightInset = 4;
/// The gap between the word and the mouse
constexpr float k_Gap = 2.0f;
/// The glows' strength at full fade, the word's a third of the mouse's
constexpr int k_GlowAlpha = 0x80;
/// The mice picture is 4 by 4
constexpr float k_Cell = 0.25f;
/// The blink counts tenths of a second in fives, lit for the first two
constexpr uint32_t k_BlinkStep = 100;
constexpr uint32_t k_BlinkSteps = 5;
constexpr uint32_t k_BlinkLit = 2;

/// The cue's size for a box between two rows: a third of its height
int CueSize(int top, int bottom)
{
	return (bottom - top + 1) / 3;
}
} // namespace

bool ButtonLit(uint32_t tickMs)
{
	return (tickMs / k_BlinkStep) % k_BlinkSteps < k_BlinkLit;
}

int LabelSize(int top, int bottom, bool biggerText)
{
	const int size = CueSize(top, bottom);
	return biggerText ? (4 * size) / 5 : (2 * size) / 3;
}

std::optional<Cue> Layout(int screenWidth, int top, int bottom, float share, float labelWidth, bool biggerText, uint32_t tickMs)
{
	const float fade = std::clamp(share, 0.0f, 1.0f);
	const int alpha = static_cast<int>(fade * 255.0f);
	if (alpha < 4)
	{
		return std::nullopt;
	}
	const int size = CueSize(top, bottom);
	// Given by its middle row, the bottom third's
	const int middle = bottom - (size / 2) + 1;
	const int y = middle - (size / 2);
	const int labelSize = LabelSize(top, bottom, biggerText);
	const int glowAlpha = static_cast<int>(static_cast<float>(k_GlowAlpha) * fade);

	// Right-aligned, the word on the left
	const float fullWidth = labelWidth + static_cast<float>(size) + k_Gap;
	const int x = static_cast<int>(static_cast<float>(screenWidth - k_RightInset) - fullWidth);
	const int mouseX = static_cast<int>(static_cast<float>(x) + labelWidth + k_Gap);
	const int labelX = x;
	const float pad = static_cast<float>(size - labelSize) * 0.5f;

	const auto sizeF = static_cast<float>(size);
	const auto yF = static_cast<float>(y);
	const float alphaF = static_cast<float>(alpha) / 255.0f;
	Cue cue;
	cue.mouse = {{static_cast<float>(mouseX), yF}, {static_cast<float>(mouseX) + sizeF, yF + sizeF}};
	// The left button: the picture with the right one lit, mirrored
	const float column = ButtonLit(tickMs) ? 1.0f : 0.0f;
	const float row = static_cast<float>(k_WheelMouseRow);
	cue.uvMin = {(column + 1.0f) * k_Cell, row * k_Cell};
	cue.uvMax = {column * k_Cell, (row + 1.0f) * k_Cell};
	cue.mouseColour = {1.0f, 1.0f, 1.0f, alphaF};
	cue.mouseGlow = cue.mouse;
	cue.mouseGlowColour = {1.0f, 1.0f, 1.0f, static_cast<float>(glowAlpha) / 255.0f};
	cue.labelGlow = {{static_cast<float>(labelX), std::trunc(yF + pad)},
	                 {std::trunc(static_cast<float>(labelX) + labelWidth), std::trunc(yF + sizeF - pad)}};
	cue.labelGlowColour = {1.0f, 1.0f, 0.0f, static_cast<float>(glowAlpha / 3) / 255.0f};
	cue.labelAt = {static_cast<float>(labelX), yF + pad};
	cue.labelSize = static_cast<float>(labelSize);
	cue.labelColour = {1.0f, 1.0f, 0.0f, alphaF};
	cue.shadowColour = {0.0f, 0.0f, 0.0f, alphaF};
	return cue;
}

} // namespace openblack::help::click_cue
