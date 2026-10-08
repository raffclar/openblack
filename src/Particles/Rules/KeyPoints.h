/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <vector>

#include <glm/vec3.hpp>

// The key-point curves of the spell files (UR_KPStretchHeight, UR_KPMoveAtoms, UR_ForestPath's RadiusSpline /
// HeightSpline). Wiki: docs/bw1-notes/miracles.md, beam explosion and the missing PSys classes.

namespace openblack::psys::key_points
{
/// The keys: (t, value, second derivative) per key, 12 bytes each like the original's array
struct Spline
{
	std::vector<glm::vec3> keys;
};

/// The array property's setter: the file's flat (t, value) pairs (an odd last value is dropped: count / 2 * 2), then
/// the cubic spline's second derivatives (Numerical Recipes' spline(): first slope yp1 and last slope ypn). Every
/// rule's constructor sets the array's flag, which passes yp1 = ypn = 0 (zero slopes at the ends); without it 1e30
/// would make a natural spline.
[[nodiscard]] Spline Make(const std::vector<float>& pairs, bool zeroEndSlopes = true);

/// The spline at t (splint): bisection for the keys around t, then a y_lo + b y_hi + ((a^3 - a)
/// y2_lo + (b^3 - b) y2_hi) h^2 / 6, not clamped outside the keys. With keys at the same t (h = 0, or fewer than two
/// keys) nothing is written: the caller's value (`unchanged`) stays.
[[nodiscard]] float Evaluate(const Spline& spline, float t, float unchanged);
} // namespace openblack::psys::key_points
