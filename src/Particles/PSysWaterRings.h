/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <glm/vec3.hpp>

/// The water rings that particle effects leave on the sea (the ring pool of ECS/WaterRings). Called by
/// UR_Explosion (Rules/Explosion.cpp) and UpdateRuleGravityWithFloor (Rules/Fireball.cpp).
namespace openblack::psys::water_rings
{

inline constexpr float k_ExplosionRingGrowth = 10.0f;
inline constexpr float k_ExplosionScorchSize = 8.0f; ///< the scorch sprite (id 0x251) on dry land

/// When an explosion starts: dry land at the point (altitude >= 4) -> false, and the caller puts the scorch sprite
/// 0x251 of size 8 at a random angle in 0..2 pi, no ring.
/// Otherwise (water or altitude < 4) three rings at the point, growth 10 x 0.5, x 0.7 and x 1, cell 0x30, colour
/// 0xFFFFFFFF, angle 0, rate and aspect 1; each is skipped when the 1024 slots are full. Returns true then.
bool AddExplosionRings(const glm::vec3& point);

/// UpdateRuleGravityWithFloor's ripple of a particle that touches the floor, once the rule's own tests passed (its
/// alpha threshold, an ImpactSound, the speed test, ImpactSoundCondition, the ripple flag on): only on water, and only
/// when the particle is farther than the rule's minDistance in x, z from its last ripple, which is then moved there
/// (lastRipple: kept per atom, (0, 0, 0) when it is made). The ring: at the particle, growth 4 x the atom's radius,
/// cell 0x30, colour 0xFFFFFFFF, angle 0, rate and aspect 1 (nothing when the pool is full).
/// Returns true when a ring was added.
bool AddParticleRipple(const glm::vec3& position, float atomRadius, glm::vec3& lastRipple, float minDistance);

} // namespace openblack::psys::water_rings
