/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <array>

#include <entt/fwd.hpp>
#include <glm/vec3.hpp>

#include "ECS/Components/FishFarm.h"

namespace openblack::ecs
{

/// What PuzzleGame type 14 keeps: the bait and the two shoals (here fish farm entities)
struct FishPuzzleParts
{
	entt::entity bait;
	std::array<entt::entity, 2> shoals;
};

/// PuzzleGame type 14 (the first time it is processed): the bait {pos, radius 11, need 30,
/// hold 500 ms} with its FishPlot net of radius 11, and two shoals of 15 fish (range 7) swimming for it at pos + (12, 0,
/// 12) and pos + (-12, 0, 12). The CHL side is ecs/PuzzleGames.h.
FishPuzzleParts CreateFishPuzzle(const glm::vec3& position);

/// The net's update, once per shoal swimming for the bait and frame (so twice for the puzzle): while closing the
/// radius goes 11 -> 1 (closure -= 2 dt), and phase += 2 dt
void AdvanceFishPlot(components::FishPlot& net, float seconds);

/// The 7 floats where the net's update and draw put them: y = p.y + 0.5 cos(i^2 + phase)
[[nodiscard]] std::array<glm::vec3, components::FishPlot::k_Floats> FishPlotFloats(const components::FishPlot& net);

/// Test hook OPENBLACK_TEST_FISH_PUZZLE="x,z[,inside]": the script's PuzzleGame 14 at (x, 0, z) (Land 4's PuzzlePos is
/// 2497.891, 3628.35), processed once so that the bait is there. With inside = 1 every fish starts at the bait, so the
/// net closes after 500 ms and PLAYED turns 1 on the next turn (ecs/PuzzleGames.h).
void RunFishPuzzleDebugHook();

} // namespace openblack::ecs
