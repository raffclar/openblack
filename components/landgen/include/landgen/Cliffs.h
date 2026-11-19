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

#include <span>

#include "landgen/Grid.h"
#include "landgen/HeightCurve.h"

/// The faces of the cliffs. The cliffs of the lands of the story are steep, but few of their faces are near vertical.
namespace openblack::landgen::cliffs
{
struct FaceSettings
{
	/// The steepest face that a part of the land can have, as the rise in metres for each metre. A noise of the seed
	/// picks the limit of each part between the two values. 2.2460 is 66 degrees and 4.7046 is 78 degrees.
	float steepestLow {1.1918f};
	float steepestHigh {3.7321f};
	/// The waves of the noise across the map
	float frequency {9.0f};
	/// The most passes over the map. Each pass goes along the map one time in each direction.
	int passes {8};
	/// A summit is the highest corner of the square that goes this many corners to each side of it
	int summitReach {15};
	/// The ground round a summit goes up to a cone over this many corners to each side of it
	int coneReach {8};
};

/// The steepest rise of a face at a point, in metres for each metre. u and v go from 0 to 1 across the map.
[[nodiscard]] float SteepestRiseAt(uint64_t seed, float u, float v, const FaceSettings& settings);

/// Makes the faces of the land no steeper than the limit of their part of the land. The heights are altitudes; the
/// curve gives their metres.
/// - A summit that the limit would cut down stays. The ground round it goes up to a steep cone, so that a narrow spire
///   becomes a peak.
/// - Each other corner that is higher than a corner next to it by more than the limit goes down to the limit.
/// The sea and the fixed corners (for example the rivers) do not go up. The same seed gives the same land.
void LimitFaces(Grid<float>& heights, const HeightCurve& curve, float seaLevel, uint64_t seed, const FaceSettings& settings,
                std::span<const uint8_t> fixed = {});
} // namespace openblack::landgen::cliffs
