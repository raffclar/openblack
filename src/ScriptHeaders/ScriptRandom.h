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

namespace openblack
{
class GameRandomInterface;
}

/// The scripts' random whole numbers, such as a line picked at random from a run of help texts
namespace openblack::script::random
{

/// A whole number from low to high, both included, drawn from the game's synced random numbers. The width is worked out
/// unsigned, as the game does, so high below low wraps round rather than failing, and a full-width range draws nothing
/// and gives low
[[nodiscard]] uint32_t WholeNumberBetween(uint32_t low, uint32_t high, GameRandomInterface& random);

} // namespace openblack::script::random
