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
#include <vector>

#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "Enums.h"
#include "Gestures/GestureRequests.h"

/// The hints of the gestures the hand waits for: a row of squares along the bottom of the screen from its right edge,
/// each showing the gesture's picture in the player's colour with a bright dot running along the gesture's shape, and
/// above it a picture of what the gesture gives. While the leash picker is open, each other leash the creature knows is
/// hinted, its collar turning slowly over its square. The whole row fades out while the advisors' words are on the
/// screen and back in after.
namespace openblack::gesture_hints
{

/// A gesture hinted
struct Hint
{
	GestureType gesture {GestureType::None};
	/// Where it goes in the row: the greatest nearest the screen's right edge
	int order {0};
	/// The leash it picks, whose collar is pictured over it
	LeashType leash {LeashType::None};
	/// The gesture's shape, its points taken evenly along it in a square 0 to 1 across (x to the right, z down)
	std::vector<glm::vec3> path;
	/// How long the dot takes to run along it, in seconds
	float period {0.0f};
	/// How far along it the dot has run, 0 to 1
	float dot {0.0f};

	bool operator==(const Hint&) const = default;
};

/// A leash's hint comes in the row after its number plus this
inline constexpr int k_LeashOrderBase = 3;
/// The leash gesture's own hint, with the collar of the leash it is about, comes nearer the screen's left than any
/// leash's
inline constexpr int k_LeashGestureOrder = 2;

/// A gesture's shape: its points evenly along it, and its length in the square it was drawn in (see
/// gesture::TrailSymbol)
struct Shape
{
	std::vector<glm::vec3> points;
	float length {0.0f};
};
/// A gesture's shape from the game's files, none without one
using ShapeOf = std::function<std::optional<Shape>(GestureType)>;
/// The dot runs along a circle in this many seconds, along other gestures in as many more as they are longer
inline constexpr float k_CirclePeriod = 2.0f;

/// The hints for what the hand waits for: the leash gesture with the collar of the leash it is about, and a hint for
/// each leash the picker offers, in the order of the row. A gesture
/// without a shape of its own takes the circle's.
[[nodiscard]] std::vector<Hint> HintsFor(std::span<const gesture::Request> requests, const ShapeOf& shapeOf);
/// Whether a new row of hints differs from the one shown; the row is then replaced, every dot starting again
[[nodiscard]] bool Differs(std::span<const Hint> shown, std::span<const Hint> next);

/// The row's squares are these shares of the width of the screen: the square's side, the gap between squares and the
/// margin from the screen's right edge. They sit on the screen's bottom edge.
inline constexpr float k_SquareShare = 0.035f;
inline constexpr float k_GapShare = 0.005f;
inline constexpr float k_MarginShare = 0.015f;

/// A rectangle on the screen in pixels
struct Box
{
	glm::vec2 min {0.0f};
	glm::vec2 max {0.0f};

