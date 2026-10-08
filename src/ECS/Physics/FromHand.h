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

#include <functional>
#include <optional>
#include <utility>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>

namespace openblack::ecs::physics::from_hand
{
/// What the hand owns that a throw needs (the hand's pots, trees and stores), set by the hand system.
struct HandHooks
{
	/// a hand pot (HandWood / HandFood) put down becomes a pile or goes into a store
	std::function<void(entt::entity)> putDownHandPot;
	/// the object is a hand pot (PotInfo HandWood / HandFood)
	std::function<bool(entt::entity)> isHandPot;
};
void SetHandHooks(HandHooks hooks);

/// Throws an object that is still in the hand (the hand lets go of it afterwards): a hand
/// pot slow enough is put down at once, else InitialisePhysicsFromHand with the holding spring's velocity. A release
/// passes dontReplant false, a forced drop true. Returns true when the object landed (put down, or LANDED out of the
/// hand) instead of flying.
/// angularMomentum: the release prediction's (from PredictRelease, this port's sign), set on the new body after
/// AddObject and the paper / disappear block; nullopt keeps AddObject's zero.
/// (pending) the original first sets the object's matrix from the release pose (the hand's angles and position):
/// the caller sets the Transform from that pose before Throw; HandHolding.cpp passes no pose and no L yet.
bool Throw(entt::entity object, glm::vec3 springVelocity, bool dontReplant,
           std::optional<glm::vec3> angularMomentum = std::nullopt);
/// Sent by the hand about 180 ms after a release, handDelta = the hand's displacement since: with a body,
/// |v| = sqrt((vz vz + vy vy) + vx vx) of its velocity, m = its mass; the external torque
/// += (-(((m dz) 1.6) |v|), 0, ((m dx) 1.6) |v|), in the original's sign (its
/// torque is F x r: stored negated here). ZeroForces copies it every substep and GameTurnUpdate zeroes it at the turn's end.
/// Nothing without a body.
void ApplyReleaseSpin(entt::entity object, glm::vec3 handDelta);
/// A forced drop: released with zero velocity, then thrown with dontReplant set
bool ForceDrop(entt::entity object);

/// AddObject, AdjustToGroundLevel, RaiseUntilNotIntersecting and the LANDED
/// rule; nullopt when no body could be made, else whether it landed. byCreature: a creature lets go of it, which
/// throws it at a lower speed (IsThrown)
std::optional<bool> InitialisePhysicsFromHand(entt::entity object, glm::vec3 velocity, bool dontReplant,
                                              std::optional<glm::vec3> angularMomentum = std::nullopt, bool byCreature = false);

/// Whether an object let go of at this velocity is thrown rather than put down: its speed across the ground squared
/// above 4 from the hand, above 1 from a creature
[[nodiscard]] bool IsThrown(glm::vec3 velocity, bool byCreature);

/// What the hand predicts at the release, sent with it.
struct ReleasePrediction
{
	glm::vec3 velocity {0.0f};        ///< the velocity after the turns
	glm::vec3 angularMomentum {0.0f}; ///< the angular momentum after the turns, this port's sign
	glm::vec3 origin {0.0f};          ///< the body's translation
	glm::mat3 rotation {1.0f};        ///< the body's rotation rows (glm's columns), for its yaw, pitch and roll
	/// the body's rotation and centre at the start of each turn and at the
	/// end, turns + 1 entries (for drawing the prediction)
	std::vector<std::pair<glm::mat3, glm::vec3>> history;
};
/// The release prediction, the physics part: a body that is not in the list, built by
/// the held object's physics setup at its Transform, L from the hand's angular
/// velocity, the hand's velocity capped at 124, AdjustToGroundLevel(1, !(tree || l2 > 1))
/// and min(turns, 15) turns of 20 substeps ZeroForces, GroundAndWater, ContactForces,
/// Integrate, its result ignored; no other body, no game logic. nullopt when no body could be built. turns is the
/// caller's min(ping / 100, 5).
/// The release velocity cap: l2 = (z z + y y) + x x of the hand's velocity; above 124 x 124
/// and not all three 0 it is scaled to 124 (k = 124 / sqrt(l2), in single precision); fast = the uncapped l2 > 1
struct CappedReleaseVelocity
{
	glm::vec3 velocity {0.0f};
	bool fast {false};
};
[[nodiscard]] CappedReleaseVelocity CapReleaseVelocity(glm::vec3 handVelocity);
[[nodiscard]] std::optional<ReleasePrediction> PredictRelease(entt::entity object, glm::vec3 handVelocity,
                                                              glm::vec3 handAngularVelocity, uint32_t turns);
/// (approximate) openblack's placement of an object that gets no physics body. In the original AddObject only
/// fails for a class that cannot become a physics object, an IMMOVABLE one or one already flying, and a body is always
/// built (the body setup reads the mesh unchecked); openblack also fails for an object with no mesh.
void PlaceWithoutBody(entt::entity object);

/// Whether the entity is a fence: its mesh is
/// BuildingAmericanFence or one of the Celtic fences.
[[nodiscard]] bool IsFence(entt::entity entity);
} // namespace openblack::ecs::physics::from_hand
