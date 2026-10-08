/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Bubble.h"

#include <cmath>
#include <cstdint>

#include <algorithm>
#include <array>
#include <bit>

using namespace openblack::help;

BubbleSizes openblack::help::DefaultBubbleSizes(float width, float height, float textSize)
{
	BubbleSizes sizes {
	    .width = width,
	    .height = height,
	    .margin = width * 0.5f * 0.2f,
	    .textSize = textSize,
	    .farWidth = width * 0.5f * (1.0f / 7.0f),
	    .farHeight = height * 0.5f * (1.0f / 7.0f),
	    .farMargin = height * 0.5f * (1.0f / 35.0f),
	    .farTextSize = textSize * (1.0f / 7.0f),
	};
	return sizes;
}

float openblack::help::GetFrac(float v, float a, float b)
{
	// (pending) the a == b case is not read: b == a gives 1 here
	if (v <= a)
	{
		return 0.0f;
	}
	if (v >= b)
	{
		return 1.0f;
	}
	return (v - a) / (b - a);
}

std::optional<BubbleLayout> openblack::help::LayoutBubble(const BubbleSizes& sizes, float depth, glm::vec2 screen, float offset,
                                                          float place)
{
	if (depth < 1.0f)
	{
		return std::nullopt;
	}
	const float f = GetFrac(depth, sizes.nearDepth, sizes.farDepth);
	const auto lerp = [f](float a, float b) { return a + (b - a) * f; };
	BubbleLayout layout {
	    .width = lerp(sizes.width, sizes.farWidth),
	    .height = lerp(sizes.height, sizes.farHeight),
	    .margin = lerp(sizes.margin, sizes.farMargin),
	    .textSize = lerp(sizes.textSize, sizes.farTextSize),
	};
	// the distance fade, in below the fade-in end and out above the fade-out start, both bounded by the middle of the
	// range
	const float middle = (sizes.nearDepth + sizes.farDepth) * 0.5f;
	const float inEnd = std::min(sizes.fadeInEnd, middle);
	const float outStart = std::max(sizes.fadeOutStart, middle);
	layout.fade = 1.0f;
	if (depth < inEnd)
	{
		layout.fade *= GetFrac(depth, sizes.nearDepth, inEnd);
	}
	if (depth > outStart)
	{
		layout.fade *= 1.0f - GetFrac(depth, outStart, sizes.farDepth);
	}
	layout.depth = depth;
	layout.tailPlace = place;
	layout.topLeft = {screen.x + offset - place * layout.width, screen.y - layout.margin - layout.height};
	layout.tailRight = offset < -0.1f;
	return layout;
}

BubbleTail openblack::help::TailFor(float screenX, int screenWidth)
{
	float u = screenX / static_cast<float>(screenWidth) * 2.0f - 1.0f;
	float offset = 10.0f;
	if (u < -1.0f)
	{
		u = -1.0f;
	}
	else if (u > 1.0f)
	{
		u = 1.0f;
		offset = -10.0f;
	}
	else if (u > 0.0f)
	{
		offset = -10.0f;
	}
	// the doubles 0.6 and 0.2
	const auto place = static_cast<float>(std::fabs(static_cast<double>(u)) * 0.6000000238418579 + 0.20000000298023224);
	return {offset, place};
}

float openblack::help::DrawDepth(float depth, float pitch, float nearPlane)
{
	// p = clamp(|pitch|, 0, pi/2) x 2/pi; n = 2 x near; e = max(d - 5, n) - n; max(n + e (1 - p^2), n)
	constexpr double k_HalfPi = 1.5707963705062866;                                                         // pi / 2
	const double p = std::clamp(std::fabs(static_cast<double>(pitch)), 0.0, k_HalfPi) * 0.6366197546520227; // 2 / pi
	const float n = nearPlane + nearPlane;
	const float e = std::max(depth - 5.0f, n) - n;
	return std::max(n + static_cast<float>(static_cast<double>(e) - std::fabs(static_cast<double>(e)) * p * p), n);
}

