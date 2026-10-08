/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <entt/entity/entity.hpp>
#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>

#include "Enums.h"

namespace openblack::ecs::components
{

/// OneOffSpellSeed (a mobile object): the one-shot miracle orb on the
/// land, mesh .\data\spells\meshes\O_Bibble_up.l3d with a 4x4 animated texture. Tapping it puts a fully charged seed
/// in the hand.
struct OneOffSpellSeed
{
	SpellSeedType seedType {SpellSeedType::None};
	float scale {1.0f};                ///< the multiplier passed to the seed
	entt::entity graphic {entt::null}; ///< the seed's graphic inside
	float phase {0.0f};                ///< the texture frame: 0..16, 18 frames a second
	int powerUp {-1};
	/// The drawing turns the orb every frame: the mesh's +Y (the dome) points at the camera,
	/// about the centre of the mesh's box. Only the drawing uses it (the object's position stays; RenderingSystem):
	/// drawn = facing * vertex + Transform::position + facingOffset
	glm::mat3 facing {1.0f};
	glm::vec3 facingOffset {0.0f};
	/// The point the orb is sorted at among the blended objects (its matrix at AddForDrawing): the box
	/// centre pushed toward the camera by its radius, so the seed inside is drawn before the bubble
	glm::vec3 sortPoint {0.0f};
};

} // namespace openblack::ecs::components
