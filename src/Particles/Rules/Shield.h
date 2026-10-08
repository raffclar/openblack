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

#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

// The particle side of the shields: the defensive shield registry that projectile particles test
// (UR_AddDefensiveSphere puts the magic shield's sphere in it), the deflection helpers the fireball's
// UpdateRuleGravityWithFloor calls (DoAnyShieldDeflections), and the rules of SF_DefenseSphere / SF_PhysicalShieldFX
// (Shield.cpp: UpdateRuleShieldSpark, UR_InitialSpin, UR_VapourEndEffect, SetCollectionAlpha, UR_AtomsAtEPTarget,
// CheckShieldDeflections). Wiki: docs/bw1-notes/magic.md, shields.

namespace openblack::psys
{
class Effect;
struct Atom;
} // namespace openblack::psys

namespace openblack::psys::shields
{

/// A defensive shield, owned by an effect, and its only kind, a sphere {radius, centre}
struct DefensiveSphere
{
	uint32_t id {0};
	const Effect* owner {nullptr};
	glm::vec3 centre {0.0f}; ///< world
	float radius {0.0f};
};

/// A new sphere at the head of the list; its id
uint32_t AddDefensiveSphere(const Effect& owner, const glm::vec3& centre, float radius);
/// Unlink and delete (when UR_AddDefensiveSphere's collection data goes)
void RemoveDefensiveSphere(uint32_t id);
/// Every sphere of an effect goes with it (the collection data are deleted with the collections)
void RemoveAllOf(const Effect* owner);
/// nullptr when gone
[[nodiscard]] DefensiveSphere* Find(uint32_t id);
/// The list, newest first
[[nodiscard]] const std::vector<DefensiveSphere>& All();

// ---- Sphere tests ----

/// |p - c|^2 < (r + margin)^2
[[nodiscard]] bool IsPointInShield(const DefensiveSphere& sphere, const glm::vec3& point, float margin);
/// `to` inside and `from` not (both with the margin)
[[nodiscard]] bool HasCrossedIntoShield(const DefensiveSphere& sphere, const glm::vec3& from, const glm::vec3& to,
                                        float margin);
/// Where the segment from -> to meets the sphere of
/// radius r + margin (out = `to` when nothing better; false when the segment misses it)
bool FindIntersect(const DefensiveSphere& sphere, const glm::vec3& from, const glm::vec3& to, float margin, glm::vec3& out);
/// Reflection off the sphere: v -= 2 (v.n) n, n = normalize(p - c)
void DeflectOffShield(const DefensiveSphere& sphere, const glm::vec3& point, glm::vec3& velocity);

/// The first sphere of the list that contains the point
[[nodiscard]] const DefensiveSphere* FindShieldContainingPoint(const glm::vec3& point, float margin);
/// The first sphere the move from -> to crossed into
[[nodiscard]] const DefensiveSphere* FindShieldCrossedInto(const glm::vec3& from, const glm::vec3& to, float margin);
/// An impact point on the shield's effect (its SpellTargets), where UpdateRuleShieldSpark makes a spark
void AddImpactTarget(const DefensiveSphere& sphere, const glm::vec3& point);
/// The Spell of the shield's effect, entt::null without one
[[nodiscard]] entt::entity SpellOf(const DefensiveSphere& sphere);

/// For the rules with CheckShieldDeflections (only the
/// fireball throws): after the atom moved from `oldGlobal`, the first shield it crossed into gets a spark at the
/// intersection and a SpellEvent 4 {hit, the move, 1, target = the shield's spell} goes to the atom's own spell. If the
/// spell answers 0 the atom is put at the hit point and its velocity reflected (true); an answer of 1 lets it through.
/// margin = 1.25 x baseScale x ruleScale.
bool DoAnyShieldDeflections(Effect& effect, Atom& atom, const glm::vec3& oldGlobal);

} // namespace openblack::psys::shields
