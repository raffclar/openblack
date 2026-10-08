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

#include <chrono>

/// (openblack) A clock that goes up by the same milliseconds every frame, for runs that must repeat (the replay test
/// of Debug/StateHash.h, profiles). OPENBLACK_FIXED_FRAME_MS=<ms> (1..1000) installs it as the tick source of
/// game_clock (GetTickCount for the game timer, the engine timer and src/Audio) and as the frame's delta of
/// Game::Update. Not covered: Particles/Rules/Lightning.cpp's 10 s steady_clock timeout. Off by default: the game reads the
/// wall clock as the original does. Install it only while no audio thread runs (before the Game, after it).
namespace openblack::fixed_clock
{

/// Reads OPENBLACK_FIXED_FRAME_MS once and installs the clock (Game::Initialize, before game_clock::Start)
void InstallFromEnvironment();
/// Tests: install it with `frameMs` (0 = uninstall, the wall clock again)
void Install(uint32_t frameMs);
[[nodiscard]] bool Enabled();
/// The start of Game::Update: the ticks go up by the frame's ms; returns the frame's delta
std::chrono::microseconds AdvanceFrame();

} // namespace openblack::fixed_clock
