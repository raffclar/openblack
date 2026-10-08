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

#include <array>
#include <vector>

#include <entt/entity/entity.hpp>

namespace openblack::ecs::components
{

/// The workshop's state, on top of its Abode. One per workshop abode entity, assigned by ecs::workshops::Create (from
/// AbodeArchetype). ecs::workshops is its only writer.
/// Not kept: the "ghost scaffold" drawn on a taken-away slot ((not ported) render). The next in the town's list is
/// Town::workshops.
struct Workshop
{
	/// the slots at the mesh's special points 6, 7, 8
	static constexpr size_t k_Slots = 3;
	/// slot byte values
	static constexpr uint8_t k_SlotFree = 0;
	static constexpr uint8_t k_SlotTakenAway = 1; ///< the scaffold moved away: the ghost is drawn, the place is kept
	static constexpr uint8_t k_SlotOccupied = 2;

	/// game turns left for the scaffold being made (0 = idle): set from the info when a scaffold starts, counted down
	/// each update, cleared when it is done and by the constructor
	int32_t countdown {0};
	/// the scaffolds owned (AddScaffold ++, also for a null scaffold; RemoveScaffold --). Signed: GetSpaceInStore =
	/// 3 - (countdown != 0) - owned
	int32_t owned {0};
	/// the wood pile, a "Magic Wood" pot (pot info 9) at the special point 4 (CreatePileWood); null after the workshop
	/// is deleted or the pot removed from it
	entt::entity woodPile {entt::null};
	/// k_SlotFree / k_SlotTakenAway / k_SlotOccupied
	std::array<uint8_t, k_Slots> slots {};
	/// the owned scaffolds, the head first (AddScaffold pushes at the head, no duplicate test: a scaffold may appear
	/// twice; RemoveScaffold / the dependants' deletion take every node of it).
	/// The count is scaffolds.size()
	std::vector<entt::entity> scaffolds;
	/// the ShowNeedsVisuals (ecs::show_needs, show needs info row 3): fed by the update with the visual wood desire
	entt::entity showNeeds {entt::null};
};

} // namespace openblack::ecs::components
