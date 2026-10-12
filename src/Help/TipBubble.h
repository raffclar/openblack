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

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

/// The help's bubble that shows a "did you know" sign's tip over the sign: a rounded, see-through box with a tail
/// pointing down at the sign, the tip's words in it under the title, sized by how far away the sign is
namespace openblack::help::tip_bubble
{

/// The sizes at the near depth and their far share
struct Sizes
{
	float width {0.0f};
	float height {0.0f};
	/// The gap above the sign, the corners' radius and the tail's size
	float margin {0.0f};
	float lineHeight {0.0f};
};

/// Up to this depth in front of the camera the bubble has its near sizes, and fades in until the next; it fades out
/// from the third and has its far sizes at the last
constexpr float k_NearDepth = 3.0f;
constexpr float k_FadeInEnd = 8.0f;
constexpr float k_FadeOutStart = 300.0f;
constexpr float k_FarDepth = 450.0f;
/// Closer than this the bubble isn't drawn
constexpr float k_TooClose = 1.0f;
constexpr Sizes k_NearSizes {.width = 400.0f, .height = 250.0f, .margin = 40.0f, .lineHeight = 19.0f};
/// The far sizes: half the near width and height, and the line, a seventh; the margin a tenth of the near width's
/// seventh
[[nodiscard]] Sizes FarSizes(const Sizes& near);

/// The bubble's colour before its alpha: half its default sky blue
constexpr glm::vec3 k_BoxColour {29.0f / 255.0f, 94.0f / 255.0f, 118.0f / 255.0f};
/// The help text heading every tip: "Did you know...?"
constexpr uint32_t k_TitleText = 4118;
/// Where the scroll starts, in lines: as far down as it goes, which shows the top of a long tip
constexpr float k_StartScroll = 1000.0f;
/// Kept while the sign is on screen, counted down each frame by the game's time
constexpr float k_DisplayTime = 3.0f;
/// What a hundredth of a game turn takes off the display time
constexpr float k_DisplayTimePerHundredth = 0.0002f;

/// A value's share of the way from a to b, held to 0..1
[[nodiscard]] float Fraction(float value, float a, float b);
/// The sizes at a depth in front of the camera
[[nodiscard]] Sizes SizesAt(float depth);
/// How much of the bubble shows at a depth: fading in close to the camera and out far away
[[nodiscard]] float DistanceAlpha(float depth);

/// Where the bubble goes on the screen
struct Placement
{
	glm::vec2 min {0.0f};
	glm::vec2 max {0.0f};
	Sizes sizes;
	/// The tail's point across the screen, and whether it leans the other way (the sign on the right half)
	float tailX {0.0f};
	bool tailFlipped {false};
	float depth {0.0f};
	float distanceAlpha {1.0f};
	/// How many pixels a pixel of the default screen covers (gui::ReferenceScale): the game's sizes are in pixels
	float pixelScale {1.0f};
};
/// The bubble over a sign seen at a pixel (y down) and a depth on a screen so wide; none when too close. Its box
/// stands a margin above the sign, its tail ten pixels to the sign's middle-of-the-screen side, and reaches further
/// left of the tail the closer the sign is to either edge. Nothing keeps it on the screen. The game's sizes are
/// pixels of its default screen: every size (the box, margin, line, tail offset, words' shadow) is multiplied by
/// `pixelScale` so the bubble takes the same share of the screen as there.
[[nodiscard]] std::optional<Placement> Place(glm::vec2 anchor, float depth, float screenWidth, float pixelScale);

/// A corner of the box's shape: a pixel, a place on the bubble's texture and an alpha from 0 to 1
struct Corner
{
	glm::vec2 position {0.0f};
	glm::vec2 uv {0.0f};
	float alpha {0.0f};
};
/// A four-sided piece, its corners top left, top right, bottom right, bottom left
using Quad = std::array<Corner, 4>;
/// The box as nine pieces of the texture's first quarter, rounded by the margin and see-through at the top, and the
/// tail from its second quarter; `alpha` is the whole bubble's
[[nodiscard]] std::vector<Quad> BoxShape(const Placement& placement, float alpha);

/// The scroll arrows, shown while there is more to read above or below, blinking
struct Arrow
{
	glm::vec2 min {0.0f};
	glm::vec2 max {0.0f};
	glm::vec2 uvMin {0.0f};
	glm::vec2 uvMax {0.0f};
};
/// The arrows blink: shown 400 ms of every 750
[[nodiscard]] bool ArrowsLit(uint32_t milliseconds);
[[nodiscard]] Arrow UpArrow(const Placement& placement);
[[nodiscard]] Arrow DownArrow(const Placement& placement);
/// The arrows' colour: yellow at half alpha whatever the bubble's
constexpr glm::vec4 k_ArrowColour {1.0f, 1.0f, 0.0f, 127.0f / 255.0f};

/// The lines a text wraps into at a width and a size
using WrapFn = std::function<std::vector<std::u16string>(std::u16string_view text, float width, float size)>;
/// The width of a line of text at a size
using WidthFn = std::function<float(std::u16string_view text, float size)>;

/// The lines a text wraps into at a width, measured by `measure` (which counts the line breaks' own widths too, as the
/// game does while it looks for where to break). A line grows four characters at a time until it is too wide, then
/// breaks at the rightmost blank (dropped, with the blanks before it) or after the rightmost hyphen at which it fits,
/// else after as many characters as fit; a line break in it then ends it there. Blanks starting a line stay. Nothing
/// more comes once not even a character fits.
[[nodiscard]] std::vector<std::u16string> Wrap(std::u16string_view text, float width, float size, const WidthFn& measure);

/// A picture code in a tip: "$g<n>" shows gesture n, "$m<n>" the control that does action n (numbered as the key
/// bindings' actions). Each takes two lines of the bubble instead of words.
struct InlineCode
{
	enum class Kind : uint8_t
	{
		Gesture,
		Control,
	};
	Kind kind {Kind::Control};
	int number {0};

