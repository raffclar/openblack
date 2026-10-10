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
#include <string>
#include <string_view>
#include <vector>

#include "TextSplitter.h"

// The display side of the script dialogue texts: the help system's text display, its ring of six entries, the box
// region, the stack of texts with its slide-in animation and the word layout. Pure logic: no bgfx, no Locator, no game
// globals; the renderer draws the TextFrame that Layout returns (the box, then each run as raw text).

namespace openblack::help
{

/// The text box region (ints, inclusive right/bottom as the users treat them: width = right - left + 1)
struct TextRegion
{
	int left;
	int top;
	int right;
	int bottom;
};

/// The box, a full-width strip above the bottom cinema bar. barPixels = the height of one cinema bar
[[nodiscard]] TextRegion ComputeTextRegion(int width, int height, int barPixels);
/// The vertical centre of the "click to continue" icon, bottom - trunc(ClickCueHeight / 2) + 1
[[nodiscard]] int ClickCueY(const TextRegion& r);
/// The height of that icon, trunc((bottom - top + 1) / 3)
[[nodiscard]] int ClickCueHeight(const TextRegion& r);

/// One raw text draw: one word at (x, y), font size = line height, colour {r,g,b,a}, clipped vertically to
/// [clipTop, clipBottom] (the box top and bottom). z = near * 1.2 and depth 0 are the renderer's.
struct TextRun
{
	TextFont font;
	std::u16string text;
	float x;
	float y;
	float size;
	uint8_t r;
	uint8_t g;
	uint8_t b;
	uint8_t a;
	float clipTop;
	float clipBottom;
};

/// What one frame draws: the background box (black at alpha 0x80) and the words
struct TextFrame
{
	bool boxShown {false};
	TextRegion box {};
	uint8_t boxAlpha {0x80};
	std::vector<TextRun> runs;
	/// The last $M<n> of the newest text seen this frame (the BINDABLE_ACTION n whose control icon is shown), -1 when
	/// none. The control icon itself is not drawn yet.
	int controlIcon {-1};
};

/// The width of a text in a font at that font size
using WidthFn = std::function<float(TextFont, std::u16string_view, float size)>;

/// The help system's dialogue text display
class HelpTextDisplay
{
public:
	/// The ring of entries
	static constexpr int k_Entries = 6;

	/// Base line height = screenHeight * (1/30), or * (1/28) when the screen needs bigger text; computed once. Ends with
	/// Reset(true).
	explicit HelpTextDisplay(int screenHeight, bool biggerText = false);

	/// Whether Data\f1 / Data\f3 were loaded; a missing one is replaced by j0
	void SetFontsLoaded(bool f1, bool f3);

	/// Adds a text with its number and narrator. drawable = whether the text may be drawn when textDraw is 1 (its help text
	/// data allows drawing it)
	void Add(std::u16string text, float number, int32_t narrator, bool drawable);
	/// all -> ring emptied (text and narrator of the 6 entries, newest = 0); always clears the font, hidden, singleLine
	/// and box shown
	void Reset(bool all);
	/// Box hidden, then the texts hidden (not drawn)
	void Close();

	/// Set by RUN_TEXT / TEMP_TEXT
	void SetSingleLine(bool on);
	[[nodiscard]] bool IsSingleLine() const;
	[[nodiscard]] bool IsHidden() const { return _hidden; }
	[[nodiscard]] bool IsBoxShown() const { return _boxShown; }
	/// The slide-in animation of the newest text, 0..1
	[[nodiscard]] float GetAnimation() const { return _anim; }

	/// The draw gate: textDraw allows it (0 never, 1 by the text's drawable flag, other always) and the texts are not
	/// hidden
	[[nodiscard]] bool IsTextDrawn(int textDraw) const;
	/// anim += dt * 0.003, clamped to 1. dt in ms: the frame delta in the citadel, else the game time increment. The
	/// game does it inside the draw, after the gate: it does not advance when IsTextDrawn(textDraw) is false
	void Advance(float dtMs, int textDraw = 2);

	/// The frame: the region, the box and the stack of texts (each measured, then drawn word by word). textDraw =
	/// the draw gate as for IsTextDrawn, topToBottom = the newest text at the bottom of the stack.
	[[nodiscard]] TextFrame Layout(int width, int height, int barPixels, int textDraw, bool topToBottom,
	                               const WidthFn& widthFn) const;
	/// Layout, made again only when one of its inputs changed since the last call: an argument or the display's state
	/// (the ring of texts, the animation, the flags). Otherwise the last frame, unchanged. The widths must be a fixed
	/// function of the font, the text and the size, as the game's fonts are once loaded. The reference holds until the
	/// next call.
	[[nodiscard]] const TextFrame& CachedLayout(int width, int height, int barPixels, int textDraw, bool topToBottom,
	                                            const WidthFn& widthFn) const;

	/// The line height at rest
	[[nodiscard]] float GetBaseLineHeight() const { return _baseLineHeight; }
	/// The newest entry has a text
	[[nodiscard]] bool HasNewestText() const { return _entries.at(static_cast<size_t>(_newest)).used; }

private:
	struct Entry
	{
		bool used {false}; ///< Has a text
		std::u16string text;
		float number {0.0f}; ///< The value of $P / $D
		int32_t narrator {0};
	};

	/// Everything Layout reads, as of the frame in _layoutFrame
	struct LayoutInputs
	{
		int width {0};
		int height {0};
		int barPixels {0};
		int textDraw {0};
		bool topToBottom {false};
		std::array<Entry, k_Entries> entries {};
		int newest {0};
		float baseLineHeight {0.0f};
		float anim {0.0f};
		bool hidden {false};
		bool singleLine {false};
		bool boxShown {false};
		bool drawable {true};
		bool f1Loaded {true};
		bool f3Loaded {true};
	};
	[[nodiscard]] bool SameLayoutInputs(int width, int height, int barPixels, int textDraw, bool topToBottom) const;
	void KeepLayoutInputs(int width, int height, int barPixels, int textDraw, bool topToBottom) const;

	std::array<Entry, k_Entries> _entries {};
	int _newest {0};
	float _baseLineHeight {0.0f};
	float _anim {0.0f}; ///< The slide-in of the newest text, 0 to 1
	bool _hidden {false};
	bool _singleLine {false};
	bool _boxShown {false};
	bool _drawable {true}; ///< The current text may be drawn, see Add / Reset
	bool _f1Loaded {true};
	bool _f3Loaded {true};

	// CachedLayout's last frame and its inputs (none before the first call)
	mutable std::optional<LayoutInputs> _layoutInputs;
	mutable TextFrame _layoutFrame;
};

} // namespace openblack::help
