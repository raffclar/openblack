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
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

#include "3D/AllMeshes.h"
#include "Enums.h"

namespace openblack::ecs::physics
{
struct PhysicsObject;
}

// MapShield: the world object a shield spell makes. The magic shield is invisible (the dome is the spell's
// SF_DefenseSphere); the physical shield is the solid MSH_S_SOLID_SHIELD that grows, spins and bobs, a physics obstacle
// that pays chants per impact, with its SF_PhysicalShieldFX. Wiki: docs/bw1-notes/magic.md ("Shields").

namespace openblack::magic::map_shield
{

/// The mesh of both
constexpr MeshId k_Mesh = MeshId::SpellSolidShield;
/// The physical shield's process and draw constants
constexpr float k_ScalePerRadius = 0.017f;
constexpr float k_MaxStartSpin = 3.0f;
constexpr float k_HiddenTime = 0.5f;
constexpr float k_GrowTime = 1.5f;
constexpr float k_SpinDownTime = 6.0f;
constexpr float k_FadeTime = 1.5f;
constexpr float k_DieTimeFactor = 1.5f; ///< Deleted once dieTime > k_FadeTime x this (2.25 s)
constexpr float k_EndSpin = 0.15f;
constexpr float k_BobSpeed = 1.3f;
constexpr double k_RescaleDelta = static_cast<double>(0.3f); ///< 0.3f widened to a double (0.30000001192092896)
constexpr uint8_t k_MinAlpha = 40;

/// The process curves, t in seconds since the creation turn: the shrink over the first 0.5 s (1 - t / 0.5, the
/// shield is not drawn then), then the grow x + x^2 - x^3 (x = (t - 0.5) / 1.5) and the spin-down y + y^2 - y^3
/// (y = (t - 0.5) / 6), both 1 once past their end
struct Curves
{
	float grow;     ///< A
	float spinDown; ///< B (0 while t < 0.5: the spin does not move)
	bool spinning;  ///< t >= 0.5
};
[[nodiscard]] Curves CurvesAt(float seconds);

/// A magic shield (type 19) or a physical shield (type 20); entt::null for another type. `position` is MapCoords (y
/// above the land, 0).
entt::entity Create(const glm::vec3& position, entt::entity spell, float radius);

/// Each spell turn: processes every available shield
void ProcessShields();
/// Draws the shields every frame
void DrawShields();

/// The magic shield goes at once (3); the physical shield lets go of the spell and fades out by itself (1)
int SetDying(entt::entity shield);
/// The physical shield's FX, then the shield out of the list
void ToBeDeleted(entt::entity shield);

/// The magic shield is a sphere of the spell's radius, the physical shield a cone of its own 2D radius and height.
/// `point` is MapCoords (y above the land).
[[nodiscard]] bool IsPointDefinitelyWithinShieldVolume(entt::entity shield, const glm::vec3& point);
/// For reactions applied to the livings of a square: true when the living is under a shield (its distance below the
/// shield's 2D radius) that the reaction's source is not definitely inside: it ignores the reaction.
/// MapCoords (y above the land).
[[nodiscard]] bool IsReactionBlockedByShield(const glm::vec3& living, const glm::vec3& source);

/// Its spell's player
[[nodiscard]] bool GetPlayer(entt::entity shield, PlayerNames& player);
/// A creature that is not controlled by a script and whose player is not the shield's must keep out of it; anything
/// else, including no creature at all, is 0. openblack has no creature class, so its player comes as an argument
/// (std::nullopt = no player) and (pending) nobody asks yet: in the original the caller is the creature's path finding
[[nodiscard]] bool CreatureMustAvoid(entt::entity shield, entt::entity creature, std::optional<PlayerNames> creaturePlayer);

// ---- physics (ECS/Physics/PhysicsObjects.cpp asks) ----

/// The physical shield 1, the magic shield 0
[[nodiscard]] bool InteractsWithPhysicsObjects(entt::entity shield);
/// The physics constants type
constexpr int k_PhysicsConstantsType = 10;
/// The scale its body is built with (the object scale, which lags the drawn one by up to 0.3)
[[nodiscard]] float CollisionScale(entt::entity shield);
/// A hit by something that destroys abodes, while the spell has
/// strength, is a SpellEvent 5 and costs |v| x mass x chantCostPerImpactMomentum x 0.0001 (forced); then the struck or
/// destroyed reaction
void ReactToPhysicsImpact(entt::entity shield, const ecs::physics::PhysicsObject& po);
/// The physical shield receives effects with no burn (burn 0); the magic shield never does
[[nodiscard]] bool IsEffectReceiver(entt::entity shield, float burn);

/// The shields, newest first
[[nodiscard]] const std::vector<entt::entity>& Shields();
/// A land is loaded
void Clear();

} // namespace openblack::magic::map_shield