std::vector<BubbleVertex> openblack::help::BubbleShape(float left, float top, float right, float bottom, float corner,
                                                       float tailX, bool tailRight, uint32_t argb, bool hasTail, uint8_t a0,
                                                       uint8_t a1)
{
	std::vector<BubbleVertex> out;
	const float w = right - left;
	const float h = bottom - top;
	if (w <= 0.0f || h <= 0.0f)
	{
		return out;
	}
	const float c = std::min({corner, w * 0.5f, h * 0.5f});
	const auto alpha = static_cast<float>(argb >> 24);
	const uint32_t rgb = argb & 0x00FFFFFFu;
	const auto colourAt = [&](float y) {
		// trunc((y - top) x slope + a0') with slope = alpha (a1 - a0) / (255 h), a0' = alpha a0 / 255
		const float slope = alpha * static_cast<float>(a1 - a0) / h * (1.0f / 255.0f);
		const float a = (y - top) * slope + alpha * static_cast<float>(a0) * (1.0f / 255.0f);
		const auto byte = static_cast<uint32_t>(std::clamp(static_cast<int>(a), 0, 255));
		return (byte << 24) | rgb;
	};
	// the grid: u {0, 0.5, 0.5, 1} x 0.25, v {0 or 0.3, 0.5, 0.5, 1} x 0.25
	const std::array<float, 4> xs = {left, left + c, right - c, right};
	const std::array<float, 4> ys = {top, top + c, bottom - c, bottom};
	const std::array<float, 4> us = {0.0f, 0.125f, 0.125f, 0.25f};
	const std::array<float, 4> vs = {hasTail ? 0.3f * 0.25f : 0.0f, 0.125f, 0.125f, 0.25f};
	const auto quad = [&out](const BubbleVertex& a, const BubbleVertex& b, const BubbleVertex& cc, const BubbleVertex& d) {
		out.insert(out.end(), {a, b, cc, a, cc, d});
	};
	for (size_t row = 0; row < 3; ++row)
	{
		for (size_t col = 0; col < 3; ++col)
		{
			quad({xs[col], ys[row], us[col], vs[row], colourAt(ys[row])},
			     {xs[col + 1], ys[row], us[col + 1], vs[row], colourAt(ys[row])},
			     {xs[col + 1], ys[row + 1], us[col + 1], vs[row + 1], colourAt(ys[row + 1])},
			     {xs[col], ys[row + 1], us[col], vs[row + 1], colourAt(ys[row + 1])});
		}
	}
	if (hasTail)
	{
		// the tail, as wide as the corner, kept 1.8 corners from the ends, 0.85 / 0.15 of it left of the point, from 0.091
		// corners above the bottom down; cell (1, 0), mirrored for the right side; the bottom's colour
		float x = std::max(tailX, left + 1.8f * c);
		x = std::min(x, right - 1.8f * c);
		const float x0 = x - (tailRight ? 0.85f * c : 0.15f * c);
		const float x1 = x0 + c;
		const float y0 = bottom - 0.091f * c;
		const float y1 = y0 + c;
		const float uLeft = tailRight ? 0.5f : 0.25f;
		const float uRight = tailRight ? 0.25f : 0.5f;
		const uint32_t colour = colourAt(bottom);
		quad({x0, y0, uLeft, 0.0f, colour}, {x1, y0, uRight, 0.0f, colour}, {x1, y1, uRight, 0.25f, colour},
		     {x0, y1, uLeft, 0.25f, colour});
	}
	return out;
}

