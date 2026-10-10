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

#include <array>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

/// The rules of the loading screen the game shows while a land loads: a tip of the day, its picture from tips.bik,
/// a version number and a bar that sweeps by the time spent loading. Free of any game state so that the screen and the
/// tests share them. Every position is in screen pixels unless it says it is in the 800 by 600 layout of the front
/// end's boxes.
namespace openblack::loading
{

/// Tips 1 to 34 are picked once a player has a profile; tip 0 greets a first run
inline constexpr int32_t k_TipCount = 34;
/// Two tips are never picked: the next one shows instead
inline constexpr std::array<int32_t, 2> k_SkippedTips = {29, 32};
/// The bar sweeps across once every 15 seconds of loading
inline constexpr int32_t k_BarPeriodMilliseconds = 15000;
/// The screen is drawn again no sooner than this after the last time
inline constexpr uint32_t k_RedrawMilliseconds = 150;
/// While a land loads, a long wait between redraws counts as no more than this
inline constexpr uint32_t k_LongestStepMilliseconds = 350;

/// The tip shown for a time in milliseconds since the computer started: its 16 ms count picks one of 34, and without
/// any profile the first tip, which explains what the tips are
[[nodiscard]] int32_t PickTip(uint32_t milliseconds, bool hasProfiles) noexcept;

/// The name of a tip's text in the game's text scripts: tip 0 is the first of the tips of the day
[[nodiscard]] std::string TipTextName(int32_t tip);
/// The name of the "Loading..." text of the banner shown for a script's map
inline constexpr std::string_view k_PleaseWaitTextName = "HELP_TEXT_DIALOG_ADDITION_85";

/// The text with the codes that tell the game to show keys or pictures blanked: each run from a $ or a backslash up
/// to the next space, the hidden-word mark or the end becomes spaces
[[nodiscard]] std::u16string BlankControlCodes(std::u16string_view text);

/// An integer rectangle from its top left to its bottom right
struct Rect
{
	int32_t left {0};
	int32_t top {0};
	int32_t right {0};
	int32_t bottom {0};

	bool operator==(const Rect&) const = default;
};

/// Where the parts of the screen go on a screen of a size
struct Layout
{
	/// The tip's picture, stretched to 64% of the screen each way
	Rect picture;
	/// The grey band behind the text. Its top is 60% of the screen's width down, as the game has it
	Rect band;
	/// The text is wrapped to this, inside the band
	Rect text;
	/// The size the text starts at before it is made to fit
	int32_t textSize {0};
	/// The size of the version number in the bottom right corner
	float versionSize {0.0f};

	bool operator==(const Layout&) const = default;
};
[[nodiscard]] Layout LayoutFor(int32_t screenWidth, int32_t screenHeight) noexcept;

/// How far a screen has faded in, from 0 to 1. The backdrop, blurred picture and text reach full strength in the first
/// half; the sharp picture crosses over the blurred one in the second
struct Fade
{
	/// The alpha everything is drawn with, 0 to 255
	uint8_t alpha {0};
	/// How far the sharp picture has come in, 0 to 1
	float sharp {0.0f};
	/// The sharp picture's alpha, 0 to 255
	uint8_t sharpAlpha {0};

	bool operator==(const Fade&) const = default;
};
[[nodiscard]] Fade FadeOf(float fade) noexcept;

/// A coloured box's alpha drawn at the screen's alpha: its own alpha times that, by 256
[[nodiscard]] constexpr uint8_t BoxAlpha(uint8_t colourAlpha, uint8_t drawAlpha) noexcept
{
	return static_cast<uint8_t>((static_cast<uint32_t>(colourAlpha) * drawAlpha) >> 8);
}

/// The front end's mapping between the screen and its 800 by 600 layout: centred a pixel a unit on screens at least
/// that big, scaled down otherwise
[[nodiscard]] std::array<int32_t, 2> Adjust(std::array<int32_t, 2> layout, int32_t screenWidth, int32_t screenHeight) noexcept;
[[nodiscard]] std::array<int32_t, 2> Unadjust(std::array<int32_t, 2> screen, int32_t screenWidth,
                                              int32_t screenHeight) noexcept;
[[nodiscard]] int32_t UnadjustSize(int32_t size, int32_t screenWidth, int32_t screenHeight) noexcept;

/// The loading bar under the picture and the frame round it, in the 800 by 600 layout
struct Bar
{
	/// The frame's outline, a pixel outside the picture
	Rect outline;
	/// The frame's soft shadow is this wide
	int32_t border {0};
	/// The bevelled box of the bar
	Rect body;
	/// The bright sweep: solid from the bar's left to `start`, fading out to `end`
	int32_t brightStart {0};
	int32_t brightEnd {0};
	/// The dark sweep that follows it
	int32_t darkStart {0};
	int32_t darkEnd {0};
	/// The tops and bottoms of the sweeps
	int32_t sweepTop {0};
	int32_t brightBottom {0};
	int32_t darkBottom {0};
	/// The shadow along the inside of the bar's top, and along its left
	Rect topShadow;
	Rect leftShadow;

