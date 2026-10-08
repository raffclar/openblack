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

#include <entt/entity/entity.hpp>
#include <glm/mat3x3.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "Enums.h"

/// The vortex objects (In / Out / Volcano; see docs/bw1-notes/vortex.md). The drawing and the
/// vortex's attract particle rule go through the engine's snapshot, the CHL handlers follow the intro's style. DRAFT: the
/// end-of-turn part (the fades' end, the land flattening) is in ProcessAll; the landscape draw's part (the decal, the
/// ground particles) is drawing, through the engine's snapshot.
namespace openblack::ecs::vortex
{
/// CHL CREATE Vortex (radius 50): In / Out / Volcano, then the creation setup (the info row, the state from the info,
/// the mesh scaled by the info's base scale, the particle systems, the volcano's sound). entt::null for another type.
entt::entity Create(glm::vec3 position, VortexType type);
/// State FadeIn (2), the turn
void StartFadeIn(entt::entity vortex);
/// VORTEX_FADE_OUT 257: state FadeOut (3), the turn
void StartFadeOut(entt::entity vortex);
/// VORTEX_PARAMETERS 328: the town and the flock parameters of an Out
void SetParameters(entt::entity vortex, entt::entity town, glm::vec3 position, float a, float b, entt::entity flock);
/// A thrown object that hits an active In and can be sucked into a vortex is taken in from the physics (its
/// velocity and world matrix)
void ReactToPhysicsImpact(entt::entity vortex, entt::entity hitter, glm::vec3 velocity, const glm::mat3& rows,
                          glm::vec3 position);
/// Take an object in (a creature only fizzes); queued for the attract particle rule
void TakeIn(entt::entity vortex, entt::entity object, glm::vec3 velocity, const glm::mat3& rows, glm::vec3 position,
            bool fromPhysics);
/// Once a game turn, at the end of the particle pass: the contents first (In / Out), then FadeIn -> Active and
/// FadeOut -> deleted after 7 s, then the land under an In / Out flattened to its mean
void ProcessAll(uint32_t turn);
/// The fade value f of a state after e seconds in it: 0 / 1; FadeIn 0 for 2 s then smooth((e - 2) / 5); FadeOut
/// smooth(1 - e / 5) for 5 s then 0; smooth(x) = ((3 - 2x) x) x
[[nodiscard]] float FadeValue(VortexStateType state, float e);
/// q = 1 - (1 - s)^2, s = FadeValue (1 in FadeOut)
[[nodiscard]] float LandFactorValue(VortexStateType state, float e);
/// With the retail spline (every y 0): b x q, b = mean - alt0 within 50 m, (56 - r)(mean - alt0) / 6 to 56 m, 0
/// beyond
[[nodiscard]] float LandOffset(glm::vec2 d, float alt0, float mean, float q);
/// Any vortex throwing this villager (the villager's thrown animation asks)
[[nodiscard]] bool IsVillagerBeingThrown(entt::entity villager);
/// On deletion (with the In's and Out's own parts): the VortexSave writer / reader, the thrown list, the particle systems
void OnDeleted(entt::entity vortex);
} // namespace openblack::ecs::vortex
