/*******************************************************************************
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

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

namespace openblack::ecs::physics
{
struct PhysicsObject;

/// The collision sounds of the physics (looked up in the editor.sad animation table through PlayAnimationEffect), plus
/// the ground dust and the water splash of a landing.
class CollisionSounds
{
public:
	/// SOUND_COLLISION_TYPE of an object (info collideSound; a Fragment is always BUSH, DeadTree mesh 406 HOLLOW_WOOD).
	[[nodiscard]] static int TypeOf(entt::entity entity);
	/// AttemptToAddSoundEvent for a body at the end of a turn (its impact and what hit it are set).
	static void AttemptToAddSoundEvent(const PhysicsObject& po);
	/// audio::PlayAnimationEffect on editor.sad as the physics calls it (collisions, building hits and destruction):
	/// the distance is the camera's to `at`, the row of editor.sad's table that fits the 5 columns picks the sample, and
	/// the channel belongs to the object (so the .sad play mode applies per object: G_BigSplash's mode 2 plays nothing
	/// while the same object's splash plays) at its point, following it every turn with `track`.
	static void PlayAnimEffect(const std::array<int32_t, 5>& key, entt::entity owner, glm::vec3 at, bool track);
	/// The pair list ages one turn (pairs stay listed for the turn they were added and the next).
	static void EndTurn();
	CollisionSounds() = delete;
};
} // namespace openblack::ecs::physics
