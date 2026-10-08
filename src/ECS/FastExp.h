/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cmath>

namespace openblack::gutils
{

/// The original's inline exp, with the FPU at 24 bits: x log2(e) (rounded to a float: the 64-bit log2(e) as the double
/// one, equal but at a tie); rounded to nearest; the fraction (exact); 2^fraction - 1 (not rounded by the precision);
/// plus 1 (rounded to a float); scaled by 2^whole (exact). Its users: the help spirits (Help/Spirits.cpp) and the
/// hand's grain state
[[nodiscard]] inline float ExpSinglePrecision(float x)
{
	const auto t = static_cast<float>(static_cast<double>(x) * 1.4426950408889634);
	const float whole = std::nearbyint(t);
	const float fraction = t - whole;
	const auto power = static_cast<float>(std::exp2(static_cast<double>(fraction)));
	return std::ldexp(power, static_cast<int>(whole));
}

} // namespace openblack::gutils
