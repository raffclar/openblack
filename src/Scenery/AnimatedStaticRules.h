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
#include <optional>
#include <span>
#include <vector>

#include "3D/AllMeshes.h"
#include "Enums.h"

// The animated scenery the land scripts place: the Norse gate of the creatures' glade, the gate stone plinth beside it,
// the piper's cave entrance and the phone box. A script opens or closes one; while it is drawn its model plays its clip
// on towards a resting place near the clip's end when open, and back to the start when closed. The plinth takes the
// three gate stones, stacked on its top, and sinks them as it opens. Pure functions, tested on their own.

namespace openblack::animated_static
{

/// The script's word for an open thing; anything else counts as closed for the clip, though only 0 is truly closed
constexpr int32_t k_Open = 1;
constexpr int32_t k_Closed = 0;
/// How many gate stones a plinth holds
constexpr size_t k_GateStoneSlots = 3;

/// The models of the gate stones laid in a plinth, in the order they were laid, each slot empty until filled
using GateStones = std::array<std::optional<MeshId>, k_GateStoneSlots>;

/// Where an open thing's clip comes to rest: two keyframes short of its last, in the clip's milliseconds
[[nodiscard]] uint32_t OpenRestingPlace(uint32_t playTime, size_t frameCount);

/// The clip's place after a frame of so many milliseconds of the game's clock: open, it plays on to its resting place
/// and stays there; otherwise it plays back to the start and stays there
[[nodiscard]] uint32_t StepClip(int32_t openState, uint32_t place, uint32_t elapsed, uint32_t restingPlace);

/// Whether it is still opening or closing: not resting at the start while closed nor at its resting place while open.
/// A closed thing resting at the open place counts as moving, as does an open one at the start.
[[nodiscard]] bool IsMoving(int32_t openState, uint32_t place, uint32_t restingPlace);

/// A thing dropped from the hand counts as a gate stone when its static kind is the gate stones' shared kind: the ape,
/// cow and tiger stones are, the uncarved rock is not
[[nodiscard]] bool IsGateStoneKind(MobileStaticInfo kind);

/// Lays a stone in the first empty slot; a full plinth takes nothing
[[nodiscard]] bool AddGateStone(GateStones& stones, MeshId stone);

/// What a plinth's stones are worth to the scripts: the ape stone 1, the tiger stone 2 and the cow stone 4, anything
/// else nothing, so 7 is all three
[[nodiscard]] uint32_t GateStoneValue(const GateStones& stones);

/// The model a thing collides with: the gate's open or closed model; the plinth's bare model while open or empty, its
/// one-stone model with a single stone and its two-stone model with more; the cave entrance's and the phone box's own;
/// none for the chess pieces
[[nodiscard]] std::optional<MeshId> CollisionModel(AnimatedStaticInfo type, int32_t openState, const GateStones& stones);

/// The stones drawn on a plinth this frame, each by its slot, lifted above the plinth's origin. While the plinth is
/// moving the stones sink with its clip; while it is closed they stand stacked on its top and the cursor can find
/// them, as the plinth itself. A plinth that is closed and still moving draws both. An open plinth at rest hides them.
struct StoneDraw
{
	size_t slot {0};
	float lift {0.0f};
	bool pickable {false};
};
struct PlinthLook
{
	int32_t openState {k_Closed};
	bool moving {false};
	uint32_t place {0};
	uint32_t playTime {0};
	/// Half the plinth model's height
	float plinthHalfHeight {0.0f};
	/// Half the height of each slot's stone model; none for an empty slot
	std::array<std::optional<float>, k_GateStoneSlots> stoneHalfHeights {};
};
[[nodiscard]] std::vector<StoneDraw> PlinthStoneDraws(const PlinthLook& look);

} // namespace openblack::animated_static
