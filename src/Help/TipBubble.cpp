/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "TipBubble.h"

#include <cmath>

#include <algorithm>

#include <glm/geometric.hpp>

namespace openblack::help::tip_bubble
{

namespace
{
/// A seventh, and a seventh of a fifth, as the game keeps them
constexpr float k_Seventh = 0.14285715f;
constexpr float k_FarMarginShare = 0.028571429f;
/// The tail is pushed this far to the sign's middle-of-the-screen side
constexpr float k_TailOffset = 10.0f;
/// The box reaches left of the tail by this much of its width at the middle of the screen, and the second share more
/// at either edge
constexpr float k_TailShareAtMiddle = 0.2f;
constexpr float k_TailShareToEdge = 0.6f;
/// The tail keeps this many radii from the box's ends, sits this share of a radius right of its point (or the other
/// share when flipped), and overlaps the box by this much of a radius
constexpr float k_TailInset = 1.8f;
constexpr float k_TailLean = 0.15f;
constexpr float k_TailLeanFlipped = 0.85f;
constexpr float k_TailOverlap = 0.091f;
/// The texture's quarters: the box's corners and edges are cut from the first, the tail is the second
constexpr float k_Quarter = 0.25f;
constexpr float k_Eighth = 0.125f;
/// The box's top row starts a little down its quarter when it has a tail
constexpr float k_TopRowV = 0.075f;
/// The arrows: a fifth of the box tall, at its right side, a ninth of that grown about each
constexpr float k_ArrowShare = 0.2f;
constexpr uint32_t k_BlinkPeriod = 750;
constexpr uint32_t k_BlinkLit = 400;
/// The words' shadow, its offset in pixels and its share of the words' alpha
constexpr float k_ShadowOffset = 2.0f;
constexpr float k_ShadowAlpha = 128.0f;

float Lerp(float a, float b, float t)
{
	return a + ((b - a) * t);
}

/// A screen point as the game turns it into a world point: cut to whole pixels
glm::vec2 Pixel(float x, float y)
{
	return {std::trunc(x), std::trunc(y)};
}
} // namespace

namespace
{
/// The blanks a line breaks at: white space and the blank that adds no space
bool IsBlank(char16_t c)
{
	constexpr char16_t k_Tilde = 0xF8FE;
	return c == u' ' || (c >= u'\t' && c <= u'\r') || c == k_Tilde;
}

bool IsLineBreak(char16_t c)
{
	return c == u'\r' || c == u'\n';
}

std::u16string_view TrimBlanksAtEnd(std::u16string_view text)
{
	while (!text.empty() && IsBlank(text.back()))
	{
		text.remove_suffix(1);
	}
	return text;
}
} // namespace

std::vector<std::u16string> Wrap(std::u16string_view text, float width, float size, const WidthFn& measure)
{
	constexpr size_t k_GrowBy = 4;
	std::vector<std::u16string> lines;
	const auto fits = [&](std::u16string_view line) { return measure(line, size) <= width; };
	while (!text.empty())
	{
		std::u16string_view line;
		size_t next = 0;
		if (fits(text))
		{
			line = text;
			next = text.size();
		}
		else
		{
			// Grown until too wide
			size_t grown = 0;
			while (grown < text.size())
			{
				grown = std::min(grown + k_GrowBy, text.size());
				if (!fits(text.substr(0, grown)))
				{
					break;
				}
			}
			// The rightmost blank or hyphen at which it fits, else as many characters as fit
			for (size_t at = grown - 1; at > 0 && next == 0; --at)
			{
				if (IsBlank(text[at]))
				{
					if (const auto before = TrimBlanksAtEnd(text.substr(0, at)); !before.empty() && fits(before))
					{
						line = before;
						next = at + 1;
					}
				}
				else if (text[at] == u'-' && at != grown - 1 && fits(text.substr(0, at + 1)))
				{
					line = text.substr(0, at + 1);
					next = at + 1;
				}
			}
			for (size_t count = grown - 1; count > 0 && next == 0; --count)
			{
				if (fits(text.substr(0, count)))
				{
					line = text.substr(0, count);
					next = count;
				}
			}
			if (next == 0)
			{
				break;
			}
		}
		// A line break ends the line there
		if (const auto found = std::ranges::find_if(line, IsLineBreak); found != line.end())
		{
			const auto at = static_cast<size_t>(found - line.begin());
			line = line.substr(0, at);
			next = at + 1;
		}
		lines.emplace_back(line);
		text.remove_prefix(next);
	}
	return lines;
}

Sizes FarSizes(const Sizes& near)
{
	const float halfWidth = near.width * 0.5f;
	return {
	    .width = halfWidth * k_Seventh,
	    .height = near.height * 0.5f * k_Seventh,
	    .margin = halfWidth * k_FarMarginShare,
	    .lineHeight = near.lineHeight * k_Seventh,
	};
}

float Fraction(float value, float a, float b)
{
	if (value <= a)
	{
		return 0.0f;
	}
	if (value >= b)
	{
		return 1.0f;
	}
	return (value - a) / (b - a);
}

Sizes SizesAt(float depth)
{
	const auto far = FarSizes(k_NearSizes);
	const float t = Fraction(depth, k_NearDepth, k_FarDepth);
	return {
	    .width = Lerp(k_NearSizes.width, far.width, t),
	    .height = Lerp(k_NearSizes.height, far.height, t),
	    .margin = Lerp(k_NearSizes.margin, far.margin, t),
	    .lineHeight = Lerp(k_NearSizes.lineHeight, far.lineHeight, t),
	};
}

float DistanceAlpha(float depth)
{
	float alpha = 1.0f;
	if (depth < k_FadeInEnd)
	{
		alpha *= Fraction(depth, k_NearDepth, k_FadeInEnd);
	}
	if (depth > k_FadeOutStart)
	{
		alpha *= 1.0f - Fraction(depth, k_FadeOutStart, k_FarDepth);
	}
	return alpha;
}

std::optional<Placement> Place(glm::vec2 anchor, float depth, float screenWidth)
{
	if (depth < k_TooClose)
	{
		return std::nullopt;
	}
	// How far across the screen the sign is, -1 at the left edge and 1 at the right
	const float across = std::clamp((anchor.x / screenWidth) * 2.0f - 1.0f, -1.0f, 1.0f);
	const float side = across <= 0.0f ? k_TailOffset : -k_TailOffset;
	const float share = (std::abs(across) * k_TailShareToEdge) + k_TailShareAtMiddle;
	Placement placement;
	placement.sizes = SizesAt(depth);
	placement.depth = depth;
	placement.distanceAlpha = DistanceAlpha(depth);
	const auto& sizes = placement.sizes;
	placement.min = {anchor.x + side - (share * sizes.width), anchor.y - sizes.margin - sizes.height};
	placement.max = placement.min + glm::vec2(sizes.width, sizes.height);
	placement.tailX = placement.min.x + (sizes.width * share);
	placement.tailFlipped = side < -0.1f;
	return placement;
}

std::vector<Quad> BoxShape(const Placement& placement, float alpha)
{
	const float x0 = placement.min.x;
	const float y0 = placement.min.y;
	const float x1 = placement.max.x;
	const float y1 = placement.max.y;
	const float width = x1 - x0;
	const float height = y1 - y0;
	const float radius = std::min({placement.sizes.margin, width * 0.5f, height * 0.5f});

	const std::array xs {x0, x0 + radius, x1 - radius, x1};
	const std::array us {0.0f, k_Eighth, k_Eighth, k_Quarter};
	const std::array ys {y0, y0 + radius, y1 - radius, y1};
	const std::array vs {k_TopRowV, k_Eighth, k_Eighth, k_Quarter};
	// See-through at the top, whole at the bottom
	const std::array alphas {0.0f, alpha * radius / height, alpha * (height - radius) / height, alpha};
	const auto corner = [&](size_t column, size_t row) {
		return Corner {
		    .position = Pixel(xs.at(column), ys.at(row)), .uv = {us.at(column), vs.at(row)}, .alpha = alphas.at(row)};
	};

	std::vector<Quad> quads;
	quads.reserve(10);
	for (size_t row = 0; row < 3; ++row)
	{
		for (size_t column = 0; column < 3; ++column)
		{
			quads.push_back(
			    {corner(column, row), corner(column + 1, row), corner(column + 1, row + 1), corner(column, row + 1)});
		}
	}

	// The tail, a square of the margin hanging under the box towards the sign
	const float point = std::min(std::max(placement.tailX, x0 + (k_TailInset * radius)), x1 - (k_TailInset * radius));
	const float left = point - (radius * (placement.tailFlipped ? k_TailLeanFlipped : k_TailLean));
	const float right = left + radius;
	const float top = y1 - (k_TailOverlap * radius);
	const float bottom = top + radius;
	const float uLeft = placement.tailFlipped ? 2.0f * k_Quarter : k_Quarter;
	const float uRight = placement.tailFlipped ? k_Quarter : 2.0f * k_Quarter;
	quads.push_back({
	    Corner {.position = Pixel(left, top), .uv = {uLeft, 0.0f}, .alpha = alpha},
	    Corner {.position = Pixel(right, top), .uv = {uRight, 0.0f}, .alpha = alpha},
	    Corner {.position = Pixel(right, bottom), .uv = {uRight, k_Quarter}, .alpha = alpha},
	    Corner {.position = Pixel(left, bottom), .uv = {uLeft, k_Quarter}, .alpha = alpha},
	});
	return quads;
}

bool ArrowsLit(uint32_t milliseconds)
{
	return milliseconds % k_BlinkPeriod < k_BlinkLit;
}

namespace
{
Arrow ArrowAt(const Placement& placement, float top, float bottom, glm::vec2 uvMin, glm::vec2 uvMax)
{
	const float size = (placement.max.y - placement.min.y) * k_ArrowShare;
	const float grow = size / 9.0f;
	return {
	    .min = {placement.max.x - (0.55f * size) - grow, top - grow},
	    .max = {placement.max.x - (0.05f * size) + grow, bottom + grow},
	    .uvMin = uvMin,
	    .uvMax = uvMax,
	};
}
} // namespace

Arrow UpArrow(const Placement& placement)
{
	const float size = (placement.max.y - placement.min.y) * k_ArrowShare;
	return ArrowAt(placement, placement.min.y + (0.5f * size), placement.min.y + size, {0.87890625f, 0.12890625f},
	               {0.99609375f, 0.24609375f});
}

Arrow DownArrow(const Placement& placement)
{
	const float size = (placement.max.y - placement.min.y) * k_ArrowShare;
	return ArrowAt(placement, placement.max.y - size, placement.max.y - (0.5f * size), {0.75390625f, 0.00390625f},
	               {0.87109375f, 0.12109375f});
}

namespace
{
/// The bounds the words of one pass are drawn within
struct WordBounds
{
	float left;
	float right;
	float clipTop;
	float fadeTopEnd;
	float fadeBottomStart;
	float clipBottom;
};

/// A row's alpha, from the colour's (0 to 255): fading in over the top of the box, then out under the bottom line, in
/// whole steps
float RowAlpha(float y, int alpha, const WordBounds& bounds)
{
	int shown = alpha;
	if (y < bounds.fadeTopEnd)
	{
		shown = static_cast<int>(Fraction(y, bounds.clipTop, bounds.fadeTopEnd) * static_cast<float>(shown));
	}
	if (y > bounds.fadeBottomStart)
	{
		shown = static_cast<int>((1.0f - Fraction(y, bounds.fadeBottomStart, bounds.clipBottom)) * static_cast<float>(shown));
	}
	return static_cast<float>(shown) / 255.0f;
}

/// The lines of an item from a top down, as far as they show
void AddLines(std::vector<Line>& out, const std::vector<std::u16string>& lines, float top, float size, int alpha,
              const WordBounds& bounds, const WidthFn& widthOf)
{
	// Nothing is drawn where a line can't fit
	if (size * 0.5f > bounds.right - bounds.left || bounds.clipBottom - bounds.clipTop < size)
	{
		return;
	}
	float y = top;
	for (const auto& text : lines)
	{
		if (y > bounds.clipBottom)
		{
			break;
		}
		if (y + size > bounds.clipTop)
		{
			const float width = widthOf(text, size);
			out.push_back({
			    .text = text,
			    .at = {bounds.left + ((bounds.right - bounds.left - width) * 0.5f), y},
			    .size = size,
			    .topAlpha = RowAlpha(y, alpha, bounds),
			    .bottomAlpha = RowAlpha(y + size, alpha, bounds),
			    .clipTop = bounds.clipTop,
			    .clipBottom = bounds.clipBottom,
			});
		}
		y += size;
	}
}
} // namespace

Words LayOutWords(const Placement& placement, std::span<const std::u16string_view> items, float scrollPixels, float alpha,
                  const WrapFn& wrap, const WidthFn& widthOf)
{
	const float x0 = placement.min.x;
	const float y0 = placement.min.y;
	const float x1 = placement.max.x;
	const float y1 = placement.max.y;
	const float height = y1 - y0;
	const float size = placement.sizes.lineHeight;
	const float inset = placement.sizes.margin / 3.0f;
	const float fadeTopEnd = y0 + (height / 3.0f);
	const float bottomLine = std::max(y1 - inset, fadeTopEnd);
	const WordBounds bounds {
	    .left = x0 + inset,
	    .right = x1 - inset,
	    .clipTop = y0,
	    .fadeTopEnd = fadeTopEnd,
	    .fadeBottomStart = bottomLine,
	    .clipBottom = bottomLine + (inset * 0.25f),
	};
	const WordBounds shadowBounds {
	    .left = bounds.left + k_ShadowOffset,
	    .right = bounds.right + k_ShadowOffset,
	    .clipTop = bounds.clipTop + k_ShadowOffset,
	    .fadeTopEnd = bounds.fadeTopEnd + k_ShadowOffset,
	    .fadeBottomStart = bounds.fadeBottomStart + k_ShadowOffset,
	    .clipBottom = bounds.clipBottom + k_ShadowOffset,
	};
	const auto shadowAlpha = static_cast<int>(k_ShadowAlpha * alpha);
	const auto wordsAlpha = static_cast<int>(255.0f * alpha);

	Words words;
	float y = bottomLine + scrollPixels;
	for (const auto item : items)
	{
		const auto lines = wrap(item, bounds.right - bounds.left, size);
		const float itemHeight = static_cast<float>(lines.size()) * size;
		y -= itemHeight;
		words.contentHeight += itemHeight;
		AddLines(words.shadows, lines, y + k_ShadowOffset, size, shadowAlpha, shadowBounds, widthOf);
		AddLines(words.lines, lines, y, size, wordsAlpha, bounds, widthOf);
		if (y < y0)
		{
			break;
		}
	}
	return words;
}

void ClampScroll(Scroll& scroll, const Placement& placement)
{
	const float visible = (placement.max.y - placement.min.y) - (2.0f * placement.sizes.margin);
	float most = scroll.contentHeight != 0.0f ? scroll.contentHeight - visible : 0.0f;
	if (scroll.lineHeight != 0.0f)
	{
		most /= scroll.lineHeight;
	}
	if (most < scroll.lines && scroll.contentHeight != 0.0f)
	{
		scroll.lines = most;
	}
	scroll.lines = std::max(scroll.lines, 0.0f);
	scroll.moreBelow = scroll.lines > 1.0f;
	scroll.moreAbove = scroll.lines < most - 1.0f;
}

bool OnScreen(const Ball& ball, const View& view)
{
	const auto projected = view.project(ball.centre);
	const float depth = projected.z;
	if (depth + ball.radius < view.nearClip)
	{
		return false;
	}
	const auto fromEye = ball.origin - view.eye;
	if (glm::dot(fromEye, fromEye) < ball.radius * ball.radius)
	{
		return true;
	}
	const float reach = ball.radius * view.pixelsPerUnit / depth;
	return projected.x + reach >= 0.0f && projected.x - reach <= view.screen.x && projected.y + reach >= 0.0f &&
	       projected.y - reach <= view.screen.y;
}

} // namespace openblack::help::tip_bubble
