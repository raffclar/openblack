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
#include <glm/vec3.hpp>

// The beam explosion's particle side: UR_Explosion (the blast of SF_BeamExplosion*, MAGIC_TYPE 7-9), SetPSysCloseDown,
// and the rules of its spot visual SF_BeamExplosionFX (UR_MoveAtom, UR_ChangeScaleXYZ). Plus the two object questions
// the blast asks of its targets. Wiki: docs/bw1-notes/miracles.md, beam explosion.

namespace openblack::psys::explosion
{
/// Whether a spell can destroy the object: it receives effects, an object flag is clear, and an object in a script
/// only for a spell flagged for it. Creatures, fields and the citadel (its heart and parts, the creature pen, worship
/// sites and totems) say no. Answers SpellEvent 7.
[[nodiscard]] bool CanBeDestroyedBySpell(entt::entity object, entt::entity spell);

/// An object destroyed by the beam is deleted. Every abode kind (creche, field, football, graveyard, puzzle totem,
/// spell dispenser, storage pit, totem, town centre, windmill, wonder, workshop) instead loses all its life.
void DestroyedByBeam(entt::entity object);

/// The point a blast throws things away from, 5 m under its centre (the origin of the pieces of the exploded objects
/// and of the five rocks: Rules/ExplodeObject.h)
[[nodiscard]] inline glm::vec3 BlastOrigin(const glm::vec3& centre)
{
	return {centre.x, centre.y - 5.0f, centre.z};
}

/// UR_ChangeScaleXYZ on one atom's values: false when the atom is before StartTime or the
/// step after StopTime has passed (nothing written). ruleScale = the XZ lerp, stretch = Y / XZ (0 when XZ <= 0.0001).
bool ChangeScaleXYZ(float age, float dt, float startTime, float stopTime, float startXZ, float stopXZ, float startY,
                    float stopY, float& ruleScale, float& stretch);

/// UR_MoveAtom on one atom: false outside [StartTime, StopTime]; t = (age - start) /
/// (stop - start), 1 when age + dt reaches StopTime, smoothstep t^2 (3 - 2t) with MoveSmoothly; start + (stop - start) t
bool MoveAtom(float age, float dt, float startTime, float stopTime, bool smoothly, const glm::vec3& start,
              const glm::vec3& stop, glm::vec3& out);
} // namespace openblack::psys::explosion
