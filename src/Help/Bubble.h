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

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

// The help system's did-you-know bubble: its sizes by the distance, its layout on the screen, its life, its scroll and
// its drag. The drawing (the 9-slice of gatheringtext.raw, the lines and the $g / $m icons) is the Renderer's, from the
// BubbleView this gives.

namespace openblack::help
{

/// The help system's bubble uses 400, 250, 19: the near sizes and the far ones (1 / 7)
struct BubbleSizes
{
	float width {400.0f};
	float height {250.0f};
	float nearDepth {3.0f};
	float fadeInEnd {8.0f};
	float margin {40.0f}; ///< w / 2 x 0.2
	float textSize {19.0f};
	float farWidth {0.0f};  ///< w / 2 / 7
	float farHeight {0.0f}; ///< h / 2 / 7
	float farDepth {450.0f};
	float fadeOutStart {300.0f};
	float farMargin {0.0f};   ///< h / 2 / 35
	float farTextSize {0.0f}; ///< text / 7
};
[[nodiscard]] BubbleSizes DefaultBubbleSizes(float width, float height, float textSize);

/// 0 at or below a, 1 at or above b, else (v - a) / (b - a)
[[nodiscard]] float GetFrac(float v, float a, float b);

/// The bubble laid out at one camera depth
struct BubbleLayout
{
	float width;
	float height;
	float depth;
	float margin;
	float textSize;
	float fade;        ///< the distance fade
	glm::vec2 topLeft; ///< screen pixels
	float tailPlace;
	bool tailRight;
};
/// The layout after the projection: nothing below a depth of 1. `screen` is the point's projection in pixels.
[[nodiscard]] std::optional<BubbleLayout> LayoutBubble(const BubbleSizes& sizes, float depth, glm::vec2 screen, float offset,
                                                       float place);

/// The tail: u = 2 sx / W - 1 clamped to -1..1; the offset +10 when u <= 0, -10 when u > 0; the place |u| x 0.6 + 0.2
struct BubbleTail
{
	float offset;
	float place;
};
[[nodiscard]] BubbleTail TailFor(float screenX, int screenWidth);

/// The screen depth the bubble is drawn at, 5 m in front of the sign and pulled toward n = 2 x near as the camera
/// looks down (pitch 0..pi/2)
[[nodiscard]] float DrawDepth(float depth, float pitch, float nearPlane);

/// One vertex of the bubble's shape in screen pixels (the original turns them into world points)
struct BubbleVertex
{
	float x;
	float y;
	float u;
	float v;
	uint32_t argb;
};
/// The bubble as triangles (three vertices each): the 9-slice of gatheringtext.raw's cell (0, 0) on a 4 x 4 grid and,
/// with a tail, its quad from cell (1, 0). The colour's alpha fades from a0 / 255 at the top to a1 / 255 at the
/// bottom. (approximate) the original draws one indexed list of 16 or 20 vertices; here two triangles a quad, in the
/// grid's order
[[nodiscard]] std::vector<BubbleVertex> BubbleShape(float left, float top, float right, float bottom, float corner, float tailX,
                                                    bool tailRight, uint32_t argb, bool hasTail, uint8_t a0, uint8_t a1);

/// The bubble text's line breaking: a line grows by up to 4 characters at a time (stopping at a CR / LF or the end)
/// while its width fits `maxWidth`; on overflow it backs off to the last whitespace or U+F8FE (the break goes there
/// and drops it) or after a '-' (kept), else one character at a time until it fits. A CR / LF inside the line ends it
/// there and is dropped.
[[nodiscard]] std::vector<std::u16string> WrapGatheringText(std::u16string_view text, float maxWidth,
                                                            const std::function<float(std::u16string_view)>& widthOf);

/// WrapGatheringText, made again only when the text, the width or the text size changed since the last call; otherwise
/// the last lines, unchanged. widthOf must be a fixed function of the text at that size, as the bubble's font is. The
/// reference holds until the next call.
class GatheringWrapCache
{
public:
	[[nodiscard]] const std::vector<std::u16string>& Wrap(std::u16string_view text, float maxWidth, float size,
	                                                      const std::function<float(std::u16string_view)>& widthOf);

private:
	bool _kept {false};
	std::u16string _text;
	float _maxWidth {0.0f};
	float _size {0.0f};
	std::vector<std::u16string> _lines;
};

/// The bubble's state
class Bubble
{
public:
	Bubble();

	/// The scroll goes to the last line (1000) and the text height to 0
	void Restart();
	/// The thing is still on the screen: the life back to 3.0
	void KeepAlive() { _life = 3.0f; }
	/// The life -= the frame's game time in ms x 0.0002, not below 0
	void Age(float frameMs);
	[[nodiscard]] float Life() const { return _life; }

	/// The scroll in lines clamped to the text that does not fit, measured with the text height of the last draw (the
	/// draw rewrites it after this); returns {more below, more above}
	struct ScrollMarks
	{
		bool below;
		bool above;
	};
	ScrollMarks ClampScroll(float boxHeight, float margin, float lineHeight);
	/// The drawn text's height (0 when the draw starts, then each part and icon added)
	void SetTextHeight(float height) { _textHeight = height; }
	[[nodiscard]] float TextHeight() const { return _textHeight; }
	/// While the hand drags the bubble, the scroll follows the mouse's y; the first frame only takes the point
	void Drag(bool dragging, int mouseY, float lineHeight);
	[[nodiscard]] float Scroll() const { return _scroll; }

	[[nodiscard]] const BubbleSizes& Sizes() const { return _sizes; }

	/// The drawn parts (the text, a gap, the closing text), each with its own wrapped lines
	static constexpr size_t k_Parts = 3;
	[[nodiscard]] GatheringWrapCache& PartWrap(size_t part) { return _partWraps.at(part); }

private:
	BubbleSizes _sizes;
	float _life {0.0f};
	float _scroll {0.0f}; ///< in lines
	float _textHeight {0.0f};
	bool _dragging {false};
	int _dragY {0};
	std::array<GatheringWrapCache, k_Parts> _partWraps {};
};

} // namespace openblack::help