	[[nodiscard]] bool operator==(const InlineCode&) const = default;
};
/// The code a text starts with: "$g" or "$m" and the number after it (read as a C string to integer, so 0 when there is
/// none). None for any other text.
[[nodiscard]] std::optional<InlineCode> ReadCode(std::u16string_view text);

/// One piece of a tip, in the order the bubble stacks them from the bottom up
struct Piece
{
	enum class Kind : uint8_t
	{
		Words,
		Code,
		/// The space between a code's picture and the words above it: seven tenths of a line
		Gap,
	};
	Kind kind {Kind::Words};
	std::u16string_view words;
	InlineCode code;

	[[nodiscard]] bool operator==(const Piece&) const = default;
};
/// The pieces of an item from the bottom up. A text starting with a code is just that code (anything after its first
/// four characters is dropped). Otherwise the first "$g" (or, failing that, the first "$m") splits it: the code's
/// picture at the bottom, the gap, the words before it, and the code's picture again on top, from its first four
/// characters (the game reads such a code twice). Text with no code is one piece of words.
[[nodiscard]] std::vector<Piece> SplitCodes(std::u16string_view item);

/// The gesture pictures are four by four on each of their sheets, and the game knows this many gestures
constexpr int k_GestureCells = 16;
constexpr int k_GestureCount = 24;

/// A gesture picture to draw: which sheet, its rectangle and its corners on the sheet (cut to the words' rows), and its
/// colour
struct GestureIcon
{
	int sheet {0};
	glm::vec2 min {0.0f};
	glm::vec2 max {0.0f};
	glm::vec2 uvMin {0.0f};
	glm::vec2 uvMax {0.0f};
	glm::vec4 colour {0.0f};
};

/// A control's picture to draw: centred between the words' margins, a little above the two lines it takes, white,
/// fading in over the top third of the box and out under the bottom line like the words, but not with the bubble
struct ControlIcon
{
	int action {0};
	/// The right end it is measured back from: half a picture right of the middle
	int right {0};
	/// Its top row and size in pixels
	int top {0};
	int size {0};
	float alpha {0.0f};
};
/// Where a control's picture goes on the screen
struct ControlBox
{
	glm::vec2 min {0.0f};
	glm::vec2 max {0.0f};

