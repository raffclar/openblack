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
};
/// The bubble over a sign seen at a pixel (y down) and a depth on a screen so wide; none when too close. Its box
/// stands a margin above the sign, its tail ten pixels to the sign's middle-of-the-screen side, and reaches further
/// left of the tail the closer the sign is to either edge. Nothing keeps it on the screen.
[[nodiscard]] std::optional<Placement> Place(glm::vec2 anchor, float depth, float screenWidth);

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
	/// How tall all the words the bubble got to are, for the scroll
	float contentHeight {0.0f};
};
/// The items of the bubble, from the bottom up: the tip, a blank line and the title. Each wraps on its own and is
/// centred; the stack sits on the bottom line, pushed down by the scroll (in pixels), and stops once an item reaches
/// the box's top. The words fade in over the top third of the box and out just under the bottom line; their black
/// shadows sit two pixels down and right at half their alpha.
[[nodiscard]] Words LayOutWords(const Placement& placement, std::span<const std::u16string_view> items, float scrollPixels,
                                float alpha, const WrapFn& wrap, const WidthFn& widthOf);

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

	/// The scroll in pixels at the last line height
	[[nodiscard]] float Pixels() const { return lines * lineHeight; }
};
/// Holds the scroll to the words last laid out, for the box as it is placed now, and sets the arrows
void ClampScroll(Scroll& scroll, const Placement& placement);

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
