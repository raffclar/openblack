/*******************************************************************************
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

/// The sharks (class Whale; docs/bw1-notes/water.md "Sharks"). Created by the CHL CREATE of type
/// Whale (archetypes::SharkArchetype), moved by the script's WALK_PATH (not ported: the camera tracks Track%d are not
/// read yet), drawn in two parts cut by the water, with a wake of rings.

/// Every game turn: each shark's process (turnStart = Pos). Then the WALK_PATH list moves them (the global lists'
/// process, MoveAlongPath).
void ProcessSharksTurn();

/// The frame part of the sharks' draw (for the whole list from the landscape draw), after the animations: the heading
/// of the turn's move, the drawn position between the turn's start and end by `turnFraction`, and the wake (a ring
/// every 50 ms of a timer shared by all sharks at the EBone[0] point).
/// The clip time advances in UpdateAnimations; the two cut draws are Renderer::DrawCutBelowWater / DrawCutAboveWater.
void UpdateSharks(float turnFraction, float gameMilliseconds);

/// OPENBLACK_TEST_SHARK: the shark part of Land 1's FollowUs, once the landscape exists: two sharks created at
/// CONVERT_CAMERA_FOCUS(221) and (230) walking the camera tracks 21 and 20 forward from 0 to 1, as the script does.
/// "track,camera[,forward[,from[,to]]]" makes a single one on that track at that camera's focus instead.
void RunSharkDebugHook();

} // namespace openblack::ecs
