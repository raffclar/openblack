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

#include <optional>

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

namespace openblack::graphics
{

/// A creature's shadow, after the game's dynamic shadows: the creature's silhouette, seen from the scene's light, in a
/// soft 32 by 32 texture projected onto the land and what stands on it. The light is kept at least 45 degrees up, and
/// never closer than three of the creature's radii. The shadow fades out as the camera pulls away, between 50 and 80
/// of the creature's radii from the ground beneath it. The hair casts none.
///
/// A villager a script draws in high detail, as the opening's family is, casts the same kind of shadow while it is
/// drawn so (in place of its foot blobs), but from the game's fixed far sun, and only onto the land.
struct CreatureShadow
{
	/// What casts a moving shadow
	enum class Caster : uint8_t
	{
		Creature,
		HighDetailVillager,
	};
	/// The game's fixed sun, a point so far off that every shadow cast from it falls the same way
	static constexpr glm::vec3 k_FixedSun {-500000.0f, 500000.0f, -500000.0f};
	/// The shadows are drawn side by side in one texture, each in a cell of this many texels, twice the game's 32 so
	/// that four filtered samples across a texel soften the edges as its box filter does. A texel of the game's border
	/// is left clear.
	static constexpr uint16_t k_CellSize = 64;
	static constexpr uint16_t k_CellBorder = 2;
	/// The most creatures' shadows drawn at once, the nearest the camera
	static constexpr uint8_t k_MaxShadows = 8;
	/// Camera distance from the ground beneath the creature, in its radii, beyond which the shadow starts fading and
	/// where it has gone
	static constexpr float k_FadeStart = 50.0f;
	static constexpr float k_FadeEnd = 80.0f;
	/// The light is pulled in to no closer than this many of the creature's radii
	static constexpr float k_LightDistance = 3.0f;
	/// A light this close round the creature is moved aside first, so it has a side to be on
	static constexpr float k_LightNudge = 0.2f;

	/// The shadow's opacity, 0 to 255, at a camera distance from the ground beneath a creature of a radius
	[[nodiscard]] static uint8_t Alpha(float cameraDistance, float radius);
	/// Where the shadow is cast from: the scene's light, brought round to at least 45 degrees above the creature and,
	/// when it is nearer than k_LightDistance radii across the ground, out to that distance
	[[nodiscard]] static glm::vec3 LightPoint(const glm::vec3& light, const glm::vec3& centre, float radius);
	/// Where a caster's shadow is cast from: a creature's from the scene's light as LightPoint brings it round, a
	/// high-detail villager's from the fixed sun whatever the scene's light
	[[nodiscard]] static glm::vec3 CastFrom(Caster caster, const glm::vec3& light, const glm::vec3& centre, float radius);
	/// Whether a caster's shadow falls on the objects standing on the land as well as on the land itself
	[[nodiscard]] static bool FallsOnObjects(Caster caster);

	/// View and projection of the silhouette, fitted to the creature's bounding sphere, looking from the light
	glm::mat4 view;
	glm::mat4 projection;
	/// World position to texture coordinates in the cell in xy, and distance past the creature's centre along the light
	/// in z
	glm::mat4 receiverMatrix;
	/// The shadow's opacity, 0 to 1
	float strength;
	/// Receivers this far along the light past the creature's centre or further are shaded
	float startDepth;
	/// Whether it falls on the objects standing on the land too, or only on the land
	bool onObjects;

	/// The shadow of a caster with a bounding sphere about centre, cast from where CastFrom says with the scene's light,
	/// seen from the camera, in cell `cell` of a texture `cells` wide. originBottomLeft and homogeneousDepth are those of
	/// bgfx::Caps. None when the shadow has faded out.
	[[nodiscard]] static std::optional<CreatureShadow> Compute(Caster caster, const glm::vec3& centre, float radius,
	                                                           float groundHeight, const glm::vec3& light,
	                                                           const glm::vec3& camera, uint8_t cell, uint8_t cells,
	                                                           bool originBottomLeft, bool homogeneousDepth);
	/// The matrix that takes the silhouette's clip space into its cell's part of the shared texture's
	[[nodiscard]] static glm::mat4 CellMatrix(uint8_t cell, uint8_t cells);
};

} // namespace openblack::graphics
