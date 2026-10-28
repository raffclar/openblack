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
#include <span>
#include <vector>

#include <glm/mat3x3.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

/// Some species have tufts of hair: groups of strands rooted on triangles of the body, each a chain of points that
/// springs back towards growing straight on from the segment before it, sags under gravity, loses speed and never
/// stretches. A group's colour, length, stiffness, damping and thickness move from its neutral look towards its evil
/// or good one with the creature's alignment. The strands are drawn as ribbons facing the camera.
namespace openblack::creature_hair
{
/// How a group of strands looks and moves
struct Look
{
	/// 0 to 255 a channel
	glm::ivec3 colour {0};
	/// In units of the hair scale
	float length {0.0f};
	/// With the length, how much of its speed a strand keeps over a second: (damping * length) squared
	float damping {0.0f};
	float stiffness {0.0f};
	/// How wide a strand is drawn and how deep its root is sunk, in units of the hair scale
	float thickness {0.0f};
};

/// A group's looks at the middle of the evil to good axis and at either end
enum class Variant : uint8_t
{
	Neutral,
	Evil,
	Good,
};
using Variants = std::array<Look, 3>;

/// A value moved from its neutral towards its evil value below an alignment of 0, its good value above
[[nodiscard]] float ByAlignment(const std::array<float, 3>& values, float alignment);
/// As ByAlignment, for a whole number, truncated
[[nodiscard]] int32_t ByAlignment(const std::array<int32_t, 3>& values, float alignment);
/// A group's look for an alignment, -1 evil to 1 good
[[nodiscard]] Look LookFor(const Variants& variants, float alignment);

/// The scale the hair of a creature of a size is measured in, in world units. Small creatures have bigger heads, and
/// hair, for their size.
[[nodiscard]] float HairScale(float size);

/// Gravity pulling the strands down, in world units a second squared
constexpr float k_Gravity = 9.81f;
/// Neither the springs' pull nor a point's speed goes over this
constexpr float k_MaxSpeed = 100.0f;
/// Steps shorter than this, in seconds, leave the speeds as they were
constexpr float k_MinStepSeconds = 1e-6f;

/// How a group's strands move at a hair scale
struct Physics
{
	/// The length of each segment, in world units
	float segmentLength;
	/// How hard a point is pulled back to where it would lie straight on from the segment before
	float stiffness;
	/// The part of its speed a point keeps over a second
	float damping;
	/// How deep under the surface each root sits
	float rootDepth;
	/// How wide a strand is drawn, either side of its line
	float halfWidth;
};
[[nodiscard]] Physics PhysicsFor(const Look& look, float scale, uint32_t segmentCount);

/// Where a strand is rooted this frame: the point on its triangle, the triangle's normal into the body and the unit
/// direction the strand grows out in. The normal is the cross product of the triangle's edges, not made unit length:
/// the root is sunk along it by the strand's root depth times its length, so deeper on bigger triangles.
struct Root
{
	glm::vec3 position;
	glm::vec3 inwardNormal;
	glm::vec3 direction;
};

/// The way a strand grows out of the surface: straight out, against the inward normal, or, for a turned strand, turned
/// from that by x, y and z angles, combined y, x then z, in the frame of the bones that move its triangle. frame's
/// columns are that frame's axes, which needn't be at right angles: the direction is taken into the frame by its
/// inverse, turned there, and taken back out. For a frame that is nearly flat, the inverse keeps the determinant at least
/// 1e-10 from zero, as the game does.
[[nodiscard]] glm::vec3 GrowthDirection(const glm::vec3& inwardNormal);
[[nodiscard]] glm::vec3 GrowthDirection(const glm::vec3& inwardNormal, const glm::mat3& frame, const glm::vec3& angles);

/// The frame a turned strand is turned in, from the rotations of the bones that move its triangle's vertices: each
/// bone's axes made unit length one by one, added up, and made unit length again, by the 3D engine's inverse square
/// root (an axis of no length stays zero). The axes are not set at right angles
/// to each other, so the frame can be skewed.
[[nodiscard]] glm::mat3 BoneFrame(std::span<const glm::mat3> bones);

/// A strand's colour drawn in a light, each channel 0 to 255: its own colour scaled by the light's, then the colour
/// added to the light (the land's colour and the haze) on top, at most white. Strands aren't shaded by the sun.
[[nodiscard]] glm::ivec3 StrandColour(const glm::ivec3& colour, const glm::ivec3& light, const glm::ivec3& added);

/// A strand's points, its root first, and their speeds in world units a second
struct Strand
{
	std::vector<glm::vec3> positions;
	std::vector<glm::vec3> velocities;
};

/// A new strand of segmentCount points, as vanilla starts it: every point and every speed is zero, because the game
/// never clears the memory it reads them into. The first step puts the root on the body and each point after it a
/// segment from the one before, towards the world origin.
[[nodiscard]] Strand FromZero(uint32_t segmentCount);

/// The strand some seconds on. Its root follows the body; each point after it is pulled towards lying straight on from
/// the segment before it, falls, loses speed and moves, then is put back the segment's length from the point before
/// it; the speeds become how far each point really moved.
void Step(Strand& strand, const Root& root, const Physics& physics, float seconds);

/// The share of its speed that a point of hair keeps over a step of some seconds
[[nodiscard]] float KeptSpeed(const Physics& physics, float seconds);

/// The same step with the share of speed kept over the step given, so that the strands of one group find it one time
void Step(Strand& strand, const Root& root, const Physics& physics, float seconds, float kept);

/// The longest step that hair takes at once, in seconds
constexpr float k_MaxStepSeconds = 1.0f;

/// The seconds that hair moves for a frame of some milliseconds: none for a time that runs back, at most the longest
/// step
[[nodiscard]] constexpr float StepSeconds(float milliseconds)
{
	const auto seconds = milliseconds / 1000.0f;
	return seconds < 0.0f ? 0.0f : (seconds > k_MaxStepSeconds ? k_MaxStepSeconds : seconds);
}

/// A corner of a strand's ribbon
struct RibbonVertex
{
	glm::vec3 position;
	glm::vec2 uv;
};
/// How far across the hair texture a ribbon reaches, its length running the width of the texture
constexpr float k_RibbonTextureHeight = 0.25f;
/// The game draws all hair with one material. A texel of a strand with an alpha below this value, 0 to 255, is not
/// drawn. The other texels write their depth and blend over the scene by their alpha.
constexpr float k_AlphaCutOff = 50.0f;

/// A strand's ribbon facing an eye: two corners at each point, either side of the strand by halfWidth, across the
/// segment before the point (the first point takes the first segment's). Two corners for each of points into out.
void BuildRibbon(std::span<const glm::vec3> points, const glm::vec3& eye, float halfWidth, std::span<RibbonVertex> out);
} // namespace openblack::creature_hair
