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

#include <entt/entity/fwd.hpp>
#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>

/// The physics and hand virtuals of the Living classes (Villager and Animal): what a villager or an animal does when it
/// starts flying, is hit, lands or is picked up. The physics (ECS/Physics/PhysicsObjects) and the hand call them
/// through their per-class hooks; the body, its G and its sinking stay with the physics.
namespace openblack::ecs::living
{

/// PhysicsObjects::SetClassHandlers for PhysicsClass::Villager and PhysicsClass::Animal (once, at start-up).
void RegisterPhysicsHandlers();

/// A villager or an animal put in the hand: marked as held, then the villager goes IN_HAND with its SCARED_STIFF clip and
/// the animal leaves its flock and goes IN_HAND. Nothing for an object that is not a Living.
void InterfaceSetInMagicHand(entt::entity entity);

/// A Living taken out of the hand: Villagers and Animals do what every object does: the object's fire effect, if any,
/// is told it left the hand (its reaction goes)
void InterfaceSetOutMagicHand(entt::entity entity);

/// The y angle kept while a Living is in the hand until its landing sets it again: the hand never changes it, and
/// openblack keeps no such angle of its own (ObjectYAngle)
struct HeldYAngle
{
	float value {0.0f};
};

// ---- the landing: the object's y angle, the bodiless EndPhysics and the pure functions (EndPhysics, the tests) ------

/// The object's y angle in radians as the original keeps it: written when the object's y angle or game angle is set
/// (villager::SetYAngle / animal_ai::SetYAngle; a game angle through its 3D conversion) and by SetXYZAngles, read when
/// the world matrix is built as AngleY(angle) (no pi / 2; the original's body is built from it). openblack keeps no
/// such angle: the Transform is the drawn rotation AngleY(angle + pi / 2) (DrawnRotation). Exact when that Transform is
/// still the one drawn from the game angle (villager WallHug::yAngle, set with it; the animal's from AnimalBrain::angle);
/// otherwise (inferred) YawFromRotation(OriginalRows(Transform)), to the rounding of the matrix's floats. 0 without a
/// Transform
[[nodiscard]] float ObjectYAngle(entt::entity entity);

/// The landType without a physics body
inline constexpr uint16_t k_LandTypeWithoutBody = 3;
/// A Living from the hand with no body built lands at once. The villager: no pose, no empathy, no SetYAngle, still
/// marked as held; then the object's landing, landType 3, altitude 0, the water branch (alive: DROWNING, its timer
/// 0; dead: VillagerDead 6), the land death (VillagerDead 5, no player) or LANDED. The animal: no SetYAngle, landType
/// 3, the object's landing, the life / state work. The same code as the body's EndPhysics from there on. The Transform
/// is redrawn from ObjectYAngle (the hand's tilt goes) and grounded. Nothing for an entity that is not a Living. For
/// physics::from_hand::PlaceWithoutBody: the object goes back on the map here (BackInMap), so the caller does not
/// insert a Living itself
void EndPhysicsWithoutBody(entt::entity entity);

/// The rows right / up / fwd (as glm columns 0 / 1 / 2) of the ORIGINAL's object matrix for openblack's DRAWN rotation
/// (the Transform's, with its pi / 2). The bodies are built with the original's rows, so this is only for a drawn
/// matrix: ObjectYAngle's fallback (YawFromRotation of the Transform).
/// The original builds the body from the world matrix = affine::AngleY(y angle) (no pi / 2 in it), but draws a
/// villager / animal at the y angle + pi / 2; openblack's Transform is the DRAWN rotation
/// (animal_ai::detail::FaceAngle: affine::AngleY(theta + pi / 2), as PathfindingSystem's InitializeStep) and the
/// body is built from it (PhysicsObjects BuildShape(transform.rotation)).
/// So body = M x K, with K the turn by pi / 2 about the object's own up axis: body column 0 = M's fwd row, column 1 =
/// M's up row, column 2 = -(M's right row). It follows any rigid motion of the body (the rows turn together), so for
/// the turn-start and the current matrix alike: right = -body[2], up = body[1], fwd = body[0]. (inferred from the two
/// readings above; not verified in game: the landing trace prints both candidates, `landing:` lines)
[[nodiscard]] glm::mat3 OriginalRows(const glm::mat3& body);
/// The original's rows of a physics body: from the hand (FromHand) the original builds the body from the held
/// object's DRAWN matrix, as openblack does (HandHolding: the hand's Transform); the world path from the world matrix,
/// which openblack builds as the original (WorldMatrixRows(ObjectYAngle, S)). Both are the rows as they are:
/// `fromHand` is kept for the callers' reading
[[nodiscard]] glm::mat3 BodyRows(const glm::mat3& body, bool fromHand);

/// The creature desires EndPhysics's empathy asks for
inline constexpr uint8_t k_DesireCompassion = 1;
inline constexpr uint8_t k_DesireAnger = 2;

/// The villager's landing pose, from the original's turn-start rows: a = right.y. a < -0.5 or NaN -> 1, yaw =
/// GetYAngle(up) stored as it is (no pi, no wrap), ANGER 0.5; a > 0.5 -> 2, yaw = WrapAngle(float(GetYAngle(up) + pi)),
/// ANGER 0.5; else 0, yaw = WrapAngle(float(GetYAngle(fwd) + pi)), COMPASSION 0.1
struct VillagerLandingPose
{
	uint16_t landType;
	float yaw; ///< the 3D angle EndPhysics gives SetYAngle (the game angle keeps it)
	uint8_t desire;
	float amount;
};
[[nodiscard]] VillagerLandingPose VillagerLandingPoseOf(const glm::mat3& turnStartRows);

/// The animal's landType from the original's turn-start right.y: a > 0.5 -> 1; a < -0.5 or NaN -> 2; else 0. The
/// villager's mapping reversed (not shared code in the original)
[[nodiscard]] uint16_t AnimalLandType(float rightY);
/// The animal's landing yaw: WrapAngle(float(GetYAngle(the CURRENT fwd row) + pi))
[[nodiscard]] float AnimalLandingYaw(const glm::vec3& currentFwdRow);
/// The yaw of the original's rows: float(ArcTanOctant(fwd.z, -fwd.x)) (as map_cells' YAngleOf). The physics takes it
/// before the class's EndPhysics and sets the angles again from it after: the y angle the landed object is drawn at
/// TODO(affine::DecomposeYXZ): one shared copy once affine has DecomposeYXZ (also map_cells' YAngleOf)
[[nodiscard]] float YawFromRotation(const glm::mat3& rows);
/// The drawn rotation of a y angle: affine::AngleY(yAngle + pi / 2) (FaceAngle's)
[[nodiscard]] glm::mat3 DrawnRotation(float yAngle);

} // namespace openblack::ecs::living
