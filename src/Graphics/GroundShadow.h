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
#include <optional>
#include <span>

#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

namespace openblack::graphics
{

/// The shadow of a villager a script draws in high detail, as the opening's family is, laid on the land as the game
/// lays it.
///
/// The body is flattened onto the level of its feet along the rays from the game's fixed far sun, so every part of it
/// below its feet is taken to be at their level. The flattened silhouette fills a 32 by 32 texture spanning exactly
/// its own extent across x and z. Each texel counts how many of its 4 by 2 coverage samples the silhouette covers, up
/// to 8 of the 15 levels of a 4 bit alpha, scaled down and rounded down by the fade with the camera's distance. The
/// outermost ring of texels is always clear, and only the triangles facing one way are counted unless their material
/// is drawn from both sides.
///
/// The texture is laid on the land as if all the land around stood at the height of the ground beneath the villager,
/// so it falls straight down onto slopes rather than along the sun. It darkens what is drawn there, its light, the
/// small bump detail and the haze, with black, by the texture's filtered alpha, fading out towards the water's edge
/// with the land's corners below altitude 2. Land nearer the sun than the nearest point of the body across the ground
/// takes none, and neither does a land block beyond the silhouette's extent. Nothing else receives it.
struct GroundShadow
{
	/// The game's fixed sun, which these shadows are cast from whatever the sky's light
	static constexpr glm::vec3 k_Sun {-500000.0f, 500000.0f, -500000.0f};
	static constexpr uint8_t k_Texels = 32;
	static constexpr uint8_t k_SamplesAcross = 4;
	static constexpr uint8_t k_SamplesDown = 2;
	static constexpr uint16_t k_SampleColumns = k_Texels * k_SamplesAcross;
	static constexpr uint16_t k_SampleRows = k_Texels * k_SamplesDown;
	/// The levels of the texture's alpha, of which a fully covered texel reaches k_SamplesAcross * k_SamplesDown
	static constexpr uint8_t k_Levels = 15;
	/// The most of them drawn at once, the nearest the camera
	static constexpr uint8_t k_MaxShadows = 4;
	/// Camera distance from the ground beneath the villager, in its radii, beyond which the shadow starts fading and
	/// where it has gone
	static constexpr float k_FadeStart = 50.0f;
	static constexpr float k_FadeEnd = 80.0f;
	/// The side of a land block, which the land is shaded by a block at a time
	static constexpr float k_BlockSize = 160.0f;

	/// Where the flattened silhouette lies
	struct Footprint
	{
		/// Its extent across x and z, on the level of the feet
		glm::vec2 low;
		glm::vec2 high;
		/// How far the body's nearest point reaches along the sun's way across the ground (see Reach)
		float nearest;
	};

	/// The shadow's opacity, 0 to 255, at a camera distance from the ground beneath a villager of a radius
	[[nodiscard]] static uint8_t Alpha(float cameraDistance, float radius);
	/// Where a point of the body lies once flattened onto the level of the feet, across x and z
	[[nodiscard]] static glm::vec2 OnGround(const glm::vec3& point, float feet);
	/// How far a point reaches along the sun's way across the ground to where the villager stands, in units of that
	/// way's own length
	[[nodiscard]] static float Reach(const glm::vec3& point, const glm::vec3& standsAt);
	/// The footprint of a body's points, with its feet at standsAt; none without points or when it has no extent
	[[nodiscard]] static std::optional<Footprint> Measure(std::span<const glm::vec3> points, const glm::vec3& standsAt);
	/// A flattened point on the grid of coverage samples: across, clamped into the grid, and down, clamped and rounded
	/// down to a whole row
	[[nodiscard]] static glm::vec2 Sample(const glm::vec2& onGround, const Footprint& footprint);
	/// Whether a triangle of three sample points faces the way that is counted when its material isn't two sided
	[[nodiscard]] static bool Counted(const std::array<glm::vec2, 3>& corners);
	/// A texel's level of alpha, 0 to k_Levels, from how many of its samples are covered and the shadow's opacity
	[[nodiscard]] static uint8_t Level(uint8_t covered, uint8_t alpha);
	/// Whether a texel is in the ring around the texture that is always clear
	[[nodiscard]] static bool OnBorder(uint8_t column, uint8_t row);
	/// Where a vertex at a sample point goes in clip space, in slot `slot` of a target `slots` slots wide, a slot being
	/// k_SampleColumns by k_SampleRows texels, so that the texels it covers are the samples the game counts.
	/// originBottomLeft is that of bgfx::Caps.
	[[nodiscard]] static glm::vec2 SlotClip(const glm::vec2& sample, uint8_t slot, uint8_t slots, bool originBottomLeft);
	/// Takes a point of the land to where it falls on the texture, 0 to 1 across and down, in x and y, ignoring its
	/// height, and how far it reaches along the sun's way across the ground in z. ground is the height of the land
	/// beneath the villager.
	[[nodiscard]] static glm::mat4 ReceiverMatrix(const Footprint& footprint, const glm::vec3& standsAt, float ground);
};

} // namespace openblack::graphics