std::vector<std::u16string> openblack::help::WrapGatheringText(std::u16string_view text, float maxWidth,
                                                               const std::function<float(std::u16string_view)>& widthOf)
{
	constexpr char16_t k_Space = 0xF8FE; // the script text's joined space
	const auto isBreakLine = [](char16_t c) { return c == u'\r' || c == u'\n'; };
	const auto isSpace = [](char16_t c) { return c == u' ' || c == u'\t' || c == u'\r' || c == u'\n' || c == 0x3000; };
	std::vector<std::u16string> lines;
	size_t start = 0;
	while (start < text.size())
	{
		// grow by up to 4 characters while it fits
		size_t count = 0;
		float width = 0.0f;
		while (start + count < text.size())
		{
			size_t step = 0;
			while (step < 4 && start + count + step < text.size())
			{
				const char16_t c = text[start + count + step];
				++step;
				if (isBreakLine(c))
				{
					break;
				}
			}
			width = widthOf(text.substr(start, count + step));
			if (width > maxWidth)
			{
				break;
			}
			count += step;
		}
		size_t next = start + count; // where the next line starts
		if (start + count < text.size() && width > maxWidth)
		{
			// back off from the last character measured to a break, whitespace / U+F8FE (dropped) or after a '-' (kept), until
			// the line fits; with no break, one character at a time
			size_t end = start + count + 4; // the overflowing measure took up to 4 more
			end = std::min(end, text.size());
			bool found = false;
			while (end > start + 1)
			{
				size_t i = end - 1;
				while (i > start && !isSpace(text[i]) && text[i] != k_Space && text[i] != u'-')
				{
					--i;
				}
				if (i == start)
				{
					break;
				}
				const size_t length = text[i] == u'-' ? i - start + 1 : i - start;
				if (widthOf(text.substr(start, length)) <= maxWidth)
				{
					count = length;
					next = i + 1;
					found = true;
					break;
				}
				end = i;
			}
			if (!found)
			{
				size_t length = std::max<size_t>(count, 1);
				while (length > 1 && widthOf(text.substr(start, length)) > maxWidth)
				{
					--length;
				}
				count = length;
				next = start + count;
			}
		}
		// a CR / LF inside the line ends it there
		for (size_t i = start; i < start + count; ++i)
		{
			if (isBreakLine(text[i]))
			{
				count = i - start;
				next = i + 1;
				break;
			}
		}
		lines.emplace_back(text.substr(start, count));
		if (next <= start)
		{
			next = start + 1; // (openblack guard) never loop on a character wider than the line
		}
		start = next;
	}
	return lines;
}

const std::vector<std::u16string>& GatheringWrapCache::Wrap(std::u16string_view text, float maxWidth, float size,
                                                            const std::function<float(std::u16string_view)>& widthOf)
{
	// the widths by their bits (a NaN is never kept)
	const auto same = [](float a, float b) {
		return std::bit_cast<uint32_t>(a) == std::bit_cast<uint32_t>(b) && !std::isnan(a);
	};
	if (!_kept || text != _text || !same(maxWidth, _maxWidth) || !same(size, _size))
	{
		_lines = WrapGatheringText(text, maxWidth, widthOf);
		_text = text;
		_maxWidth = maxWidth;
		_size = size;
		_kept = true;
	}
	return _lines;
}

Bubble::Bubble()
    : _sizes(DefaultBubbleSizes(400.0f, 250.0f, 19.0f))
{
}

void Bubble::Restart()
{
	_scroll = 1000.0f; // the last line
	_textHeight = 0.0f;
}

void Bubble::Age(float frameMs)
{
	_life -= frameMs * 0.0002f;
	if (_life < 0.0f)
	{
		_life = 0.0f;
	}
}

Bubble::ScrollMarks Bubble::ClampScroll(float boxHeight, float margin, float lineHeight)
{
	const float textHeight = _textHeight;
	float last = 0.0f;
	if (textHeight != 0.0f)
	{
		last = textHeight - (boxHeight - (margin + margin));
		if (lineHeight != 0.0f)
		{
			last /= lineHeight;
		}
		if (last < _scroll)
		{
			_scroll = last;
		}
	}
	if (_scroll < 0.0f)
	{
		_scroll = 0.0f;
	}
	return {.below = last - 1.0f > _scroll, .above = _scroll > 1.0f};
}

void Bubble::Drag(bool dragging, int mouseY, float lineHeight)
{
	if (!dragging)
	{
		_dragging = false;
		return;
	}
	if (lineHeight != 0.0f)
	{
		if (!_dragging)
		{
			_dragY = mouseY;
		}
		const int dy = mouseY - _dragY;
		_dragY = mouseY;
		_scroll += static_cast<float>(dy) / lineHeight;
	}
	_dragging = true;
}
