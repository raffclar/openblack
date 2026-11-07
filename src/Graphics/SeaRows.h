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
#include <optional>

#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>

/// Black & White draws its sea in rows across the screen, two pixels apart, from where a square of sea around the island
/// meets the top of the screen down to its bottom. Each row has its own ripple and fades with its distance. This works
/// out the rows on the CPU; fs_water draws them.
namespace openblack::graphics::sea_rows
{

/// The square of sea the rows are worked out from: 30000 units a side on the sea level, about the middle of the map
inline constexpr float k_SquareMinimum = -12440.0f;
inline constexpr float k_SquareMaximum = 17560.0f;

/// How long the sea's texture repeats, in units: from 2000 down to 200 as the detail level's water tiling goes from 0 to
/// 1. At a tiling of 0 the sea is instead a still square of 140000 units repeating its texture 50 times.
[[nodiscard]] constexpr float Period(float waterTiling)
{
	return 2000.0f - (1800.0f * waterTiling);
}
inline constexpr float k_StillPeriod = 2800.0f;

/// The highest and lowest the square of sea reaches on the screen, in pixels from the top, and how far away it is there
/// as one over its view depth. The top is raised to the screen's top and the bottom lowered to its bottom without
/// their depths following.
struct ScreenRange
{
	float top;
	float bottom;
	float inverseDepthTop;
	float inverseDepthBottom;
};

/// Where the square of sea lies on the screen, clipped to the view: none when it is off the screen
/// @param viewProjection world to clip space, with w the view depth
[[nodiscard]] std::optional<ScreenRange> ComputeScreenRange(const glm::mat4& viewProjection, glm::vec2 viewportSize,
                                                            float nearDistance);

/// The rows: row r is at screen y first + 2 r, and its one over view depth inverseDepth + r inverseStep
struct Rows
{
	int first;
	/// Rows 0 to count are drawn
	int count;
	float inverseDepth;
	float inverseStep;
	/// The first row is nearly clear when the sea starts below the top of the screen
	bool softTop;
};
[[nodiscard]] Rows MakeRows(const ScreenRange& range);

/// How far along the sea a row's texture goes for each pixel, as a part of the screen's width. Each row is a strip from
/// the left side of the screen to one pixel past its last column. The game finds the texture at the two ends of the
/// strip from the first column and the last column. Thus the texture across the strip is (width - 1) / width of the sea
/// under it.
[[nodiscard]] constexpr float RowTextureSpan(float viewportWidth)
{
	return viewportWidth > 0.0f ? (viewportWidth - 1.0f) / viewportWidth : 0.0f;
}

/// The rows' ripple: a step of 16 round a sine. The game moves it on by one with each frame it draws the sea while its
/// time goes on, and its frames came at the screen's refresh: it waits for the vertical sync by default, at the display's
/// default refresh rate. Here the step moves on at most once a frame and no more often than that refresh, 60 times a
/// second, so the ripple runs at the same speed whatever the frame rate. Slower frames step once each, as the game's did.
struct Ripple
{
	static constexpr uint8_t k_Steps = 16;
	/// The time between steps at the screen's refresh
	static constexpr std::chrono::duration<float, std::milli> k_StepInterval {1000.0f / 60.0f};

	uint8_t step {0};
	/// The time since the last step that counts towards the next
	std::chrono::duration<float, std::milli> sinceStep {0.0f};
};

/// The ripple after a frame of the sea drawn
/// @param frameTime the frame's real time
/// @param gameTimeMoves whether the game's time moved on in the frame: no step while it is paused
[[nodiscard]] Ripple AdvanceRipple(Ripple ripple, std::chrono::milliseconds frameTime, bool gameTimeMoves);

/// The sea's texture drifts with the ambient wind. The rows move it by world units, which repeat with the period; the
/// still sea moves it by repeats of its texture, which keep only their fraction.
struct Drift
{
	/// Each millisecond of the frame's game time moves the rows' texture by this many units for each unit of wind
	static constexpr float k_RowsRate = -0.0030303032f;
	/// Each millisecond of the frame's game time moves the still sea's texture by this many repeats for each unit of wind
	static constexpr float k_StillRate = -3.0303029e-6f;

	/// The rows' texture offset along x and z, in units, from 0 up to the period on either side
	glm::vec2 rows {0.0f};
	/// The still sea's texture offset along x and z, in repeats of the texture, above -1 and below 1
	glm::vec2 still {0.0f};
};

/// How the sea was drawn in a frame, which tells how far its texture drifts
enum class DriftDraw : uint8_t
{
	/// The rows, which are on the screen: the rows' texture drifts two times
	Rows,
	/// The rows were to be drawn, but none are on the screen: the rows' texture drifts one time
	NoRows,
	/// The still sea of the lowest detail level: the rows' texture and the still sea's texture drift one time each
	Still,
};

/// The drift after a frame of the sea drawn
/// @param wind the ambient wind along x and z
/// @param frameGameTime the frame's game time: none while the game is paused
/// @param period how long the rows' texture repeats (see Period)
[[nodiscard]] Drift AdvanceDrift(Drift drift, glm::vec2 wind, std::chrono::milliseconds frameGameTime, float period,
                                 DriftDraw draw);

} // namespace openblack::graphics::sea_rows
