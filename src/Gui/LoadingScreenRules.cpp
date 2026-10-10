/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "LoadingScreenRules.h"

#include <cwctype>

#include <algorithm>

#include <fmt/format.h>

namespace openblack::loading
{

namespace
{
/// The tips' texts follow one another in the text scripts, the first of them named this with a number from 1
constexpr std::string_view k_TipTextPrefix = "HELP_TEXT_TOTD_";
/// The mark the game puts in place of words it can't show
constexpr char16_t k_HiddenWord = u'\xF8FE';
/// The front end's boxes are laid out on this
constexpr int32_t k_LayoutWidth = 800;
constexpr int32_t k_LayoutHeight = 600;
/// The "please wait" banner waits this long before it fades in
constexpr int32_t k_PleaseWaitDelay = 5000;
/// The blurred picture is a texture of this size
constexpr uint32_t k_BlurTextureSize = 256;
/// Each blurred pixel averages a block this big, and the blocks step half that
constexpr uint32_t k_BlurBlock = 16;
constexpr uint32_t k_BlurStep = 8;

[[nodiscard]] float LayoutScale(int32_t screenWidth, int32_t screenHeight) noexcept
{
	return std::max(850.0f / static_cast<float>(screenWidth), 650.0f / static_cast<float>(screenHeight));
}

[[nodiscard]] bool FitsLayout(int32_t screenWidth, int32_t screenHeight) noexcept
{
	return screenWidth >= k_LayoutWidth && screenHeight >= k_LayoutHeight;
}

/// The red, green and blue masks of a 16-bit format and the bit set as opaque alpha
struct Masks
{
	uint32_t red;
	uint32_t green;
	uint32_t blue;
	uint32_t alpha;
};

[[nodiscard]] constexpr Masks MasksOf(Rgb16 format) noexcept
{
	return format == Rgb16::Rgb555 ? Masks {.red = 0x7C00, .green = 0x3E0, .blue = 0x1F, .alpha = 0x8000}
	                               : Masks {.red = 0xF800, .green = 0x7E0, .blue = 0x1F, .alpha = 0};
}
} // namespace

int32_t PickTip(uint32_t milliseconds, bool hasProfiles) noexcept
{
	if (!hasProfiles)
	{
		return 0;
	}
	const auto roll = static_cast<int32_t>((milliseconds >> 4) % static_cast<uint32_t>(k_TipCount));
	int32_t tip = roll + 1;
	while (std::ranges::find(k_SkippedTips, tip) != k_SkippedTips.end())
	{
		tip = tip % k_TipCount + 1;
	}
	return tip;
}

std::string TipTextName(int32_t tip)
{
	return fmt::format("{}{:02}", k_TipTextPrefix, tip + 1);
}

std::u16string BlankControlCodes(std::u16string_view text)
{
	std::u16string blanked(text);
	for (size_t i = 0; i < blanked.size(); ++i)
	{
		if (blanked[i] != u'$' && blanked[i] != u'\\')
		{
			continue;
		}
		for (; i < blanked.size(); ++i)
		{
			if (std::iswspace(static_cast<wint_t>(blanked[i])) != 0 || blanked[i] == k_HiddenWord)
			{
				break;
			}
			blanked[i] = u' ';
		}
	}
	return blanked;
}

Layout LayoutFor(int32_t screenWidth, int32_t screenHeight) noexcept
{
	const auto width = static_cast<float>(screenWidth);
	const auto height = static_cast<float>(screenHeight);
	const auto textLeft = static_cast<int32_t>(width * 0.125f);
	const auto textRight = static_cast<int32_t>(width * 0.875f);
	// The game measures the band's top down the screen by its width
	const auto textTop = static_cast<int32_t>(width * 0.6f);
	const auto textBottom = static_cast<int32_t>(height * 0.95f);
	const auto pictureX = static_cast<int32_t>(width * 0.18f);
	const auto pictureY = static_cast<int32_t>(height * 0.05f);
	const auto pictureWidth = static_cast<int32_t>(width * 0.64f);
	const auto pictureHeight = static_cast<int32_t>(height * 0.64f);
	return {
	    .picture = {.left = pictureX, .top = pictureY, .right = pictureX + pictureWidth, .bottom = pictureY + pictureHeight},
	    .band = {.left = 0, .top = textTop, .right = screenWidth, .bottom = textBottom},
	    .text = {.left = textLeft, .top = textTop, .right = textRight, .bottom = textBottom},
	    .textSize = screenWidth / 20 - 2,
	    .versionSize = width * 0.02f,
	};
}

Fade FadeOf(float fade) noexcept
{
	const float sharp = std::clamp((fade - 0.5f) * 2.0f, 0.0f, 1.0f);
	const float alpha = std::min(fade * 2.0f, 1.0f);
	return {
	    .alpha = static_cast<uint8_t>(static_cast<int32_t>(alpha * 255.0f)),
	    .sharp = sharp,
	    .sharpAlpha = static_cast<uint8_t>(static_cast<int32_t>(sharp * 255.0f)),
	};
}

std::array<int32_t, 2> Adjust(std::array<int32_t, 2> layout, int32_t screenWidth, int32_t screenHeight) noexcept
{
	if (FitsLayout(screenWidth, screenHeight))
	{
		return {layout[0] + (screenWidth - k_LayoutWidth) / 2, layout[1] + (screenHeight - k_LayoutHeight) / 2};
	}
	const float scale = LayoutScale(screenWidth, screenHeight);
	return {
	    static_cast<int32_t>((static_cast<float>(screenWidth) - 800.0f / scale) * 0.5f + static_cast<float>(layout[0]) / scale),
	    static_cast<int32_t>((static_cast<float>(screenHeight) - 600.0f / scale) * 0.5f +
	                         static_cast<float>(layout[1]) / scale),
	};
}

std::array<int32_t, 2> Unadjust(std::array<int32_t, 2> screen, int32_t screenWidth, int32_t screenHeight) noexcept
{
	if (FitsLayout(screenWidth, screenHeight))
	{
		return {screen[0] - (screenWidth - k_LayoutWidth) / 2, screen[1] - (screenHeight - k_LayoutHeight) / 2};
	}
	const float scale = LayoutScale(screenWidth, screenHeight);
	const auto x =
	    static_cast<int32_t>(static_cast<float>(screen[0]) - (static_cast<float>(screenWidth) - 800.0f / scale) * 0.5f);
	const auto y =
	    static_cast<int32_t>(static_cast<float>(screen[1]) - (static_cast<float>(screenHeight) - 600.0f / scale) * 0.5f);
	return {static_cast<int32_t>(static_cast<float>(x) * scale), static_cast<int32_t>(static_cast<float>(y) * scale)};
}

int32_t UnadjustSize(int32_t size, int32_t screenWidth, int32_t screenHeight) noexcept
{
	if (FitsLayout(screenWidth, screenHeight))
	{
		return size;
	}
	return static_cast<int32_t>(static_cast<float>(size) * LayoutScale(screenWidth, screenHeight));
}

Bar BarFor(const Layout& layout, float progress, int32_t screenWidth, int32_t screenHeight) noexcept
{
	const auto [left, top] = Unadjust({layout.picture.left - 1, layout.picture.top - 1}, screenWidth, screenHeight);
	const auto [right, bottom] = Unadjust({layout.picture.right + 1, layout.picture.bottom + 1}, screenWidth, screenHeight);
	const int32_t border = UnadjustSize(screenWidth / 50, screenWidth, screenHeight);
	const float barTop = static_cast<float>(border) * 1.5f;
	const float barBottom = static_cast<float>(border) * 2.5f;
	const auto below = static_cast<float>(bottom);

	const float phase = progress - static_cast<float>(static_cast<int32_t>(progress));
	const auto sweep = [left, right](float at) {
		const int32_t width = right - left + 120;
		const auto start = static_cast<int32_t>(at * static_cast<float>(width) + static_cast<float>(left) - 60.0f);
		const int32_t end = start + 120;
		const auto clampToBar = [left, right](int32_t x) { return x > left + 3 ? (x < right - 3 ? x : right - 3) : left + 3; };
		return std::array {clampToBar(start), clampToBar(end)};
	};
	const auto bright = sweep(phase * 2.0f);
	const auto dark = sweep((phase - 0.5f) * 2.0f);

	const auto sweepTop = static_cast<int32_t>(below + barTop + 3.0f);
	return {
	    .outline = {.left = left, .top = top, .right = right, .bottom = bottom},
	    .border = border,
	    .body = {.left = left,
	             .top = static_cast<int32_t>(below + barTop),
	             .right = right,
	             .bottom = static_cast<int32_t>(below + barBottom)},
	    .brightStart = bright[0],
	    .brightEnd = bright[1],
	    .darkStart = dark[0],
	    .darkEnd = dark[1],
	    .sweepTop = sweepTop,
	    .brightBottom = static_cast<int32_t>(below + barBottom - 4.0f),
	    .darkBottom = static_cast<int32_t>(below + barBottom - 3.0f),
	    .topShadow = {.left = left + 3,
	                  .top = sweepTop,
	                  .right = right - 4,
	                  .bottom = static_cast<int32_t>(below + static_cast<float>(border) * 1.8f + 3.0f)},
	    .leftShadow = {.left = left + 3,
	                   .top = sweepTop,
	                   .right = static_cast<int32_t>(static_cast<float>(border) * 0.3f + static_cast<float>(left + 3)),
	                   .bottom = static_cast<int32_t>(below + barBottom - 4.0f)},
	};
}

int32_t FitTextSize(const Layout& layout, const std::function<float(int32_t size)>& heightAt)
{
	const auto bandHeight = static_cast<float>(layout.text.bottom - layout.text.top);
	int32_t size = layout.textSize;
	float height = heightAt(size);
	while (height >= bandHeight && size > 8)
	{
		size -= 2;
		height = heightAt(size);
	}
	return size;
}

std::u16string VersionText(uint32_t version, uint32_t developerPatch)
{
	auto text = fmt::format("V{:01}.{:02}", version / 100, version % 100);
	if (developerPatch != 0)
	{
		text += fmt::format(" Beta {}", developerPatch);
	}
	return {text.begin(), text.end()};
}

bool LoadingClock::Tick(uint32_t now, Mode mode) noexcept
{
	if (_time == 0)
	{
		_last = now;
		_time = 1;
	}
	const uint32_t elapsed = now - _last;
	if (elapsed < k_RedrawMilliseconds)
	{
		return false;
	}
	_time +=
	    static_cast<int32_t>(mode == Mode::Tips && elapsed > k_LongestStepMilliseconds ? k_LongestStepMilliseconds : elapsed);
	_last = now;
	return true;
}

uint8_t PleaseWaitAlpha(int32_t loadingTime) noexcept
{
	if (loadingTime <= k_PleaseWaitDelay)
	{
		return 0;
	}
	return static_cast<uint8_t>(std::clamp((loadingTime - k_PleaseWaitDelay) / 2, 0, 255));
}

PleaseWaitBanner PleaseWaitBannerFor(int32_t screenWidth, int32_t screenHeight) noexcept
{
	const int32_t middle = screenHeight >> 1;
	const int32_t top = middle - 30;
	const int32_t bottom = middle + 30;
	return {
	    .band = {.left = -1, .top = top, .right = screenWidth, .bottom = bottom},
	    .edge = static_cast<int32_t>(static_cast<float>(screenWidth) * 0.4f),
	    .shadowAbove = {.left = -1, .top = top - 15, .right = screenWidth, .bottom = top},
	    .shadowBelow = {.left = -1, .top = bottom, .right = screenWidth, .bottom = bottom + 15},
	};
}

TextBox TextBoxFor(Rect layout, int32_t start, int32_t size, int32_t screenWidth, int32_t screenHeight) noexcept
{
	int32_t width = layout.right - layout.left;
	int32_t height = layout.bottom - layout.top;
	int32_t below = start - layout.top;
	auto textSize = static_cast<float>(size);
	const auto topLeft = Adjust({layout.left, layout.top}, screenWidth, screenHeight);
	// The scale the layout is drawn at: one on screens big enough for it, more than one on smaller ones
	const float scale = FitsLayout(screenWidth, screenHeight) ? 1.0f : LayoutScale(screenWidth, screenHeight);
	textSize /= scale;
	below = static_cast<int32_t>(static_cast<float>(below) / scale);
	width = static_cast<int32_t>(static_cast<float>(width) / scale);
	height = static_cast<int32_t>(static_cast<float>(height) / scale);
	return {
	    .box = {.left = topLeft[0], .top = topLeft[1], .right = topLeft[0] + width, .bottom = topLeft[1] + height},
	    .start = topLeft[1] + below,
	    .size = textSize,
	};
}

std::vector<uint16_t> ToSixteenBit(std::span<const uint8_t> bgrx, uint32_t width, uint32_t height, Rgb16 format)
{
	std::vector<uint16_t> pixels(static_cast<size_t>(width) * (height + k_BlurBlock), 0);
	const size_t count = std::min(static_cast<size_t>(width) * height, bgrx.size() / 4);
	for (size_t i = 0; i < count; ++i)
	{
		const uint32_t blue = bgrx[i * 4];
		const uint32_t green = bgrx[(i * 4) + 1];
		const uint32_t red = bgrx[(i * 4) + 2];
		pixels[i] = static_cast<uint16_t>(format == Rgb16::Rgb555 ? ((red >> 3) << 10) | ((green >> 3) << 5) | (blue >> 3)
		                                                          : ((red >> 3) << 11) | ((green >> 2) << 5) | (blue >> 3));
	}
	return pixels;
}

Blur BlurPicture(std::span<const uint16_t> pixels, uint32_t width, uint32_t height, Rgb16 format)
{
	const uint32_t blurWidth = width / k_BlurStep;
	const uint32_t blurHeight = height / k_BlurStep;
	if (blurWidth > k_BlurTextureSize || blurHeight > k_BlurTextureSize || blurWidth == 0 || blurHeight == 0)
	{
		return {};
	}
	const auto masks = MasksOf(format);
	Blur blur {.texels = std::vector<uint16_t>(static_cast<size_t>(k_BlurTextureSize) * k_BlurTextureSize, 0),
	           .width = blurWidth,
	           .height = blurHeight};
	for (uint32_t y = 0; y < blurHeight; ++y)
	{
		for (uint32_t x = 0; x < blurWidth; ++x)
		{
			// The block's rows are a picture's width apart, its pixels one after the other, past the row's end if need be
			const size_t first = (static_cast<size_t>(y) * k_BlurStep * width) + (static_cast<size_t>(x) * k_BlurStep);
			uint32_t red = 0;
			uint32_t green = 0;
			uint32_t blue = 0;
			for (uint32_t row = 0; row < k_BlurBlock; ++row)
			{
				for (uint32_t column = 0; column < k_BlurBlock; ++column)
				{
					const size_t at = first + (static_cast<size_t>(row) * width) + column;
					const uint32_t pixel = at < pixels.size() ? pixels[at] : 0;
					red += pixel & masks.red;
					green += pixel & masks.green;
					blue += pixel & masks.blue;
				}
			}
			blur.texels[(static_cast<size_t>(y) * k_BlurTextureSize) + x] = static_cast<uint16_t>(
			    ((blue >> 8) & masks.blue) + ((green >> 8) & masks.green) + masks.alpha + ((red >> 8) & masks.red));
		}
	}
	return blur;
}

std::array<uint8_t, 4> ToRgba8(uint16_t pixel, Rgb16 format) noexcept
{
	const auto widen5 = [](uint32_t value) { return static_cast<uint8_t>((value << 3) | (value >> 2)); };
	const auto widen6 = [](uint32_t value) { return static_cast<uint8_t>((value << 2) | (value >> 4)); };
	if (format == Rgb16::Rgb555)
	{
		return {widen5((pixel >> 10) & 0x1F), widen5((pixel >> 5) & 0x1F), widen5(pixel & 0x1F), 0xFF};
	}
	return {widen5((pixel >> 11) & 0x1F), widen6((pixel >> 5) & 0x3F), widen5(pixel & 0x1F), 0xFF};
}

} // namespace openblack::loading
