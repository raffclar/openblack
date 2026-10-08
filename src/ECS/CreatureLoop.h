/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include "Enums.h"

// The creatures in the game loop: their turn, their updates between two turns, their leashes' ropes, a new land and
// the leash keys. Every pass walks the creatures found in the registry, so with no creature in the world nothing is
// changed and nothing is drawn from the random streams. Docs: docs/bw1-notes/creature.md.

namespace openblack
{
class Profiler;
} // namespace openblack

namespace openblack::input
{
class GameActionInterface;
} // namespace openblack::input

namespace openblack::ecs::creature_loop
{
/// The creatures' turn, one block right after the living list (approximate: the original runs each creature inside
/// the living list): their bodies, poses, skins, leashes, minds, plans, learning, walking, what they act on, their
/// fights, then the miracles cast on them
void ProcessTurn(Profiler& profiler);

/// Whether the world's creatures stand still this frame: the world is paused while the player is inside the citadel,
/// and its creatures freeze with it (the citadel's creature room has updates of its own)
[[nodiscard]] constexpr bool FrozenInCitadel(bool paused, bool insideCitadel)
{
	return paused && insideCitadel;
}

/// The creatures between two turns, a share of the way through the turn and some game milliseconds on: fights, drawn
/// poses, the sick, what they hold, bodies, hair, sounds, footprints and skins
void UpdateFrame(float turnFraction, uint32_t frameGameMs, Profiler& profiler);

/// The leashes' ropes, some game seconds on, once the hand is placed
void UpdateLeash(float frameGameSeconds, Profiler& profiler);

/// A new land: no footprints of the last one
void OnLoadMap();

/// The leash shortcuts (L, V, B) that went down this frame, for the local player's creature when they have one they
/// can lead; with none, a press does nothing and logs nothing
void ProcessLeashKeys(const input::GameActionInterface& actions);

/// The leash the player's hand holds comes off, as a hand demo starts; one tied to something stays. Without a creature on
/// a leash in the hand nothing is done and nothing is logged
void ReleaseLeashHeldInHand(PlayerNames player);
} // namespace openblack::ecs::creature_loop
