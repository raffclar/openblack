/*******************************************************************************
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

/// The physics side of the objects a particle system carries (the storm's tornado and the vortex): the take and
/// release of a carried object and the vortex's throw. Wiki: docs/bw1-notes/vortex.md. The PSys rules that call them
/// (UR_Tornado, UR_VortexAttract) are unowned files with only the call hunks ("physics (unowned)").
namespace openblack::ecs::physics::particle_carried_objects
{
/// The object starts physics with no velocity, spin (0, 1, 0), no thrower and no body: refused (false) when the
/// object is IN_PHYSICS or IMMOVABLE; else IN_PHYSICS, out of the map cells, its fire StartedMoving(0), no body; then
/// it is marked carried, the CarriedByParticleSystem component here. Returns whether it was taken.
bool Take(entt::entity object);
/// An object no longer available is only forgotten (the caller's guard); else its angles come from the atom's
/// matrix, its position is the atom's (x, z in fixed point, truncated), altitude 0, its physics ends with no body
/// (back in the map where it is, no flight and no impact) and the carried mark is cleared.
void Release(entt::entity object, const glm::mat3& atomRows, glm::vec3 atomPosition);
/// Carried by a particle system (between Take and Release)
[[nodiscard]] bool IsCarried(entt::entity object);
/// Availability as the particle systems ask it: not being deleted (ecs::IsAvailable) and,
/// for a villager, villager::IsAvailable (its final state is not DYING)
[[nodiscard]] bool IsAvailable(entt::entity object);

/// The vortex (the thrower) flings an object: h = 0.5 object::GetHeight, B = (GameFloatRand(5) + 8) speed,
/// C = (GameFloatRand(5) + 10) speed; v = (sin(angle) B, C, -cos(angle) B), w = ((0 - v.z) / h, 0, (v.x - 0) / h)
/// (1 / h first, then each product); the object starts physics with v, w, the thrower and a body.
void Fling(entt::entity object, entt::entity thrower, float angle, float speed);
/// Fling's velocity and body-axes spin for a half height h and the two GameFloatRand(5.0) draws (B's, then C's)
struct FlingMotion
{
	glm::vec3 velocity;
	glm::vec3 spin;
};
[[nodiscard]] FlingMotion FlingMotionOf(float h, float angle, float speed, float randB, float randC);
} // namespace openblack::ecs::physics::particle_carried_objects