	bool operator==(const Bar&) const = default;
};
/// `progress` is the loading time over the bar's period: each whole period the sweeps run across again
[[nodiscard]] Bar BarFor(const Layout& layout, float progress, int32_t screenWidth, int32_t screenHeight) noexcept;

/// The size the text is drawn at: from the starting size down by two while its wrapped height doesn't fit the band and
/// it is still over 8. `heightAt` gives the wrapped height at a size
[[nodiscard]] int32_t FitTextSize(const Layout& layout, const std::function<float(int32_t size)>& heightAt);

/// The version number shown, from the game's version (100 for 1.00) and, for a developer's patch, its number
[[nodiscard]] std::u16string VersionText(uint32_t version, uint32_t developerPatch);

/// The time the loading screen has been loading for, which drives its bar. It doesn't start again with each load
class LoadingClock
{
public:
	/// What the screen is drawn as while loading
	enum class Mode : uint8_t
	{
		/// The tips screen, for the start of a game or a saved one
		Tips,
		/// The "please wait" banner, for a script loading a map
		PleaseWait,
	};

	/// Back to no time at all, as a new game's setting up and a script's map do
	void Reset() noexcept { _time = 0; }

	/// A redraw point reached at `now`, in milliseconds. The first only notes the time; afterwards a redraw is due when
	/// 150 ms have passed, the tips screen counting a longer wait as 350 ms. True when it is to be drawn
	bool Tick(uint32_t now, Mode mode) noexcept;

	/// The loading time in milliseconds
	[[nodiscard]] int32_t Time() const noexcept { return _time; }
	/// How far along the bar's sweeps are
	[[nodiscard]] float Progress() const noexcept
	{
		return static_cast<float>(_time) / static_cast<float>(k_BarPeriodMilliseconds);
	}

private:
	int32_t _time {0};
	uint32_t _last {0};
};

/// The "please wait" banner's alpha: none for the first five seconds, then fading in over half a second
[[nodiscard]] uint8_t PleaseWaitAlpha(int32_t loadingTime) noexcept;

/// The "please wait" banner across the middle of a screen
struct PleaseWaitBanner
{
	/// The band, 60 pixels high, from a pixel left of the screen to its right
	Rect band;
	/// The band's darker ends reach this far in from each side
	int32_t edge {0};
	/// The shadows fading out above and below it, 15 pixels each
	Rect shadowAbove;
	Rect shadowBelow;

	bool operator==(const PleaseWaitBanner&) const = default;
};
[[nodiscard]] PleaseWaitBanner PleaseWaitBannerFor(int32_t screenWidth, int32_t screenHeight) noexcept;

/// Text of the front end wrapped in a box of its 800 by 600 layout, on the screen
struct TextBox
{
	Rect box;
	/// The first line's top
	int32_t start {0};
	float size {0.0f};

	bool operator==(const TextBox&) const = default;
};
/// A box of the layout with its first line at `start`, for text `size` high
[[nodiscard]] TextBox TextBoxFor(Rect layout, int32_t start, int32_t size, int32_t screenWidth, int32_t screenHeight) noexcept;
/// The "please wait" text's box and its shadow's, a pixel down and right, in the layout; the text is centred both ways
inline constexpr Rect k_PleaseWaitText {.left = 100, .top = 200, .right = 700, .bottom = 400};
inline constexpr Rect k_PleaseWaitShadow {.left = 101, .top = 201, .right = 701, .bottom = 401};
/// The front end's big text
inline constexpr int32_t k_BigTextSize = 35;

/// The two ways a 16-bit picture keeps its colour
enum class Rgb16 : uint8_t
{
	/// 5 bits each, the top bit set as opaque alpha
	Rgb555,
	/// 5, 6 and 5 bits
	Rgb565,
};

/// A picture of 8-bit blue, green, red and unused bytes cut to 16 bits a pixel, as the game copies its videos. Sixteen
/// rows of nothing follow the picture: the blur reads into them
[[nodiscard]] std::vector<uint16_t> ToSixteenBit(std::span<const uint8_t> bgrx, uint32_t width, uint32_t height, Rgb16 format);

/// The blurred copy of a picture the tips screen draws behind and before its sharp picture: an eighth of its size each
/// way, each pixel the average of the 16 by 16 block from it, stepped 8 pixels at a time, every channel averaged and
/// rounded down on its own. The blocks along the right run on into the next row, as the game reads the picture as one
/// run of pixels. Laid out in a 256 by 256 texture from its top left; empty when the eighth doesn't fit it
struct Blur
{
	std::vector<uint16_t> texels;
	uint32_t width {0};
	uint32_t height {0};
};
[[nodiscard]] Blur BlurPicture(std::span<const uint16_t> pixels, uint32_t width, uint32_t height, Rgb16 format);

/// A 16-bit pixel as 8-bit red, green, blue and alpha, each channel widened by repeating its top bits
[[nodiscard]] std::array<uint8_t, 4> ToRgba8(uint16_t pixel, Rgb16 format) noexcept;

} // namespace openblack::loading
