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

#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

namespace openblack::ecs::missionary_boat
{

/// MissionaryBoat (only one boat at a time): the missionaries' boat of Land 1, made by the CHL
/// PLAY_JC_SPECIAL(6) in mode 0. Mode 0 is the launch: the ark mesh slides down the dry
/// dock along Data\MISC\Boat1.anm while five sailors push, with ScriptSfx creak / slide / splash and dust and spray;
/// near the end of the clip it turns into mode 1, the crossing: the hull rocks on Boat2.anm and sails from
/// (1456.54, 0, 3263.06) at 5 units a second along -(1, 0, 1)/sqrt 2 for 60 s with the cow, the grain pile and five
/// people on deck and a wake of five flat smoke sprites. docs/bw1-notes/water.md "The missionaries' boat".

/// Makes the boat in `mode`; an existing boat is freed first
void Create(int32_t mode);

/// One frame (milliseconds of game time, whole as in the original): PreDraw (from the landscape's draw: the clip, the
/// hull's matrix, the reflection) then PostDraw (sounds, dust, the hull, the people and the wake)
void Update(float gameMilliseconds);

/// The hull while a boat exists (null otherwise): lit by the land light with no haze
[[nodiscard]] entt::entity GetHull();

/// The hull while a boat exists, drawn into the reflection by PreDraw in k_ReflectionColour
[[nodiscard]] entt::entity GetReflectedHull();
constexpr uint32_t k_ReflectionColour = 0xFF303070u;

/// A sprite of the mode 1 wake (flat on XZ, smoke material, mode 6)
struct WakeSprite
{
	glm::vec3 position;
	float half;
	float aspect; ///< the z half is half x aspect
	float angle;  ///< the turn about Y
	uint8_t cell; ///< the smoke cell, 0..63
	uint32_t argb;
};
/// The wake sprites drawn this frame
[[nodiscard]] const std::vector<WakeSprite>& GetWake();

/// Test hook OPENBLACK_TEST_JC_SPECIAL="6[,mode[,frames[,fast forward ms]]]": PLAY_JC_SPECIAL(6) that many frames after
/// the landscape exists (mode 1 starts the crossing at once), then (test only) that much game time at once in steps of
/// 33 ms, for screenshots at a given moment. OPENBLACK_BOAT_TRACE=1 logs the boat every 500 ms.
void RunDebugHook();

} // namespace openblack::ecs::missionary_boat
