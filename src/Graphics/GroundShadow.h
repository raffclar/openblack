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

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

namespace openblack::graphics
{

/// The moving shadows the game lays on the land: the creatures', the hand's and those of the villagers a script draws
/// in high detail, as the opening's family is.
///
/// Each body is flattened onto a level, along the rays from a point of light: every point of it is moved to where the
/// ray from the light through it, with its height taken above the level, meets the level. The flattened silhouette
/// fills a 32 by 32 texture spanning exactly its own extent across x and z. Each texel counts how many of its 4 by 2
/// coverage samples the silhouette covers, up to 8 of the 15 levels of a 4 bit alpha, scaled down and rounded down by
/// the fade with the camera's distance. The outermost ring of texels is always clear, and only the triangles facing one
/// way are counted unless their material is drawn from both sides.
///
/// The texture is laid on the land as if all the land around stood at one height: the ground beneath a villager, and
/// for a creature or the hand the level itself, so it falls straight down onto slopes rather than along the light. It
/// darkens what is drawn there, its light, the small bump detail and the haze, with black, by the texture's filtered
/// alpha, fading out towards the water's edge with the land's corners below altitude 2. Land nearer the light than the
/// nearest point of the body across the ground takes none, and neither does a land block beyond the silhouette's
/// extent. The creatures' and the hand's shadows also fall on the objects standing on the land, straight down and
/// without the water's fade, but never on the body casting them.
struct GroundShadow
{
	/// The game's fixed far sun, which the villagers' shadows are cast from whatever the sky's light
	static constexpr glm::vec3 k_Sun {-500000.0f, 500000.0f, -500000.0f};
	static constexpr uint8_t k_Texels = 32;
	static constexpr uint8_t k_SamplesAcross = 4;
	static constexpr uint8_t k_SamplesDown = 2;
	static constexpr uint16_t k_SampleColumns = k_Texels * k_SamplesAcross;
	static constexpr uint16_t k_SampleRows = k_Texels * k_SamplesDown;
	/// The levels of the texture's alpha, of which a fully covered texel reaches k_SamplesAcross * k_SamplesDown
	static constexpr uint8_t k_Levels = 15;
	/// Camera distance from the ground beneath the body, in its radii, beyond which the shadow starts fading and where
	/// it has gone
	static constexpr float k_FadeStart = 50.0f;
	static constexpr float k_FadeEnd = 80.0f;
	/// The side of a land block, which the land is shaded by a block at a time
	static constexpr float k_BlockSize = 160.0f;
	/// A creature's light is pulled in to no closer across the ground than this many of its radii
	static constexpr float k_CreatureLightReach = 3.0f;
	/// A creature's light this close round it across the ground is moved aside first, so it has a side to be on
	static constexpr double k_CreatureLightNudge = 0.1;
	/// The texels a shadow's data takes in the data the land and objects read (see ground_shadow.sh): four of where it
	/// lies, then its texture's levels, four a texel
	static constexpr uint16_t k_DataTexels = 4 + (k_Texels * k_Texels / 4);

	/// Where a body's shadow is cast from and laid
	struct Cast
	{
		/// The point the body is lit from
		glm::vec3 light;
		/// The level the body is flattened onto
		float level;
		/// The height the land is taken to stand at as the shadow is laid on it
		float ground;
	};

	/// A point of a body
	struct Point
	{
		glm::vec3 position;
		/// Kept from dipping below the level: true for the parts of a body that move with it whole, false for those
		/// moved by its bones
		bool keptAboveLevel;
	};

	/// The coverage samples of a shadow's texture as its triangles are counted into it, and where the last triangle's
	/// edges started and ended each row, which the game keeps from one triangle to the next
	struct Coverage
	{
		/// A row of samples, a bit each from the left, 64 to a word
		struct Row
		{
			std::array<uint64_t, k_SampleColumns / 64> words {};

			[[nodiscard]] bool test(size_t column) const { return ((words.at(column / 64) >> (column % 64)) & 1U) != 0; }
			[[nodiscard]] size_t count() const;
			/// Covers the samples from `from` up to but not including `to`
			void Fill(uint32_t from, uint32_t to);
			/// Covers every sample
			void set() { words.fill(~uint64_t {0}); }
		};
		std::array<Row, k_SampleRows> rows {};
		std::array<int32_t, k_SampleRows> starts {};
		std::array<int32_t, k_SampleRows> ends {};
	};

