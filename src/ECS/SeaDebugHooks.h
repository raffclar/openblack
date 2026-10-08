/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

namespace openblack::ecs
{

/// Environment-variable test hooks of the sea (docs/bw1-notes/openblack-internals.md), run once when the landscape
/// exists (from HandSystem::RunDebugHooks):
///
/// OPENBLACK_TEST_SEA="x,z,type[,height]": makes a villager (a Celtic farmer without a town), an animal (a cow), a tree
/// (a beech), a pot (a hand pot of 300 food) or a rock (a lime boulder of scale 0.5) at height (default 2) over the
/// ground at (x, z) and puts it in the physics with no velocity; type = villager | animal | tree | pot | rock. It also
/// logs GET_LAND_HEIGHT there and at the dry reference point (1788.4, 2710). With OPENBLACK_PHYSICS_TRACE the physics
/// logs each turn's centre, density and radius (the sinking); a drowning villager logs its counter every 100 turns.
/// With OPENBLACK_TEST_CUT set too, the object gets components::CutByPlane (its part under the water in dark blue).
///
/// OPENBLACK_TEST_FISH_PUZZLE="x,z[,inside]": the fish puzzle there (ecs/FishPuzzle.h, RunFishPuzzleDebugHook).
void RunSeaDebugHooks();

} // namespace openblack::ecs
