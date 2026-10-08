/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

// The PSys value noise (the lattice of Ebert et al., "Texturing and Modeling"), used by the rules that
// wiggle atoms (UR_GesturingRecognised, UR_GustyWind). Wiki: docs/bw1-notes/magic.md, "Gestos".

namespace openblack::psys::noise
{
/// The Catmull-Rom spline through `count` knots at x in 0..1
[[nodiscard]] float Spline(float x, int count, const float* knots);
/// The lattice value of i (the 256 values through the permutation table)
[[nodiscard]] float Lattice(int i);
/// The spline through the lattice values around x, about -1..1
[[nodiscard]] float SignedValueNoise(float x);
} // namespace openblack::psys::noise
