/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

// Helpers shared by the HandSystem translation units:
//   HandSystem.cpp      input state machine, animation, public interface
//   HandPlacement.cpp   hand geometry, placement, cursor pick (ObtainRequiredHandPosition)
//   HandHolding.cpp     pick up, hold poses, spring, drop / throw
//   HandResources.cpp   piles, pots, multi pick-up, put down, stores
//   HandTrees.cpp       tug, uproot, roots, replant, dead trees
//   HandEffects.cpp     grip dust, multi pick-up particles
//   HandFish.cpp        splash of gripping the water, catching fish
//   HandPhysics.cpp     trees, pots and stores in the physics system (ECS/Physics)
//   HandDebugHooks.cpp  environment-variable test hooks

#include <cstdint>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

#include "Audio/Audio.h"
#include "Audio/Device/Sound.h"

namespace openblack::ecs::systems::hand_detail
{
/// A sound effect (NULL, sample, 3, 0, 0, 0, bank), a 2D one-shot without owner: the hand's failure (FailApply:
/// G_SpellCastFailure). The sample is a "<bank>.sad/<n>" SoundId.
void PlaySample(audio::SoundId id);
/// A sound effect from play options with is3D 1, track 0, no owner, at a world point (the options form of the hand's
/// 3D sites, e.g. G_HandInWater when gripping the water): not started beyond the sample's max
/// distance from the camera. Returns the channel as an entity (entt::null when it did not start).
entt::entity PlaySample3D(audio::SoundId id, glm::vec3 point);
/// MapCoords::IsLand: the landscape cell under the point does not have the water bit.
bool IsLand(glm::vec3 point);
/// OPENBLACK_DUMP_ENTITY_COUNTS (HandDebugHooks.cpp): entity counts per kind, once.
void DumpEntityCounts();

/// One turn of the multi pick-up from a pile: t = clamp(pickTurns / rampTicks, 0, 1) (<= 0 or NaN gives 0) and the
/// amount (perTurn + (perTurnEnd - perTurn) x t^2, truncated toward zero), before the source and room limits
struct PickUpStep
{
	float t;
	uint32_t amount;
};
[[nodiscard]] PickUpStep PickUpAmount(uint32_t perTurn, uint32_t perTurnEnd, float rampTicks, uint32_t pickTurns);
} // namespace openblack::ecs::systems::hand_detail
