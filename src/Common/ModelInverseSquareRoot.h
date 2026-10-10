/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

namespace openblack::gutils
{

/// The 3D engine's inverse square root, which normalises its vectors: a guess from a 128-entry table of the mantissa's top
/// 7 bits, then one Newton step. It differs in its last bits from both 1 / sqrt(x) and the game's distance InvSqrt.
[[nodiscard]] float ModelInverseSquareRoot(float value);

} // namespace openblack::gutils
