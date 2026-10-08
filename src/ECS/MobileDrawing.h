/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <entt/entity/fwd.hpp>
#include <glm/mat4x4.hpp>

namespace openblack::ecs
{
class Registry;

/// How the original draws villagers and animals between game turns (docs/bw1-notes/animation.md). The simulation moves
/// them once per turn (10 per second); every rendered frame a moving one is drawn between its position at the start
/// and the end of the last turn by the turn's fraction (one turn behind, the two land heights lerped), a villager's
/// drawn yaw turns towards the real one (3 rad/s, faster past 90 degrees), and it is sheared along the slope while it
/// stands on the land.

/// The start of one living's turn: its position into DrawPosition::turnStart (assigned if it has none yet), right
/// before its reactions are processed. For the one living loop
void BeginLivingTurn(entt::entity entity);

/// Every frame: the drawn positions, with `turnFraction` (0..0.99) and the frame's game milliseconds
void UpdateMobileDrawing(float turnFraction, float milliseconds);

/// The model matrix a mobile object is drawn with this frame (CarriedProps' grip, the SuperVillagers' on-screen test and
/// swim rings; through DrawnBodyModel RenderingSystem's instance matrix and the eyes): in the physics its pose between
/// its last two turns (PhysicsDrawPose, no slope shear), else a creature's pose between its turns
/// (creature_pose::BetweenTurns, when it differs from the Transform), else its DrawPosition (between turns, turning),
/// else its Transform; T(p) R S with the transform's scale, or a creature's drawn scale in its pen (affine::Model);
/// then, with a DrawPosition and slopeShear, the slope shear (row 1 added to rows 0 and 2). slopeShear false: without
/// it (a tree's roots, RenderingSystem.cpp; DrawnPosition). The shadow blobs use the shear too (villagers and animals).
/// A creature's body, eyes, hair roots, footprints, what it holds and the hand's touch all take this one matrix
/// (through DrawnBodyModel). Needs a Transform
[[nodiscard]] glm::mat4 DrawnModel(const Registry& registry, entt::entity entity, bool slopeShear = true);

/// Where the object is drawn this frame: DrawnModel's translation (the same sources in the same order, which the shear
/// does not move). For the drawing's readers of the position of an object that may be in the hand, not for the logic
[[nodiscard]] glm::vec3 DrawnPosition(const Registry& registry, entt::entity entity);

/// The matrix the body is drawn with: DrawnModel, then a SuperVillager's own turn (ECS/SuperVillager.h): a copy of the
/// sheared object matrix turned about its own Y by DrawPosition::followDrawnTurn (affine::RotateY, rows 0 and 2).
/// DrawnModel itself for every other object (the turn is 0). RenderingSystem's instance matrix and the SuperVillagers'
/// eyes
[[nodiscard]] glm::mat4 DrawnBodyModel(const Registry& registry, entt::entity entity);

/// Every position at the start of the turn the entity has (a villager's or animal's DrawPosition, a shark's own)
/// becomes its Transform's position
void SnapTurnStart(Registry& registry, entt::entity entity);

/// A thing was put somewhere outside its own turn (a script's SET_POSITION, the hand, the end of physics): its
/// positions at the start of the turn are snapped (SnapTurnStart), so it is drawn at its new place from the next frame
/// with no slide, and events::Teleported is published
void NotifyTeleported(entt::entity entity);

/// A SuperVillager's yaw (DrawPosition::follow*) once a frame: followYaw = Wrap(it); equal to Wrap(target) or snap: it
/// takes the target, no turn; else it steps (ms x 0.001) x radiansPerSecond towards it (Wrap of the difference), taking
/// it when the difference is not more than a step (no turn). Returns the turn of the body's drawn copy about its own Y
/// (followYaw - Wrap(target), affine::RotateY; DrawPosition::followDrawnTurn), 0 for none
float StepYawFollow(float& followYaw, float target, float milliseconds, float radiansPerSecond, bool snap);

} // namespace openblack::ecs
