/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <entt/fwd.hpp>
#include <glm/fwd.hpp>

#include "Enums.h"

namespace openblack
{
struct GCitadelHeartInfo;
}

// The citadel heart: the script's finished temple (CREATE_CITADEL), the town's plan of one (CREATE_PLANNED_CITADEL, a
// planned heart in Town::plannedAbodes) and its conversion into a temple under construction, the temple's 3D side (the
// land flattening, the CitadelEntrance) and the entrance's tap.
// The building side (BuildBy / Built, the CitadelBuildingSite) is in ecs::abodes / ecs::building_sites, the per-turn
// part in worship::citadel::Process.

namespace openblack::ecs::archetypes
{
class CitadelArchetype
{
public:
	/// CREATE_CITADEL: CreateHeart(pos, angle, scale 1.0, life 1.0, built): a built temple (the script's size is
	/// ignored), and its worship sites opened as life >= 1
	static entt::entity Create(const glm::vec3& position, PlayerNames playerOwner, const glm::mat4& rotation,
	                           const glm::vec3& size);
	/// CREATE_PLANNED_CITADEL: only a plan at the tail of the town's list; not drawn, not in the map cells, no land
	/// flattening, no creation index. `heartInfo` is the script's N3 (the heart info record)
	static void CreatePlan(entt::entity town, const glm::vec3& position, uint32_t heartInfo, float yAngle, float scale);
	/// Null unless the heart info's mesh is suitable for a fixed object at the plan's position, angle and scale
	/// (town_placement::IsSuitableForFixed); else CreatePlannedNoFixedCheck
	static entt::entity CreatePlanned(entt::entity town, size_t plan, float life);
	/// The town's player (none: null, the plan survives); its citadel (made with the heart when it has none);
	/// CreateHeart(..., life, under construction) at 0 %; the heart's town = the plan's town; a rebuild plan's heart is
	/// marked not repaired; no new-building check (the plan has no town of its own); the plan deleted. Returns the heart
	/// or null
	static entt::entity CreatePlannedNoFixedCheck(entt::entity town, size_t plan, float life);
	/// The heart, its 3D side (mesh, land flattening, entrance, map cells) and, at life >= 1, the citadel's worship
	/// sites opened. `citadel` null: the player's citadel is made with this heart (openblack keeps the citadel on its
	/// first heart's entity, components::CitadelWorship)
	static entt::entity CreateHeart(const glm::vec3& position, PlayerNames owner, entt::entity citadel, float yAngle,
	                                float scale, float life, bool underConstruction);
	/// The land flattening's blend for one land cell: the cell (dx, dz) from the heart's of altitude `altitude`, the
	/// heart's cell `centreAltitude`: truncate(a x t + (1 - t) x A) & 0xFF, in float
	[[nodiscard]] static uint16_t FlattenedAltitude(int dx, int dz, int altitude, int centreAltitude);
	/// The heart info of the script's N3. (approximate) openblack keeps the one record (N3 = 0, every shipped land);
	/// another N3 reads past it into the pens' records in the original
	[[nodiscard]] static const GCitadelHeartInfo& HeartInfo(uint32_t heartInfo);
	/// The entrance's valid-to-tap and tap handlers in ecs::hand_tap (Register<CitadelEntrance>). Once (later calls do
	/// nothing); CreateHeart calls it
	static void RegisterTapHandlers();
	CitadelArchetype() = delete;
};
} // namespace openblack::ecs::archetypes
