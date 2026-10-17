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
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

/// EXPERIMENT (not for merge): the jellyfish in the deep water. They are not in the original game. They drift slowly up
/// through the deep water in a loose grid round the eye, one at most in each cell, and a new one starts near the sea bed
/// when one gets near the surface. Each is one instance of a small mesh: a bell (an outer and an inner shell) with
/// thick inner tentacles and thin outer ones. The shaders vs_underwater_jelly.sc and fs_underwater_jelly.sc make the
/// beat of the bell, the sway of the tentacles and the glow.
namespace openblack::graphics::underwater::jellyfish
{

/// The side of a cell of the grid, in units: one jellyfish at most in each cell
inline constexpr float k_Cell = 12.0f;
/// How far from the eye they are, in units; they fade out over the last part of it
inline constexpr float k_Range = 170.0f;
/// The share of the range over which they fade out
inline constexpr float k_FadeShare = 0.3f;
/// The least depth of the sea bed where they are, in units: never in the shallows where the creatures walk
inline constexpr float k_LeastDepth = 7.0f;
/// The depth over which they come in past the least depth
inline constexpr float k_DepthFade = 5.0f;
/// The share of the deep cells that have one
inline constexpr float k_Share = 0.55f;
/// How fast they rise, in units a second
inline constexpr float k_Rise = 0.22f;
/// The space they keep from the sea bed and from the surface, in units
inline constexpr float k_BedGap = 1.0f;
inline constexpr float k_SurfaceGap = 1.8f;
/// The size of the bell across, the least and the most, in units
inline constexpr float k_LeastSize = 0.5f;
inline constexpr float k_MostSize = 1.4f;
/// The most that are drawn
inline constexpr uint32_t k_Most = 320;

/// The two passes of the shader, as the w of the eye's uniform: the bodies in the scene, and their own light after the
/// water's pass
inline constexpr float k_BodyPass = 0.0f;
inline constexpr float k_GlowPass = 1.0f;
/// Their own light after the water's pass. The water takes it out over this share of the way only, and the light keeps
/// most of its colour: only this share of the change of colour by the water stays, the rest is a grey loss.
inline constexpr float k_GlowPathShare = 0.5f;
inline constexpr float k_GlowTint = 0.15f;

/// The parts of the mesh: the outer shell of the bell, its inner shell, the inner tentacles and the outer tentacles
enum class Part : uint8_t
{
	OuterShell,
	InnerShell,
	InnerTentacle,
	OuterTentacle,
};
inline constexpr uint32_t k_InnerTentacles = 6;
inline constexpr uint32_t k_OuterTentacles = 13;
/// Rings and sides of each shell, and lengths along each tentacle
inline constexpr uint32_t k_ShellRings = 7;
inline constexpr uint32_t k_ShellSides = 18;
inline constexpr uint32_t k_TentacleSegments = 10;

/// One jellyfish this frame
struct Jellyfish
{
	/// The top of its bell
	glm::vec3 position {0.0f};
	/// The bell's size across
	float size {1.0f};
	/// The phase of its beat, 0 to 1
	float phase {0.0f};
	/// Its colour, 0 pink to 1 orange
	float hue {0.0f};
	/// How much it shows: it fades in near the sea bed, out near the surface, and out at the far edge of the range
	float shows {1.0f};
	/// A random number of its own, 0 to 1
	float seed {0.0f};
};

/// The depth of the sea bed below the sea at a point (positive under the sea)
using DepthAt = std::function<float(glm::vec2 point)>;

/// The jellyfish round the eye at a time: the cells in range whose sea bed is deep enough each have one or none by a
/// hash of the cell, at a place in the cell and a height that rises with the time. The nearest come first.
[[nodiscard]] std::vector<Jellyfish> Around(glm::vec2 eye, float seconds, float seaLevel, const DepthAt& depthAt);

/// The two vectors of a jellyfish's instance: xyz the top of the bell and w its size; x the phase, y the colour, z how
/// much it shows and w its seed
[[nodiscard]] std::array<glm::vec4, 2> InstanceData(const Jellyfish& jellyfish);

/// The length of the way from the eye to a point that goes through the water: all of it with the eye under the water,
/// and the part under the sea's level with the eye above it
[[nodiscard]] float WaterPath(glm::vec3 eye, glm::vec3 point, float seaLevel);

/// The part of each colour of a jellyfish's own light that comes to the eye through a length of water. It fades with
/// the distance as the water's light does, but more slowly, and it keeps its colour longer: pink stays pink far away.
[[nodiscard]] glm::vec3 GlowThroughWater(float path);

/// A vertex of the mesh: x along (round the shell, or down the tentacle, 0 to 1), y across (down the shell 0 to 1, or
/// the side of the tentacle, -1 or 1), z the part, w which tentacle
struct Vertex
{
	glm::vec4 data {0.0f};
};
/// The mesh of one jellyfish, with the inner shell and the tentacles first and the outer shell last, so that the outer
/// shell glows over the rest
void Mesh(std::vector<Vertex>& vertices, std::vector<uint16_t>& indices);

} // namespace openblack::graphics::underwater::jellyfish
