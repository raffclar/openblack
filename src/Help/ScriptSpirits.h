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

#include <functional>

/// How the scripts name the advisors: the rules between a script's spirit and the advisor it means
namespace openblack::help::script_spirits
{

/// The advisors as the help names them: 1 the good one, 2 the evil one
constexpr int32_t k_GoodSpirit = 1;
constexpr int32_t k_EvilSpirit = 2;

/// An alignment from -1 (evil) to 1 (good) in seven steps, 0 the most evil to 6 the most good
[[nodiscard]] int32_t DiscreteAlignment(float alignment);

/// The advisor a script's spirit means: none or good (0, 1) the good one, evil (2) the evil one, the player's alignment
/// (3) the evil one for an alignment below neutral, its opposite (4) the evil one from neutral up, random (5) either,
/// by a number below 100 that `rand100` draws only then
[[nodiscard]] int32_t HelpSpiritOf(int32_t scriptSpirit, int32_t discreteAlignment, const std::function<uint32_t()>& rand100);

/// The advisor who says a help text, by its narrator: 1 good, 2 evil, 0 neither
[[nodiscard]] int32_t SpiritWhoTalks(int32_t narrator);

/// A script's fraction across or down the screen, from 0 to 1. Not a number is not one.
[[nodiscard]] bool IsScreenFraction(float value);

} // namespace openblack::help::script_spirits
