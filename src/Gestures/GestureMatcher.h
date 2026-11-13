/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>

#include <numbers>
#include <optional>
#include <span>

#include <GestureFile.h>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "Enums.h"
#include "Gestures/GestureRecorder.h"

/// Matching the hand's path with the gesture templates, as the game does. The game only ever asks whether the path is
/// one particular gesture, the one it is waiting for; it tries that gesture's templates in turn and takes the first that
/// fits. A template fits when, from one of the path's key points on, the turns at the path's corners add up to the turns
/// at the template's: the difference between the two running sums may never grow past three sixteenths of a half turn,
/// and a small turn on either side may be passed over when that brings the sums closer. Some templates also need the
/// path to set off the same way, and some need its box to be about as wide, against as tall, as theirs.
namespace openblack::gesture
{

/// Turns smaller than this, about 30 degrees, may be passed over
constexpr float k_SmallTurn = std::numbers::pi_v<float> * 21.0f / 128.0f;
/// The most the turns may differ by along the way, about 34 degrees
constexpr float k_TurnTolerance = std::numbers::pi_v<float> * 3.0f / 16.0f;
/// A path whose box is less wide than this against its height is tall, more than k_WideAspect wide
constexpr float k_TallAspect = 0.15f;
constexpr float k_WideAspect = 4.0f;
/// A circle's size is the box's half width across the land, a little more
constexpr float k_CircleRadiusScale = 1.05f;

/// A template that fits the path
struct Match
{
	/// Its place in the templates
	size_t templateIndex {0};
	/// Drawn as the template's mirror image
	bool mirrored {false};
	/// The key points of the path it fits, first and last
	size_t firstKey {0};
	size_t lastKey {0};
	/// The most the turns differed by along the way, in radians: smaller is a closer fit
	float largestDifference {0.0f};
};

/// The path's width against its height on the screen, the height scaled by the screen's width over its height, as the
/// game measures it. A screen wider than 4:3 scales the height as a 4:3 screen does, so the same shape that the hand
/// draws gives the same value as on a 4:3 screen.
[[nodiscard]] float AspectOf(const ScreenBox& box, float screenAspect);
/// Whether a path of an aspect may be the template's
[[nodiscard]] bool AspectFits(float pathAspect, float templateAspect);

/// Whether the template fits the path's key points, drawn as it is or mirrored
[[nodiscard]] std::optional<Match> MatchTemplate(const gestures::GestureTemplate& gestureTemplate,
                                                 std::span<const KeyPoint> keys, bool mirrored, float screenAspect);
/// The first template of the gesture that fits the key points, as it is or, where it may be, mirrored
[[nodiscard]] std::optional<Match> Recognise(std::span<const gestures::GestureTemplate> templates, GestureType gesture,
                                             std::span<const KeyPoint> keys, float screenAspect);
/// The template of any gesture that fits the key points most closely, for the debug window
[[nodiscard]] std::optional<Match> ClosestMatch(std::span<const gestures::GestureTemplate> templates,
                                                std::span<const KeyPoint> keys, float screenAspect);

/// Where a recognised gesture is on the screen: the box round the points it was drawn with
[[nodiscard]] ScreenBox GestureBox(const GestureRecorder& recorder, const Match& match);
/// The middle of a box on the screen as the game finds it: half of its size, which counts the pixels at both ends, from
/// its smaller corner
[[nodiscard]] glm::vec2 BoxMiddle(const ScreenBox& box);
/// The pixel that a circle drawn in a box of pixels has its middle on: the box's middle, cut to whole pixels
[[nodiscard]] glm::ivec2 CircleMiddlePixel(const ScreenBox& boxInPixels);
/// The pixel at the right of a circle drawn in a box of pixels, level with its middle: half of the larger side of the box
/// out from the middle, cut to whole pixels
[[nodiscard]] glm::ivec2 CircleEdgePixel(const ScreenBox& boxInPixels);
/// How far a point is from the camera's eye
[[nodiscard]] float DistanceFromEye(glm::vec3 eye, glm::vec3 point);
/// A circle's radius on the land: the distance across the view between its middle on the land and its edge, a little
/// more. The edge is on the line from the eye through the edge's pixel, as far from the eye as the middle. "Across the
/// view" is along the land, square to the heading of the camera.
[[nodiscard]] float CircleRadius(glm::vec3 eye, glm::vec3 middle, glm::vec3 edge, float cameraHeading);

} // namespace openblack::gesture
