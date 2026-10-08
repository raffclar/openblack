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

#include <optional>

#include "3D/AllMeshes.h"
#include "Enums.h"

/// What each kind of object is to the physics: whether thrown things hit it (it becomes a resting obstacle when
/// something flies near it) and whether it can itself be thrown, dropped or knocked into flight. Pure rules over the
/// facts the physics reads from the object, so they can be tested without a land.
namespace openblack::ecs::physics::class_rules
{

/// A building, or anything that stands like one, is hit while more than this share of it is built...
inline constexpr float k_LeastBuiltToHit = 0.1f;
/// ...and while it has more life than this
inline constexpr float k_LeastLifeToHit = 0.01f;
/// The temple's heart is hit while more than this share of it is built, compared at double precision
inline constexpr double k_LeastHeartBuiltToHit = 0.1;

/// The weight of the bodies of fixed things whose weight doesn't follow their size: buildings...
inline constexpr float k_BuildingMass = 2000.0f;
/// ...and the temple's heart, the gates and the chess pieces
inline constexpr float k_HeavyFixedMass = 10000.0f;

/// The rule for buildings and the things that stand like them: hit while built and standing (both strictly)
[[nodiscard]] bool StandingBuilding(float percentBuilt, float life);

/// Whether thrown things hit a building of a kind: the town centre always; the graveyard, the football pitch and fields
/// never; the rest (the town's totem and the spell dispensers too) while built and standing
[[nodiscard]] bool AbodeIsObstacle(AbodeNumber type, float percentBuilt, float life);

/// Whether thrown things hit the temple's heart
[[nodiscard]] bool CitadelHeartIsObstacle(float percentBuilt);

/// The statics as heavy as rocks: the gate totems, the weeping stones and the singing stone
[[nodiscard]] bool IsHeavyStatic(MobileStaticInfo type);
/// Whether a static's model is one of the hand's toys
[[nodiscard]] bool IsToyModel(MeshId mesh);
/// Whether a static's model is a fence
[[nodiscard]] bool IsFenceModel(MeshId mesh);
/// The statics that are plain objects to the physics: the lanterns, the singing stone's base and the bonfire. They
/// never fly and are made of the unmovable material
[[nodiscard]] bool IsPlainObjectStatic(MobileStaticInfo type);

/// Whether thrown things hit a static. Toys, rocks and the statics as heavy as rocks, fences and idols always; the
/// lanterns and the bonfire never; the singing stone's base always; anything else as a building is
[[nodiscard]] bool MobileStaticIsObstacle(MobileStaticInfo type, MobileStaticInfo mobileType, MeshId mesh, float percentBuilt,
                                          float life);
/// Whether a static can fly: all but the plain objects among them
[[nodiscard]] bool MobileStaticCanBecomePhysicsObject(MobileStaticInfo type);

/// Whether thrown things hit a mobile object: all but the creed
[[nodiscard]] bool MobileObjectIsObstacle(MobileObjectInfo type);
/// Whether a mobile object can fly: not the whale, the creed or the base of the tower puzzle. The puzzle's blocks fly
/// while they are not set in place, which nothing does yet
[[nodiscard]] bool MobileObjectCanBecomePhysicsObject(MobileObjectInfo type);

/// Whether thrown things hit an animated static: only the gates, the piper's cave and the phone box; never the chess
/// pieces
[[nodiscard]] bool AnimatedStaticIsObstacle(AnimatedStaticInfo type);
/// The model an animated static's body is made from, by its open and plinth states: the Norse gate's shut or open
/// model, the totem plinth's empty, half or full one (the phone box shares the empty plinth's), the piper's cave
/// entrance; none for anything else
[[nodiscard]] std::optional<MeshId> AnimatedStaticCollisionMesh(AnimatedStaticInfo type, int32_t openState, int32_t plinthState,
                                                                int32_t plinthFull);

} // namespace openblack::ecs::physics::class_rules
