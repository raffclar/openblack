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

#include <entt/entity/entity.hpp>
#include <entt/fwd.hpp>
#include <glm/vec3.hpp>

#include "3D/AxisAlignedBoundingBox.h"
#include "3D/MapCoords.h"
#include "Enums.h"

namespace openblack
{
struct GPotInfo;
} // namespace openblack

/// The object size family: Get2DRadius, GetRadius, GetHeight, GetScale and the routines built on them. All of them read
/// the mesh's bounding box as filled at load: the half extents (max - min) x 0.5 of the union of the sub-meshes and the
/// half diagonal sqrt((hz^2 + hy^2) + hx^2).
///
/// Two levels, because the original has both and they disagree on purpose:
/// - The mesh level (MeshHalfExtents, MeshRadius2D, MeshHalfHeight, MeshHalfDiagonal): the fields read inline from the
///   object's mesh, with no class override (fixed-object placement, scaffolds, field and tree drawing, physics
///   setup...). A Field there is its mesh, not 5 m.
/// - The object level (GetScale, Get2DRadius, GetRadius, GetHeight and the derived ones): the virtual calls, with the
///   class overrides: Field and FishFarm = 5, food piles (food, magic food, puzzle grain) = x GetProportionRaised,
///   MagicTeleport = 6, magic fireballs and creatures their own. Trees, rocks, abodes, villagers, animals, the citadel
///   heart and the wood piles override none of the four. Each derived routine lists its own overrides.
/// - Not covered: the things that are not objects (the citadel, planned buildings, spell seeds, shields, storms, towns,
///   arenas, building sites, street lights, mist...), which have their own radii or none. None of them is asked here:
///   openblack's temple is the CitadelHeart, which is an object (pending if one of them ever is).
///
/// Port each site to the level the original uses there. The game logic runs with the FPU at 24 bits, so everything is
/// float, no double and no FMA.
namespace openblack::ecs::object
{

/// The Get2DRadius and mesh radius of a field or fish farm (no mesh, no scale)
constexpr float k_FieldRadius = 5.0f;
/// A magic teleport's Get2DRadius
constexpr float k_MagicTeleportRadius = 6.0f;
/// A magic fireball's Get2DRadius is GetScale x this
constexpr float k_MagicFireBallRadius = 1.0f;
/// A creature's height is the body's user size x 15
constexpr float k_CreatureHeightPerScale = 15.0f;
/// The floor GetProportionRaised gives a non-empty pile (food and wood)
constexpr float k_ProportionFloor = 0.05f;
/// The hold radius of an ABOVE hold, x GetHeight
constexpr float k_HoldAboveHeightFactor = 0.75f;
/// The hold radius of a tree or dead tree, x Get2DRadius
constexpr float k_TreeHoldFactor = 0.2f;
/// A dead tree's default fire radius, x GetHeight
constexpr float k_DeadTreeFireFactor = 0.35f;
/// A worship site's real radius (also its default fire radius)
constexpr float k_WorshipSiteRadius = 14.0f;
/// The villager hug radius is Get2DRadius x 1.05 + 0.0005
constexpr float k_HugRadiusFactor = 1.05f;
constexpr float k_HugRadiusMargin = 0.0005f;
/// A tree's villager hug radius and route plan radius are min(Get2DRadius x 0.1, 0.25)
constexpr float k_TreeHugFactor = 0.1f;
constexpr float k_TreeHugMax = 0.25f;
/// The citadel heart's route plan radius is Get2DRadius x 0.33
constexpr float k_CitadelHeartRoutePlanFactor = 0.33f;
/// A worship site's centre is its local point (12.55, 0, -26.1) through its matrix
constexpr float k_WorshipSiteCentreRight = 12.55f;
constexpr float k_WorshipSiteCentreBack = 26.1f;

// ---- Mesh level (no override) -------------------------------------------------------------------------------------

/// The half extents of a box: (max - min) x 0.5
[[nodiscard]] glm::vec3 HalfExtents(const AxisAlignedBoundingBox& box);
/// The half diagonal of the half extents: sqrt((hz hz + hy hy) + hx hx), in that order
[[nodiscard]] float HalfDiagonal(glm::vec3 half);
/// s x max(hx, hz) (hx when hz < hx, else hz; one product)
[[nodiscard]] float Radius2D(glm::vec3 half, float scale);
/// 2 x (hy x s): the product first, then doubled
[[nodiscard]] float Height(glm::vec3 half, float scale);

/// The half extents of a loaded mesh; nothing when the mesh is not loaded
[[nodiscard]] std::optional<glm::vec3> MeshHalfExtents(entt::id_type meshId);
/// The mesh's half diagonal, no scale; 0 without the mesh
[[nodiscard]] float MeshHalfDiagonal(entt::id_type meshId);
/// The inline Get2DRadius: s x max(hx, hz); 0 without the mesh
[[nodiscard]] float MeshRadius2D(entt::id_type meshId, float scale);
/// The inline whole height 2 x (s x hy) (tree drawing); 0 without the mesh
[[nodiscard]] float MeshHeight(entt::id_type meshId, float scale);
/// The inline half height, no scale (field and tree drawing, physics setup); 0 without the mesh
[[nodiscard]] float MeshHalfHeight(entt::id_type meshId);

/// The mesh of an object (the Mesh component's id), when it has one and it is loaded
[[nodiscard]] std::optional<glm::vec3> ObjectHalfExtents(entt::entity object);

// ---- GetProportionRaised (piles) ----------------------------------------------------------------------------------

/// A food pile's raised proportion: p = amount (unsigned) / maxInPot; p < 0 -> 0 and no floor; p > 1 -> 1; p == 0
/// stays 0 (an empty pile is 0); else p = (1 - 0.05) p + 0.05. Then 1 - (1 - p)^2 clamped to 0..1
[[nodiscard]] float PileFoodProportionRaised(uint32_t amount, uint32_t maxInPot);
/// A wood pile's raised proportion: p = amount / maxInPot; p > 0 -> (1 - 0.05) p + 0.05; then clamped to 0..1 (no
/// square)
[[nodiscard]] float PileWoodProportionRaised(uint32_t amount, uint32_t maxInPot);
/// The pile's own one by its info's potType (PileFood / PileWood); 1 for a pot or anything else, which has no such
/// method (inferred: never asked)
[[nodiscard]] float GetProportionRaised(entt::entity pile);
/// The GPotInfo row of a pot or pile entity (its Pot::type), null for anything else or without the info constants
[[nodiscard]] const GPotInfo* PotInfoOf(entt::entity object);
/// Whether the object is a food pile (food, magic food or puzzle grain): the pots whose info potType is PileFood
[[nodiscard]] bool IsPileFood(entt::entity object);

// ---- Object level (the virtual calls, with the class overrides) -----------------------------------------------------

/// GetScale: the Transform's uniform scale (x); a map shield's object scale; a creature's user size (inferred: the
/// Transform's scale). 0 without a Transform
[[nodiscard]] float GetScale(entt::entity object);
/// The scale field the generic GetHeight reads directly, not the virtual GetScale
[[nodiscard]] float GetScaleField(entt::entity object);

/// The generic Get2DRadius itself (the non-virtual body a food pile calls): GetScale x max(hx, hz); 0 without a mesh
[[nodiscard]] float FootprintRadius(entt::entity object);
/// The generic GetHeight itself: 2 x hy x the scale field; 0 without a mesh
[[nodiscard]] float ObjectHeight(entt::entity object);

/// Get2DRadius with the class overrides: a field or fish farm 5; a magic teleport 6; a magic fireball GetScale x 1;
/// food piles GetProportionRaised x FootprintRadius; a creature's reads its body, not ported: the generic one stands
/// in (inferred); every other class FootprintRadius
[[nodiscard]] float Get2DRadius(entt::entity object);
/// GetRadius: the same as Get2DRadius (a creature's too)
[[nodiscard]] float GetRadius(entt::entity object);
/// GetHeight: a magic fireball's is its Get2DRadius; a creature's its user size x 15; every other class the generic
/// one (fields, fish farms and food piles keep it)
[[nodiscard]] float GetHeight(entt::entity object);
/// GetTopPos: the MapCoords altitude (above the ground) + GetHeight; 0 for a shield
[[nodiscard]] float GetTopPos(entt::entity object);
/// GetHeightForHandAboveInteractObject: GetHeight; a fish farm 5
[[nodiscard]] float GetHeightForHandAboveInteractObject(entt::entity object);
/// GetMeshRadius: the mesh's half diagonal, no scale; a field or fish farm 5
[[nodiscard]] float GetMeshRadius(entt::entity object);

// ---- Derived (built on the object level, each its own routine) ------------------------------------------------------

/// GetHoldRadius: a tree or dead tree Get2DRadius x 0.2; anything else GetHeight x 0.75 when the hold type is ABOVE,
/// else Get2DRadius. The hold type is the hand's (HandSystem::HoldType), so the caller says whether it is ABOVE. A
/// spell seed's (GetScale x a seed info value) needs the seed info: the caller's
[[nodiscard]] float GetHoldRadius(entt::entity object, bool holdTypeAbove);
/// GetDefaultFireRadius: Get2DRadius; a dead tree GetHeight x 0.35; a worship site 14
[[nodiscard]] float GetDefaultFireRadius(entt::entity object);
/// GetVillagerHugRadius: Get2DRadius x 1.05 + 0.0005; a tree min(Get2DRadius x 0.1, 0.25)
[[nodiscard]] float GetVillagerHugRadius(entt::entity object);
/// GetRoutePlanRadius with no creature: Get2DRadius; a tree min(Get2DRadius x 0.1, 0.25); the citadel heart
/// Get2DRadius x 0.33 (openblack: the Temple). The creature branch is not ported
[[nodiscard]] float GetRoutePlanRadius(entt::entity object);
/// The distance to another object: GetDistanceInMetres(a, b) - (R2D(b) + R2D(a)), the radii added first and the sum
/// subtracted from the distance; a worship site uses GetDistanceInMetres(its centre, b) - (14 + R2D(b))
[[nodiscard]] float GetDistanceFromObject(entt::entity object, entt::entity other);
/// The distance to a point: GetDistanceInMetres - GetRadius. No object class overrides it
[[nodiscard]] float GetDistanceFromObject(entt::entity object, glm::vec3 point);
/// Touching another object: GetDistanceFromObject(other) <= margin
[[nodiscard]] bool IsTouching(entt::entity object, entt::entity other, float margin);
/// Touching a point: GetDistanceFromObject(point) <= 0
[[nodiscard]] bool IsTouching(entt::entity object, glm::vec3 point);

struct BoundingSphere
{
	glm::vec3 centre;
	float radius;
};
/// The bounding sphere: h = GetHeight x 0.5, r = sqrt(R2D R2D + h h); the centre is the MapCoords' x, z and y =
/// (ground + altitude) + h. The ground is the island's (LandIsland::HeightAt, through map_coords::ToWorld). Villagers,
/// animals and mobile statics (rocks, dead and felled trees, bonfires, fragments, magic teleports) use R2D x 0.5. A
/// creature's own sphere is not ported: the generic one stands in (inferred)
[[nodiscard]] BoundingSphere GetBoundingSphere(entt::entity object);

/// A worship site's centre: the matrix's right x 12.55 - its forward x 26.1 + its position, per component in that
/// order (openblack keeps the point rather than a MapCoords). The matrix is the site's Transform (inferred, the same
/// reading as WorshipScore's)
[[nodiscard]] glm::vec3 WorshipSiteCentre(entt::entity site);

// ---- Points around an object (GUtils angles, Common/GUtilsAngle) -------------------------------------------------------
// Each one is this + GetPosFromAngle(an angle, a radius) through MapCoords addition, so the result keeps this object's
// altitude (GetPosFromAngle's is 0). Each has its own radius: do not swap one for another.

/// The object's MapCoords: map_coords::FromWorld of its Transform (the altitude above the island's ground); a zero
/// MapCoords without one
[[nodiscard]] map_coords::MapCoords MapCoordsOf(entt::entity object);
/// The nearest point by another object: this + GetPosFromAngle(angle to the other, R2D(other) + R2D(this))
[[nodiscard]] map_coords::MapCoords GetNearestPosOfObject(entt::entity object, entt::entity other);
/// The nearest edge towards a point: this + GetPosFromAngle(angle to p, R2D(this))
[[nodiscard]] map_coords::MapCoords GetNearestEdgeToPos(entt::entity object, const map_coords::MapCoords& pos);
/// The edge along an angle: this + GetPosFromAngle(angle, R2D(this) + extra). The angle is the caller's, not worked
/// out here
[[nodiscard]] map_coords::MapCoords GetNearestEdge(entt::entity object, float angle, float extra);
/// The working point: this + GetPosFromAngle(angle to the other, R(this) + R(other)), GetRadius of both, not
/// Get2DRadius
[[nodiscard]] map_coords::MapCoords GetWorkingPos(entt::entity object, entt::entity other);
/// A tree's working point: this + GetPosFromAngle(angle to the other, R2D(other) + 0.9): the other object's radius
/// only
[[nodiscard]] map_coords::MapCoords TreeGetWorkingPos(entt::entity tree, entt::entity other);
/// The reach added to the other object's radius for a tree's working point
constexpr float k_TreeWorkingReach = 0.9f;
/// A big forest's arrive point for a villager: this + GetPosFromAngle(angle to the villager, R(this) x 0.5), GetRadius
[[nodiscard]] map_coords::MapCoords BigForestGetArrivePos(entt::entity bigForest, entt::entity villager);

} // namespace openblack::ecs::object