	[[nodiscard]] bool operator==(const ControlBox&) const = default;
};
/// A control's picture `width` wide, right-aligned on its right end after a gap, in pixels
[[nodiscard]] ControlBox PlaceControl(const ControlIcon& icon, float width, float gap);
/// The gap between a control's picture and its (empty) label, in pixels of the default screen
constexpr float k_ControlGap = 2.0f;

/// Whether a control action has a binding to show
using ControlShownFn = std::function<bool(int action)>;

/// A line of words to draw: where its top left goes, its alpha at its top and bottom, and the rows it is cut to
struct Line
{
	std::u16string text;
	glm::vec2 at {0.0f};
	float size {0.0f};
	float topAlpha {0.0f};
	float bottomAlpha {0.0f};
	float clipTop {0.0f};
	float clipBottom {0.0f};
};
/// The words of the bubble, their shadows first
struct Words
{
	std::vector<Line> shadows;
	std::vector<Line> lines;
	/// The gesture pictures, each shadow before its picture
	std::vector<GestureIcon> gestures;
	/// The control pictures
	std::vector<ControlIcon> controls;
	/// How tall all the words the bubble got to are, for the scroll
	float contentHeight {0.0f};
};
/// The items of the bubble, from the bottom up: the tip, a blank line and the title. Each wraps on its own and is
/// centred; the stack sits on the bottom line, pushed down by the scroll (in pixels), and stops once an item reaches
/// the box's top. The words fade in over the top third of the box and out just under the bottom line; their black
/// shadows sit two pixels down and right at half their alpha. An item's picture codes (SplitCodes) take two lines each:
/// a gesture (1 to 23) always, drawn with a shadow like the words and cut to their rows; a control only when
/// `controlShown` says its action is bound, drawn only when wholly inside the box.
[[nodiscard]] Words LayOutWords(const Placement& placement, std::span<const std::u16string_view> items, float scrollPixels,
                                float alpha, const WrapFn& wrap, const WidthFn& widthOf, const ControlShownFn& controlShown);

/// The scroll, in lines: held between none and as far as the words went past the box (less a margin at each end) when
/// they were last laid out
struct Scroll
{
	float lines {k_StartScroll};
	/// How tall the words were, and their line height, when last laid out; 0 before
	float contentHeight {0.0f};
	float lineHeight {0.0f};
	/// More to read above, or below
	bool moreAbove {false};
	bool moreBelow {false};
	/// While the hand grips the bubble: the pointer's height on the screen when the words last followed it
	std::optional<float> handY;

	/// The scroll in pixels at the last line height
	[[nodiscard]] float Pixels() const { return lines * lineHeight; }
};
/// Holds the scroll to the words last laid out, for the box as it is placed now, and sets the arrows
void ClampScroll(Scroll& scroll, const Placement& placement);
/// Gripped, the words follow the hand like paper: the scroll moves by the pointer's movement down the screen, in lines
/// of the last line height, so moving the hand up brings up the lines below. The first gripped frame only notes where
/// the pointer is, and nothing moves before the words were laid out. The scroll is held to the words when next clamped.
void FollowHand(Scroll& scroll, float pointerY);
/// The hand lets go of the bubble
void LetGo(Scroll& scroll);

/// Whether a pixel is inside the box, its edges left out
[[nodiscard]] bool Inside(const Placement& placement, glm::vec2 pointer);
/// What the hand points at besides the bubble, as depths in front of the camera
struct UnderHand
{
	/// The object the interface picked
	std::optional<float> objectDepth;
	/// The land under the pointer
	std::optional<float> landDepth;
};
/// Whether the hand is on the bubble rather than on what is under it: the pointer is inside the box, and neither an
/// object nor the land under the pointer is nearer than the sign the bubble points at (an object as near wins too)
[[nodiscard]] bool HandOnBubble(const Placement& placement, glm::vec2 pointer, const UnderHand& under);

/// The sign's model as a ball: its middle in the world and its reach
struct Ball
{
	glm::vec3 centre {0.0f};
	float radius {0.0f};
	/// The model's own origin in the world
	glm::vec3 origin {0.0f};
};
/// What the bubble's on-screen check needs of the camera
struct View
{
	glm::vec3 eye {0.0f};
	/// A point's place on the screen in pixels (y down) and its depth in front of the camera, as (x, y, depth)
	std::function<glm::vec3(const glm::vec3& point)> project;
	float nearClip {1.0f};
	/// The pixels across the screen a unit covers at a depth of 1
	float pixelsPerUnit {1.0f};
	glm::vec2 screen {0.0f};
};
/// Whether a sign is on the screen for its bubble: never when its ball is wholly nearer than the near plane; always
/// when the camera is within its reach of the model's origin; else when the square round its ball's circle on the
/// screen overlaps the screen
[[nodiscard]] bool OnScreen(const Ball& ball, const View& view);

} // namespace openblack::help::tip_bubble
