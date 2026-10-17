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
#include <cstdint>

#include <array>
#include <functional>
#include <optional>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

/// The stars and the burst of light in the falling spell's film. Both are placed on the screen in pixels of the game's
/// default screen, which the film fills.
///
/// - The stars: sixteen frames of the smoke sheet that fall slowly down the screen, turn, become smaller and fade. They
///   start a time into the film and stop when the last one has faded.
/// - The burst of light: four fans of rays round the middle of the copy's body, each in its colour, that turn and
///   become longer near the end of the film. The game's time does not move them; the real time does.
namespace openblack::film_effects
{

/// The game's default screen, in pixels, which the film fills
constexpr glm::ivec2 k_Screen {800, 600};
/// The stars start after this time into the film, and the burst of light after this time
constexpr int32_t k_StarsFromMs = 13450;
constexpr int32_t k_BurstFromMs = 37750;

/// Gives a random number from a to b by the C runtime's generator
using CrtRandom = std::function<float(float, float)>;
/// Gives a random whole number from 0 to less than a limit by the local generator
using LocalRandom = std::function<uint32_t(int32_t)>;
/// Gives a random number from 0 to a limit by the local generator
using LocalFloatRandom = std::function<float(float)>;

constexpr size_t k_StarCount = 16;
/// A star: where it is on the screen (0 to 1 across and down), how far from the camera, its size and colour
struct Star
{
	glm::vec2 screen {0.0f};
	float depth {0.0f};
	float size {0.0f};
	/// The time it has lived, in its own units
	float age {0.0f};
	/// How fast it turns, and how fast it falls with it
	float spin {0.0f};
	/// How fast it becomes smaller
	float shrink {0.0f};
	/// 0xRRGGBB
	uint32_t rgb {0};
};
/// The stars as the film starts. For each star, the random numbers come in this order: its colour, across, down, its
/// depth, its size and its spin.
[[nodiscard]] std::array<Star, k_StarCount> MakeStars(const CrtRandom& random);

/// A star as it is drawn: a square that faces the camera
struct StarSprite
{
	/// Where it is on the screen, in pixels of k_Screen, and how far from the camera
	glm::ivec2 pixel {0};
	float depth {0.0f};
	float halfSize {0.0f};
	/// How far it turns, in radians: a positive turn is clockwise on the screen
	float roll {0.0f};
	/// Its frame of the sheet of 8 by 8 frames
	int32_t frame {0};
	/// 0xAARRGGBB
	uint32_t argb {0};
};
/// A star lives on by the real milliseconds of a frame. The star as it is drawn this frame, or none when it has faded.
/// It falls after it is drawn.
[[nodiscard]] std::optional<StarSprite> StepStar(Star& star, int32_t frameMs, int32_t index);
/// The frame's place in its sheet: the top left corner and the size, in parts of the sheet
[[nodiscard]] glm::vec4 SheetCell(int32_t frame);

constexpr size_t k_RayCount = 64;
/// A ray of the burst of light
struct Ray
{
	float length {0.0f};
	/// Where it starts to turn, and how fast it turns
	float angle {0.0f};
	float speed {0.0f};
	/// How much its length changes as it turns
	float wobble {0.0f};
};
/// The burst of light: its rays, and how far it has turned, opened and grown
struct Burst
{
	std::array<Ray, k_RayCount> rays {};
	float spin {0.0f};
	float open {0.0f};
	float grow {1.0f};
};
/// The rays of the burst. For each ray, the random numbers come in this order: its length (local), its angle, its speed,
/// which way it turns, its wobble, and two local draws that can make it longer or shorter.
[[nodiscard]] std::array<Ray, k_RayCount> MakeRays(const LocalFloatRandom& localFloat, const CrtRandom& random,
                                                   const LocalRandom& local);
/// The burst turns, opens and grows by the real milliseconds of a frame. Its alpha this frame, 0 to 255.
[[nodiscard]] uint32_t StepBurst(Burst& burst, int32_t frameMs);

/// One of the four fans of the burst: how long its rays are, how far it has turned, and its colour (0xAARRGGBB)
struct Fan
{
	float radius {0.0f};
	float time {0.0f};
	uint32_t argb {0};
};
constexpr size_t k_FanCount = 4;
/// The four fans of the burst at an alpha, in the order drawn
[[nodiscard]] std::array<Fan, k_FanCount> Fans(const Burst& burst, uint32_t alpha);

/// A corner of a fan: where it is on the screen, in pixels of k_Screen, its place in the atmosphere texture, and its
/// colour (0xAARRGGBB)
struct FanCorner
{
	glm::ivec2 pixel {0};
	glm::vec2 uv {0.0f};
	uint32_t argb {0};
};
constexpr size_t k_FanCorners = k_RayCount * 2;
constexpr size_t k_FanIndices = k_RayCount * 3;
/// The corners of a fan round a point of the screen: for each ray, a corner at the point in the fan's colour and one at
/// the end of the ray that is clear and black
[[nodiscard]] std::array<FanCorner, k_FanCorners> FanCorners(const Burst& burst, const Fan& fan, glm::ivec2 centre);
/// The triangles of a fan: each ray's corner at the middle, its end, and the next ray's end
[[nodiscard]] std::array<uint16_t, k_FanIndices> FanTriangles();
/// The depth of the fans in front of the camera, in parts of the near distance
constexpr float k_FanDepth = 1.1f;

/// The pixel of k_Screen at a point of the screen, from -1 to 1 across and up. The fractions are dropped.
[[nodiscard]] glm::ivec2 PixelOf(glm::vec2 screen);
/// The point in the camera's space at a pixel of k_Screen at a depth in front of the camera. The projection's scales
/// across and up give how wide the camera sees.
[[nodiscard]] glm::vec3 CameraPoint(glm::ivec2 pixel, float depth, glm::vec2 projectionScale);

} // namespace openblack::film_effects
