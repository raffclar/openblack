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

/// The machine's clock as the game reads it, to time presses, pick among sounds and know the date. Read through the time
/// system, it follows a fixed frame time and counts from when the clock was last restarted, so that a seeded,
/// deterministic run reads the same numbers each time; without a time system (tests that make a system alone) it is the
/// wall clock's.
namespace openblack::machine_clock
{
/// Milliseconds, wrapping as the machine's count does
[[nodiscard]] uint32_t Ticks();
/// Seconds since 1970
[[nodiscard]] int64_t UnixTime();
} // namespace openblack::machine_clock