	/// Where the flattened silhouette lies
	struct Footprint
	{
		/// Its extent across x and z, on the level
		glm::vec2 low;
		glm::vec2 high;
		/// How far the body's nearest point reaches along the light's way across the ground (see Reach)
		float nearest;
	};

	/// The fade, 0 to 255, at a camera distance from the ground beneath a body of a radius
	[[nodiscard]] static float Fade(float cameraDistance, float radius);
	/// A villager's shadow's opacity, 0 to 255: the fade, rounded down
	[[nodiscard]] static uint8_t Alpha(float cameraDistance, float radius);
	/// A creature's or the hand's shadow's opacity, 0 to 255: its own full opacity, 255, scaled by the fade in 255ths
	[[nodiscard]] static uint8_t ScaledAlpha(float fade);
	/// Where a creature's shadow is cast from: the scene's light, moved out across the ground to at least
	/// k_CreatureLightReach of its radii from where it stands and brought round to at least 45 degrees above it
	[[nodiscard]] static glm::vec3 CreatureLight(const glm::vec3& sceneLight, const glm::vec3& standsAt, float radius);
	/// Where a point of a body lies once flattened onto the level, across x and z. The light's height is taken as it is,
	/// the point's above the level, as the game takes them.
	[[nodiscard]] static glm::vec2 OnLevel(const Point& point, const Cast& cast);
	/// The light's way across the ground to where a body stands, of length 1
	[[nodiscard]] static glm::vec2 LightWay(const glm::vec3& standsAt, const glm::vec3& light);
	/// How far a point reaches along the light's way across the ground to where the body stands, in units of that way's
	/// own length
	[[nodiscard]] static float Reach(const glm::vec3& point, const glm::vec3& standsAt, const glm::vec3& light);
	/// The footprint of a body's points, standing at standsAt; none without points or when it has no extent
	[[nodiscard]] static std::optional<Footprint> Measure(std::span<const Point> points, const glm::vec3& standsAt,
	                                                      const Cast& cast);
	/// As Measure, also leaving each point where it lies once flattened (OnLevel) in `flattened`, as many as the points
	[[nodiscard]] static std::optional<Footprint> Measure(std::span<const Point> points, const glm::vec3& standsAt,
	                                                      const Cast& cast, std::span<glm::vec2> flattened);
	/// A flattened point on the grid of coverage samples: across, clamped into the grid, and down, clamped and rounded
	/// down to a whole row
	[[nodiscard]] static glm::vec2 Sample(const glm::vec2& onLevel, const Footprint& footprint);
	/// Whether a triangle of three sample points faces the way that is counted when its material isn't two sided
	[[nodiscard]] static bool Counted(const std::array<glm::vec2, 3>& corners);
	/// Counts a triangle of three sample points into the coverage, as the game does: unless its material is drawn from
	/// both sides, only if it faces the counted way, then each row it spans, taken at the rows its corners are rounded
	/// down to, from where its left edges cross the row to where its right edges do, stepped in 16.16 fixed point.
	/// secondRowsOnly leaves the first row of each texel's pair of sample rows out, as for the hand itself.
	static void Cover(Coverage& coverage, const std::array<glm::vec2, 3>& corners, bool twoSided, bool secondRowsOnly);
	/// The levels of a shadow's texture, row by row: how many of each texel's samples are covered, cut back by the
	/// shadow's opacity, with the ring around it clear
	[[nodiscard]] static std::array<uint8_t, k_Texels * k_Texels> Levels(const Coverage& coverage, uint8_t alpha);
	/// A texel's level of alpha, 0 to k_Levels, from how many of its samples are covered and the shadow's opacity
	[[nodiscard]] static uint8_t Level(uint8_t covered, uint8_t alpha);
	/// Whether a texel is in the ring around the texture that is always clear
	[[nodiscard]] static bool OnBorder(uint8_t column, uint8_t row);
	/// Takes a point of the land to where it falls on the texture, 0 to 1 across and down, ignoring its height:
	/// u = x * a + b and v = z * c + d, as (a, b, c, d)
	[[nodiscard]] static glm::vec4 Receiver(const Footprint& footprint, const Cast& cast);
	/// Whether a shadow's extent reaches the land block with its lowest corner at `block`, which is shaded by it
	[[nodiscard]] static bool ReachesBlock(const Footprint& footprint, const glm::vec2& block);
};

} // namespace openblack::graphics
