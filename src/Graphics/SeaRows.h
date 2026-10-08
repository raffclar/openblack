/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <optional>

#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>

/// The CPU side of the original's sea: the screen rows the sea is drawn in and the wind drift of its texture. The rows
/// themselves are built per pixel in fs_water.
namespace openblack::graphics::sea
{

/// The quad whose screen extent gives the sea rows, 30000 x 30000 on y = 0 centred on (2560, 2560)
inline constexpr float k_RowsQuadMinimum = -12440.0f;
inline constexpr float k_RowsQuadMaximum = 17560.0f;
/// Detail level 0 (WaterTiling = 0): a world quad of +-70000 with 50 texture repeats
inline constexpr float k_LowDetailSeaHalfSize = 70000.0f;
inline constexpr float k_LowDetailSeaPeriod = 2 * k_LowDetailSeaHalfSize / 50.0f; // 2800

/// The lowest and highest screen y (pixels from the top) of the rows quad clipped by the frustum, and 1 / view depth at
/// those two vertices (rhw = near / z, divided by near). top is raised to 0 and bottom lowered to height - 1 without
/// correcting their 1 / z.
struct ScreenRange
{
	float top;
	float bottom;
	float inverseDepthTop;
	float inverseDepthBottom;
};

/// The rows quad's screen range; nullopt when the quad is off the screen (nothing to draw)
/// @param viewProjection world to clip space (w = view depth)
/// @param nearDistance the near plane the quad is clipped with
[[nodiscard]] std::optional<ScreenRange> ComputeScreenRange(const glm::mat4& viewProjection, glm::vec2 viewportSize,
                                                            float nearDistance);

/// The sea's rows: vertex rows r = 0..count at screen y first + 2 r; their 1 / depth is inverseDepth + r * step
/// (affine in the screen y, from the two range vertices)
struct Rows
{
	int first;          ///< top, truncated
	int count;          ///< n = (bottom truncated toward zero - first + 2) / 2; the triangles cover rows 0..n
	float inverseDepth; ///< of row 0
	float inverseStep;  ///< per row: (1/z bottom - 1/z top) / (n - 1)
	bool softTop;       ///< row 0 gets alpha 0x20 (its y > 0.5: the sea starts inside the screen)
};
[[nodiscard]] Rows MakeRows(const ScreenRange& range);

/// The sea texture offsets (one for levels 1..6, one for level 0), moved by the ambient wind every drawn frame.
class Drift
{
public:
	/// Run twice a frame, the second time only when the rows quad is on screen: off += wind.xz * ms * -1/330, then
	/// off -= trunc(off / P) * P
	void ScrollRows(float milliseconds, glm::vec2 wind, float period);
	/// off0 += wind.xz * ms * -1/330000, then off0 -= trunc(off0) (in texture repeats)
	void ScrollLowDetailSea(float milliseconds, glm::vec2 wind);

	[[nodiscard]] glm::vec2 GetRowsOffset() const { return _rows; }
	[[nodiscard]] glm::vec2 GetLowDetailSeaOffset() const { return _level0; }

private:
	glm::vec2 _rows {0.0f};
	glm::vec2 _level0 {0.0f};
};

/// The ambient wind direction: the normalised (ambient[4] / 8, 0, ambient[5] / 8) of the atmosphere's signed bytes, set
/// every turn. ambient[4..5] are only written at start-up (0), by a saved game's load and by the Internet weather, so in
/// a game it is 0 and the sea does not drift.
inline constexpr glm::vec2 k_AmbientWind {0.0f, 0.0f};

} // namespace openblack::graphics::sea