	bool operator==(const Box&) const = default;
};
/// The width the row is laid out on: the screen's, or that of a 4:3 screen as tall on a wider one
[[nodiscard]] float RowWidth(glm::vec2 screen);
/// The square of the row's nth hint, counting from the right
[[nodiscard]] Box Square(size_t slot, glm::vec2 screen);
/// The square over a hint's square, where what it gives is pictured
[[nodiscard]] Box PictureSquare(const Box& square);
/// The pixel a point of a gesture's square falls on in a hint's square, whole pixels towards the corner
[[nodiscard]] glm::vec2 PixelOf(const Box& square, glm::vec2 point);

/// The gesture's picture and the dot are flat squares facing the camera this far in front of it, half these sizes
/// across: drawn through the camera, they are sized by its field of view
inline constexpr float k_SpriteDepth = 10.0f;
inline constexpr float k_PictureHalfSize = 0.2f;
inline constexpr float k_DotHalfSize = 0.1f;
/// The side in pixels of a square facing the camera, of a half size, for a row of a width and the camera's horizontal
/// field of view (radians)
[[nodiscard]] float SpriteSide(float halfSize, float rowWidth, float horizontalFov);

/// The row fades in and out at this much a second
inline constexpr float k_FadeRate = 2.0f;
/// The row's opacity some seconds of game time on: falling while the advisors' words show, rising otherwise. The row
/// starts fully shown.
[[nodiscard]] float Fade(float alpha, float seconds, bool wordsShowing);

/// Past the end of the gesture the dot carries on from this far along it, while a second dot runs that much behind it
inline constexpr float k_DotOverlap = 0.1f;
/// The dot some seconds on, along a gesture it runs along in `period` seconds
[[nodiscard]] float StepDot(float dot, float seconds, float period);
/// Where the dots are drawn along the gesture: the dot, and a second one a little behind it once it is near the end
[[nodiscard]] std::vector<float> DotsAt(float dot);
/// How bright a dot is: dark at both ends of the gesture, full at its middle
[[nodiscard]] float DotBrightness(float at);
/// The point a share of the way along a gesture's path: between its points either side, the first before the start
/// and the last past the end
[[nodiscard]] glm::vec3 PointAlong(std::span<const glm::vec3> path, float at);
/// The dot is white, at this opacity of 255, added to what is under it
inline constexpr uint8_t k_DotAlpha = 150;
/// A dot's shade of white, of 255: its brightness at the row's opacity
[[nodiscard]] uint8_t DotShade(float at, float alpha);
/// The gesture's picture is drawn in the player's colour (0xRRGGBB), every channel and its opacity scaled by the
/// row's: red, green, blue and alpha of 255
[[nodiscard]] std::array<uint8_t, 4> PictureColour(uint32_t rgb, float alpha);
/// The dot is cut from the sprite sheet's picture with this number, in its rows of eight
inline constexpr int k_DotPicture = 40;
inline constexpr int k_DotSheetSide = 8;
/// The gestures' pictures come four rows of four to a sheet
inline constexpr int k_GestureSheetSide = 4;
inline constexpr int k_GesturesPerSheet = k_GestureSheetSide * k_GestureSheetSide;

/// The collar of a leash pictured over its hint, seen through a camera of its own: from a little above, looking at it
/// over its square from the same distance as the screen's camera sees the hints, turning slowly
inline constexpr glm::vec3 k_CollarEye {0.0f, 2.0f, 0.0f};
inline constexpr glm::vec3 k_CollarAt {0.0f, 0.0f, 10.0f};
inline constexpr float k_CollarScale = 0.15f;
/// How a collar is turned: its three axes
struct Axes
{
	glm::vec3 x {1.0f, 0.0f, 0.0f};
	glm::vec3 y {0.0f, 1.0f, 0.0f};
	glm::vec3 z {0.0f, 0.0f, 1.0f};

	bool operator==(const Axes&) const = default;
};
/// A collar some seconds on: it turns about its own three axes in turn, each leash at its own pace
[[nodiscard]] Axes Tumble(const Axes& axes, LeashType leash, float seconds);

/// Where a collar is drawn in its camera's space: at the point the camera looks at, turned and scaled
[[nodiscard]] glm::mat4 CollarModel(const Axes& axes);
/// The camera a collar is seen through
[[nodiscard]] glm::mat4 CollarView();
/// How many pixels across the screen a thing a unit to the side at a unit's distance from the screen's camera is, for a
/// row of a width and the camera's horizontal field of view (radians)
[[nodiscard]] float FocalPixels(float rowWidth, float horizontalFov);
/// A rectangle of whole pixels: its left, top, width and height
struct PixelRect
{
	int32_t x {0};
	int32_t y {0};
	int32_t width {0};
	int32_t height {0};

	bool operator==(const PixelRect&) const = default;
};
/// The whole pixels a picture's square covers
[[nodiscard]] PixelRect PixelsOf(const Box& square);
/// A collar camera's projection onto the pixels of its rectangle: as the screen's camera projects, but centred on a
/// point of the screen (the middle of the picture's square), with depth from the near clip out to infinity
[[nodiscard]] glm::mat4 CollarProjection(const PixelRect& rect, glm::vec2 centre, float focalPixels, float nearClip);

/// The row as it is shown
struct Row
{
	std::vector<Hint> hints;
	float alpha {1.0f};
	/// How each leash's collar has turned, by leash number. Each starts unturned on a new land.
	std::array<Axes, 4> collars {};
};
/// The row some seconds of game time on: it fades, and while it shows the dots run and the collars of its leashes turn
void Step(Row& row, float seconds, bool wordsShowing);

} // namespace openblack::gesture_hints
