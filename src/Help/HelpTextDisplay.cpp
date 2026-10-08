/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "HelpTextDisplay.h"

#include <cmath>
#include <cstdio>

#include <algorithm>
#include <bit>
#include <utility>

#include "Common/TruncateToInt.h"

namespace openblack::help
{

namespace
{
using text_splitter::DrawState;
using text_splitter::Piece;
using text_splitter::Splitter;

// Layout and animation constants, as single-precision floats
constexpr float k_BoxHeightFraction = 0.12666666507720947f;       // 19/150 of the screen height
constexpr float k_BoxMarginFraction = 0.02500000037252903f;       // 1/40
constexpr float k_LineHeightFraction = 0.03333333507180214f;      // 1/30
constexpr float k_BiggerLineHeightFraction = 0.0357142873108387f; // 1/28
constexpr float k_AnimSpeed = 0.003000000026077032f;              // per ms
constexpr float k_AnimNearlyDone = 0.9998999834060669f;
constexpr float k_SlotThird = 0.3333333432674408f;  // 1/3 (also used negated)
constexpr float k_EnterScale = 1.0752688646316528f; // 1/0.93
constexpr float k_SecondScale = 0.8600000143051147f;
constexpr float k_OlderScale = 0.9300000071525574f;

/// The entry's number formatted with "%3.3f%%" or "%3.3f"
std::u16string FormatNumber(float number, bool percent)
{
	std::array<char, 64> buffer {};
	std::snprintf(buffer.data(), buffer.size(), percent ? "%3.3f%%" : "%3.3f", static_cast<double>(number));
	std::u16string out;
	for (const char* c = buffer.data(); *c != '\0'; ++c)
	{
		out.push_back(static_cast<char16_t>(static_cast<unsigned char>(*c)));
	}
	return out;
}

struct Measure
{
	float width {0.0f};
	float height {0.0f};
};

/// The widest line and the height of the wrapped text, with the font, colour and line height of the state (the codes it
/// walks change the state, as in the original: the draw that follows starts with the font and colour the measure ended
/// with). The font is never null here.
Measure MeasureText(const TextRegion& region, std::u16string_view text, float lineHeight, float number, DrawState& state,
                    const WidthFn& widthFn)
{
	const int maxLines = TruncateToInt(static_cast<float>(region.bottom - region.top + 1) / lineHeight);
	int pen = 0;
	int widest = 0;
	int line = 0;
	Piece type = Piece::Word;
	Splitter splitter(text);
	std::u16string word;
	if (maxLines > 0)
	{
		do
		{
			if (type == Piece::Stop || splitter.AtEnd())
			{
				break;
			}
			if (type == Piece::NewLine)
			{
				widest = std::max(widest, pen);
				pen = 0;
				if (++line >= maxLines)
				{
					break;
				}
			}
			else if (type == Piece::Spaced) // the width of a space
			{
				pen = TruncateToInt(widthFn(state.font, u" ", lineHeight) + static_cast<float>(pen));
			}
			type = splitter.Next(word, state, false);
			if (type == Piece::Percent || type == Piece::Number) // the type stays: no space follows
			{
				word = FormatNumber(number, type == Piece::Percent);
			}
			if (!word.empty())
			{
				const int w = TruncateToInt(widthFn(state.font, word, lineHeight));
				if ((region.right - region.left) - w - pen + 1 < 0)
				{
					widest = std::max(widest, pen);
					pen = 0;
					if (++line >= maxLines) // the word is dropped
					{
						break;
					}
				}
				pen += w;
			}
		} while (line < maxLines);
		widest = std::max(widest, pen);
	}
	Measure out {
	    .width = static_cast<float>(widest),
	    .height = static_cast<float>(std::min(line + 1, maxLines)) * lineHeight,
	};
	return out;
}

/// Measures, places and draws the words of one entry; returns the measured height. The low byte of alpha is the colour
/// alpha.
float DrawEntryText(const TextRegion& region, std::u16string_view text, int vAlign, float yOffset, bool iconFlag,
                    float lineHeight, int alpha, float number, DrawState& state, const WidthFn& widthFn,
                    std::vector<TextRun>& runs)
{
	const int maxLines = TruncateToInt(static_cast<float>(region.bottom - region.top + 1) / lineHeight);
	const Measure measure = MeasureText(region, text, lineHeight, number, state, widthFn);
	const int regionHeight = region.bottom - region.top + 1;
	float yAcc = 0.0f;
	if (vAlign == 2) // bottom
	{
		yAcc = static_cast<float>(regionHeight) - measure.height;
	}
	else if (vAlign != 0) // centred (not truncated)
	{
		yAcc = (static_cast<float>(regionHeight) - measure.height) * 0.5f;
	}
	// The block is centred; every line starts at x0 (ragged right)
	const int x0 = TruncateToInt((static_cast<float>(region.right - region.left + 1) - measure.width) * 0.5f);
	int pen = x0;
	int line = 0;
	Piece type = Piece::Word;
	// (pending) the missionaries song karaoke: while that music plays, the newest text's word matching the script's
	// current word index gets a zoomed highlight box; not drawn. The word counter is kept for it.
	int wordIndex = 0;
	Splitter splitter(text);
	std::u16string word;
	if (maxLines > 0)
	{
		do
		{
			if (type == Piece::Stop || splitter.AtEnd())
			{
				break;
			}
			++wordIndex;
			if (type == Piece::NewLine)
			{
				yAcc += lineHeight;
				pen = x0;
				if (++line >= maxLines)
				{
					break;
				}
			}
			else if (type == Piece::Spaced)
			{
				pen = TruncateToInt(widthFn(state.font, u" ", lineHeight) + static_cast<float>(pen));
			}
			type = splitter.Next(word, state, iconFlag);
			if (type == Piece::Percent || type == Piece::Number)
			{
				word = FormatNumber(number, type == Piece::Percent);
				type = Piece::Spaced; // unlike the measure, a space follows the number
			}
			if (!word.empty())
			{
				const float w = widthFn(state.font, word, lineHeight);
				// Wrap when (right - left + 1) - (pen + w) < 0; pen already includes x0
				if (static_cast<float>(region.right - region.left + 1) - (static_cast<float>(pen) + w) < 0.0f)
				{
					yAcc += lineHeight;
					pen = x0;
					if (++line >= maxLines) // the rest is dropped (no ellipsis)
					{
						break;
					}
				}
				// One run per word at (left + pen, top + yAcc + yOffset), sized to the line height, clipped to the box
				TextRun run {
				    .font = state.font,
				    .text = word,
				    .x = static_cast<float>(region.left + pen),
				    .y = static_cast<float>(region.top) + yAcc + yOffset,
				    .size = lineHeight,
				    .r = state.r,
				    .g = state.g,
				    .b = state.b,
				    .a = static_cast<uint8_t>(alpha & 0xFF),
				    .clipTop = static_cast<float>(region.top),
				    .clipBottom = static_cast<float>(region.bottom),
				};
				runs.push_back(std::move(run));
				pen = TruncateToInt(static_cast<float>(pen) + w);
			}
		} while (line < maxLines);
	}
	static_cast<void>(wordIndex);
	return measure.height;
}
} // namespace

TextRegion ComputeTextRegion(int width, int height, int barPixels)
{
	const int margin = TruncateToInt(static_cast<float>(height - 2 * barPixels) * k_BoxMarginFraction);
	const int boxHeight = TruncateToInt(static_cast<float>(height) * k_BoxHeightFraction);
	TextRegion region {};
	region.left = 0;
	region.bottom = height - margin - barPixels - 1;
	region.top = region.bottom - boxHeight;
	region.right = width;
	return region;
}

int ClickCueHeight(const TextRegion& r)
{
	return (r.bottom - r.top + 1) / 3; // signed, toward zero
}

int ClickCueY(const TextRegion& r)
{
	return r.bottom - ClickCueHeight(r) / 2 + 1;
}

HelpTextDisplay::HelpTextDisplay(int screenHeight, bool biggerText)
{
	// Base line height = screen height * (1/28 or 1/30); the alpha is rewritten per entry
	const float fraction = biggerText ? k_BiggerLineHeightFraction : k_LineHeightFraction;
	_baseLineHeight = static_cast<float>(screenHeight) * fraction;
	Reset(true);
}

void HelpTextDisplay::SetFontsLoaded(bool f1, bool f3)
{
	_f1Loaded = f1;
	_f3Loaded = f3;
}

void HelpTextDisplay::Add(std::u16string text, float number, int32_t narrator, bool drawable)
{
	_newest = (_newest + 1) % k_Entries;
	_anim = 0.0f; // the slide-in restarts
	Entry& entry = _entries[static_cast<size_t>(_newest)];
	entry.used = true;
	entry.text = std::move(text); // the original keeps the pointer
	entry.number = number;
	entry.narrator = narrator;
	if (_entries[static_cast<size_t>((_newest + 5) % k_Entries)].used)
	{
		_singleLine = false;
	}
	_hidden = false;
	_boxShown = true;
	// The current text that feeds the TEXT_DRAW gate is set by the caller, not here (inferred: the gate input is taken
	// per Add). Reset leaves it alone: after ClearAllText the ring and the box are empty.
	_drawable = drawable;
}

void HelpTextDisplay::Reset(bool all)
{
	if (all)
	{
		for (Entry& entry : _entries) // text and narrator (number and colour stay)
		{
			entry.used = false;
			entry.text.clear();
			entry.narrator = 0;
		}
		_newest = 0;
	}
	// The font is cleared too (set again per entry)
	_hidden = false;
	_singleLine = false;
	_boxShown = false;
}

void HelpTextDisplay::Close()
{
	_boxShown = false;
	_hidden = true;
}

void HelpTextDisplay::SetSingleLine(bool on)
{
	_singleLine = on;
}

bool HelpTextDisplay::IsSingleLine() const
{
	return _singleLine;
}

bool HelpTextDisplay::IsTextDrawn(int textDraw) const
{
	// TEXT_DRAW 0 -> never; 1 -> the current text's drawable flag (no current text -> drawn); other -> always
	bool gate = true;
	if (textDraw == 0)
	{
		gate = false;
	}
	else if (textDraw == 1)
	{
		gate = _drawable;
	}
	return gate && !_hidden;
}

void HelpTextDisplay::Advance(float dtMs, int textDraw)
{
	if (!IsTextDrawn(textDraw)) // the original returns before the animation
	{
		return;
	}
	// anim += dt * 0.003, clamped to 1
	_anim = dtMs * k_AnimSpeed + _anim; // each step rounded to a float (the FPU at 24 bits)
	if (_anim > 1.0f)
	{
		_anim = 1.0f;
	}
}

TextFrame HelpTextDisplay::Layout(int width, int height, int barPixels, int textDraw, bool topToBottom,
                                  const WidthFn& widthFn) const
{
	TextFrame frame;
	// The region is recomputed every frame
	const TextRegion region = ComputeTextRegion(width, height, barPixels);
	frame.box = region;
	// The box only needs the TEXT_DRAW gate, not the hidden flag (Close clears the box flag anyway). Skipping the
	// whole draw in some game modes is the caller's job.
	bool gate = true;
	if (textDraw == 0)
	{
		gate = false;
	}
	else if (textDraw == 1)
	{
		gate = _drawable;
	}
	frame.boxShown = gate && _boxShown;
	frame.boxAlpha = 0x80;

	// The stack of texts
	if (!IsTextDrawn(textDraw))
	{
		return frame;
	}
	const float base = _baseLineHeight;
	const float anim = _anim;
	// y0 = trunc(base * (TOPTOBOTTOM ? -1/3 : 1/3)); scale 1
	int yCur = TruncateToInt(base * (topToBottom ? -k_SlotThird : k_SlotThird));
	float scaleCur = 1.0f;
	// The centred position of a single line and the slot before the first
	const int halfRegion = (region.bottom - region.top + 1) / 2;
	// (each step rounded to a float)
	float centre = 0.0f;
	int yPrev = 0;
	if (topToBottom)
	{
		centre = static_cast<float>(-halfRegion) + base * 0.5f;
		yPrev = TruncateToInt(static_cast<float>(yCur) + base);
	}
	else
	{
		centre = static_cast<float>(halfRegion) - base * 0.5f;
		yPrev = TruncateToInt(static_cast<float>(yCur) - base);
	}
	float scalePrev = k_EnterScale;
	const bool previous = _entries[static_cast<size_t>((_newest + 5) % k_Entries)].used;
	if (!previous)
	{
		if (_singleLine) // centred, no zoom (fade only)
		{
			yCur = TruncateToInt(centre);
			yPrev = yCur;
			scalePrev = 1.0f;
		}
	}
	else if (!_entries[static_cast<size_t>((_newest + 4) % k_Entries)].used && _singleLine)
	{
		yCur = TruncateToInt((static_cast<float>(yCur) - centre) * anim + centre);
		yPrev = yCur;
	}
	// Six entries while the animation runs, four once it is done
	const int count = anim < k_AnimNearlyDone ? 6 : 4;
	for (int i = 0; i < count; ++i)
	{
		const int index = (_newest - i + k_Entries) % k_Entries;
		const Entry& entry = _entries[static_cast<size_t>(index)];
		if (!entry.used)
		{
			break;
		}
		// From the previous slot to this one
		const float scale = (scaleCur - scalePrev) * anim + scalePrev;
		const float y = static_cast<float>(yCur - yPrev) * anim + static_cast<float>(yPrev);
		const float lineHeight = scale * base;
		// Alpha
		int alpha = TruncateToInt(scale * 255.0f);
		if (alpha < 0xFF)
		{
			alpha -= 20;
		}
		if (alpha < 0)
		{
			alpha = 0;
		}
		if (i == 0)
		{
			alpha = TruncateToInt(static_cast<float>(alpha) * anim); // the newest fades in
		}
		// Bottom-aligned when TOPTOBOTTOM, else top; only the newest text shows its control icon
		DrawState state;
		state.f1Loaded = _f1Loaded;
		state.f3Loaded = _f3Loaded;
		state.SetEntry(entry.narrator);
		const float textHeight = DrawEntryText(region, entry.text, topToBottom ? 2 : 0, y, i == 0, lineHeight, alpha,
		                                       entry.number, state, widthFn, frame.runs);
		if (i == 0)
		{
			frame.controlIcon = state.controlIcon;
		}
		// The next slot
		scalePrev = scaleCur;
		yPrev = yCur;
		const float slot = textHeight / scale * scaleCur;
		yCur = topToBottom ? TruncateToInt(static_cast<float>(yCur) - slot) : TruncateToInt(slot + static_cast<float>(yCur));
		scaleCur = scaleCur * (i == 0 ? k_SecondScale : k_OlderScale);
	}
	return frame;
}

namespace
{
/// Floats compared by their bits: -0 and +0 lay out differently (a $P number prints its sign) and a NaN is never kept
[[nodiscard]] bool SameBits(float a, float b)
{
	return std::bit_cast<uint32_t>(a) == std::bit_cast<uint32_t>(b) && !std::isnan(a);
}
} // namespace

bool HelpTextDisplay::SameLayoutInputs(int width, int height, int barPixels, int textDraw, bool topToBottom) const
{
	if (!_layoutInputs.has_value())
	{
		return false;
	}
	const LayoutInputs& kept = *_layoutInputs;
	const auto sameEntry = [](const Entry& a, const Entry& b) {
		return a.used == b.used && a.text == b.text && SameBits(a.number, b.number) && a.narrator == b.narrator;
	};
	return kept.width == width && kept.height == height && kept.barPixels == barPixels && kept.textDraw == textDraw &&
	       kept.topToBottom == topToBottom && std::ranges::equal(kept.entries, _entries, sameEntry) && kept.newest == _newest &&
	       SameBits(kept.baseLineHeight, _baseLineHeight) && SameBits(kept.anim, _anim) && kept.hidden == _hidden &&
	       kept.singleLine == _singleLine && kept.boxShown == _boxShown && kept.drawable == _drawable &&
	       kept.f1Loaded == _f1Loaded && kept.f3Loaded == _f3Loaded;
}

void HelpTextDisplay::KeepLayoutInputs(int width, int height, int barPixels, int textDraw, bool topToBottom) const
{
	LayoutInputs& kept = _layoutInputs.has_value() ? *_layoutInputs : _layoutInputs.emplace();
	kept.width = width;
	kept.height = height;
	kept.barPixels = barPixels;
	kept.textDraw = textDraw;
	kept.topToBottom = topToBottom;
	kept.entries = _entries; // element by element, reusing the kept strings' storage
	kept.newest = _newest;
	kept.baseLineHeight = _baseLineHeight;
	kept.anim = _anim;
	kept.hidden = _hidden;
	kept.singleLine = _singleLine;
	kept.boxShown = _boxShown;
	kept.drawable = _drawable;
	kept.f1Loaded = _f1Loaded;
	kept.f3Loaded = _f3Loaded;
}

const TextFrame& HelpTextDisplay::CachedLayout(int width, int height, int barPixels, int textDraw, bool topToBottom,
                                               const WidthFn& widthFn) const
{
	if (!SameLayoutInputs(width, height, barPixels, textDraw, topToBottom))
	{
		_layoutFrame = Layout(width, height, barPixels, textDraw, topToBottom, widthFn);
		KeepLayoutInputs(width, height, barPixels, textDraw, topToBottom);
	}
	return _layoutFrame;
}

} // namespace openblack::help
