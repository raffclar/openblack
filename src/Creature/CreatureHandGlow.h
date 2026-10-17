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
#include <optional>
#include <span>
#include <vector>

#include <glm/mat3x3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

/// The glows that a creature can have on its body, as on the hands of a creature that has a creed power. Each glow is
/// the creed model with a glowing shell over it (see particles::volume_glow). The model itself is not drawn. The shell
/// is white, its texture slides with real time, and it reaches further out the more power the glow has.
namespace openblack::creature_hand_glow
{

/// Where a glow is on the body
enum class Place : uint8_t
{
	/// The left hand: the mirror bone of the right hand
	LeftHand = 0,
	/// The belly bone
	Belly = 1,
	RightHand = 2,
	/// Above the body, where it turns about the up axis with the game's time
	Circling = 3,
};
constexpr size_t k_PlaceCount = 4;

/// One glow. A new glow has a power and a scale of 1.
struct Glow
{
	float power {1.0f};
	float scale {1.0f};
	/// How far the circling glow has turned, in radians
	float turn {0.0f};
	/// How far the texture has slid across and up, 0 to 1
	glm::vec2 slide {0.0f};
};
using Glows = std::array<std::optional<Glow>, k_PlaceCount>;

/// The shell reaches out this far for each unit of power, in parts of the model's half diagonal
constexpr float k_ZoomPerPower = 2.0f;
/// How fast the texture slides across and up, in sheets a second
constexpr glm::vec2 k_SlideSpeed {2.0f, 0.2f};
/// How much of the texture the shell spans across and up
constexpr glm::vec2 k_TextureFraction {0.5f, 0.2f};
/// The number of the shell's texture in the list of the glowing shells' textures
constexpr int k_Texture = 3;
/// White, with a full alpha
constexpr uint32_t k_Argb = 0xFFFFFFFFu;
/// The glow's model is this many times the body's scale
constexpr float k_BodyScale = 1.0f;
/// The model's back axis is multiplied by this, so the model is a mirror image along that axis
constexpr float k_BackAxis = -1.0f;
/// The circling glow turns this many radians each second of the game's time
constexpr float k_TurnSpeed = 1.0f;
/// The circling glow is this many times the creature's size times k_HeightPerSize above the body's origin
constexpr float k_CirclingHeight = 1.3f;
constexpr float k_HeightPerSize = 15.0f;

/// Gives a place a glow of a scale and a power. A power of 0 removes the glow. A place without a glow gets a new one.
void Set(Glows& glows, Place place, float scale, float power);

/// How far the shell reaches out, in parts of the model's half diagonal
[[nodiscard]] float Zoom(const Glow& glow);
/// Slides the texture on by the milliseconds of a frame
void Slide(Glow& glow, int32_t frameMs);
/// Turns the circling glow on by the milliseconds of the game's time of a frame
void Turn(Glow& glow, int32_t gameMs);

/// The bones whose middle is where a hand glow is: the bones at the ends of the branches that start at the hand and at
/// each bone after it under the same parent, in the order that the skeleton is walked
[[nodiscard]] std::vector<uint32_t> BranchEnds(std::span<const uint32_t> parents, uint32_t bone);
/// The middle of some points: their sum, times one over their number
[[nodiscard]] glm::vec3 Middle(std::span<const glm::vec3> points);

/// A bone's axes in the world, each made one long
[[nodiscard]] glm::mat3 UnitAxes(const glm::mat3& axes);
/// The axes of the circling glow after it has turned
[[nodiscard]] glm::mat3 CirclingAxes(float turn);
/// Where the circling glow is: above the body's origin by the creature's size
[[nodiscard]] glm::vec3 CirclingPoint(const glm::vec3& bodyOrigin, float size);

/// The matrix that the glow's model is drawn with: unit axes and a point, scaled by the body's scale and by the glow's
/// scale, the back axis in mirror image
[[nodiscard]] glm::mat4 ModelMatrix(const glm::mat3& unitAxes, const glm::vec3& point, float bodyScale, float glowScale);

} // namespace openblack::creature_hand_glow
