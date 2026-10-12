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

/// The ripple's pattern down the rows is laid out on the game's default screen, 800 pixels wide, whose rows are two
/// pixels apart: each even row there takes the next step of the sine, two on from the one before, and each odd row
/// repeats the row above. The camera keeps the game's horizontal field of view, so a pixel of a screen W wide covers
/// 800 / W of what a pixel there did. Rows two pixels apart on a wider screen therefore cover less sea each, and the
/// ripple, the same distance on the sea, would fold the texture back and forth into bands. Instead the pattern runs
/// over the share of the view it covers on the default screen. This gives how many of its rows each row here spans.
inline constexpr float k_ReferenceScreenWidth = 800.0f;
[[nodiscard]] constexpr float RippleRowsPerRow(float screenWidth)
{
	return screenWidth > 0.0f ? k_ReferenceScreenWidth / screenWidth : 1.0f;
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

} // namespace openblack::graphics::sea_rows
