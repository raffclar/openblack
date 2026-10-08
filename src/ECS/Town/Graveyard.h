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

// The town's graveyard and the town's link to it. Its dead count drives a 3-bit graves stage on the 3D object.

namespace openblack::ecs::components
{
/// The dead buried here (AddDead counts up while below 50; saved with the graveyard)
struct Graveyard
{
	uint32_t dead {0};
	/// The graves stage put on the 3D object (3 bits); (pending) what the draw shows for it
	uint8_t gravesStage {0};
};
} // namespace openblack::ecs::components

namespace openblack::ecs::graveyard
{
/// AddDead counts only while the dead are below this
constexpr float k_MaxDead = 50.0f;
/// Graves stage = truncated dead x this, at least 1 once someone is buried
constexpr float k_GravesPerDead = 0.18f;

/// The town's graveyard (entt::null when none)
[[nodiscard]] entt::entity GetGraveyard(entt::entity town);
/// Sets the town's graveyard only when it has none or `graveyard` is null: the first functional graveyard stays, a
/// null clears it
void SetGraveyard(entt::entity town, entt::entity graveyard);
/// With a town, functional, and fewer than 50 dead: one more dead, stage = truncated dead x 0.18 (at least 1 with
/// someone buried), stored in 3 bits (stage 8 shows as 0). Callers: MakeFunctional and the town's death book-keeping
void AddDead(entt::entity graveyard);
/// The abode's MakeFunctional first (the caller's), then with a town that has no graveyard SetGraveyard(this), then
/// AddDead (literal: one dead counted)
void MakeFunctional(entt::entity graveyard);
/// On deletion: with a town whose graveyard is this one, the town's abodes are searched for the first other functional
/// abode whose ABODE_TYPE has bit 2 or bit 9 (literal, so any civic building such as the storage pit passes, not only
/// a graveyard); SetGraveyard(null), SetGraveyard(found or null); then the abode's own DeleteDependants
void DeleteDependants(entt::entity graveyard);
} // namespace openblack::ecs::graveyard
