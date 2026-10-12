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
#include <functional>
#include <optional>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

/// A storm's own clouds: up to sixteen puffs of mist that gather one a frame over the storm, each wandering slowly round
/// it at its cloud height, tinted from the land's colour by the storm's shade and as opaque as the storm is strong. A
/// strong or dark storm also darkens the land under it.
namespace openblack::storm_clouds
{

/// The most clouds a storm gathers, whatever its count
inline constexpr int k_MostClouds = 16;
/// A cloud takes this many steps to reach each place it wanders to
inline constexpr int k_StepsPerLeg = 400;
/// The clouds are drawn, and step on, with the frames: at most 60 times a second, the screen's refresh the game waited for
inline constexpr std::chrono::duration<float, std::milli> k_StepInterval {1000.0f / 60.0f};
/// A cloud with no more opacity than this is not drawn
inline constexpr uint8_t k_LeastShownAlpha = 5;
/// The shadow a storm casts on the land covers this many cells along each side, centred on the storm
inline constexpr int k_ShadowCells = 40;

/// A random number between two others, from the C runtime's numbers
using Random = std::function<float(float low, float high)>;

/// Where a cloud is over its storm and where it is going: x and z are fractions of half the storm's reach either side of
/// its middle, y its height above the ground
struct Drift
{
	glm::vec3 place {0.0f};
	glm::vec3 target {0.0f};
	/// What it moves by each step
	glm::vec3 step {0.0f};
	/// Steps until it reaches its target; at none it picks a new one
	int32_t stepsLeft {0};
};

/// A new cloud's shape and where it starts
struct NewCloud
{
	/// How many times smaller it looks seen level than seen from above or below
	float edgeShrink;
	/// Its size as a share of the storm's reach
	float sizeFactor;
	Drift drift;
};

/// How many clouds a storm with this count gathers
[[nodiscard]] int Wanted(int32_t cloudCount) noexcept;

/// A new cloud for a storm with clouds at this height, drawing its numbers in the game's order (the mist's own animation
/// takes the one before)
[[nodiscard]] NewCloud MakeCloud(const Random& random, float cloudHeight);

/// One step of a cloud's wander: once at its target it picks the next, up to 20 above the cloud height and anywhere over
/// the storm, and sets off there on the next step; otherwise it moves a step on
void Step(Drift& drift, const Random& random, float cloudHeight);

/// Where a cloud is drawn and how big
struct Placement
{
	glm::vec2 xz;
	/// Its height above the ground under it
	float heightAboveGround;
	float size;
};
/// A cloud over a storm whose middle and reach (its inner radius as faded plus its outer radius) are given
[[nodiscard]] Placement Place(const Drift& drift, float sizeFactor, const glm::vec3& stormCentre, float innerRadius,
                              float outerRadius);

/// A cloud's opacity, 0 to 255, for the storm's strength as faded: three quarters at full strength
[[nodiscard]] uint8_t Alpha(float strength) noexcept;

/// A cloud's colour, 0xRRGGBB: the land's colour, darkened by half the storm's shade when it has any
[[nodiscard]] uint32_t Tint(uint32_t landColour, float shade) noexcept;

/// The flash a storm's lightning lights its clouds with, 0 to 255, from the glow of its last flash
[[nodiscard]] uint8_t FlashGrey(float glow) noexcept;

/// How darkly the storm shades the land under it, 1 to 255; none when it is too faint
[[nodiscard]] std::optional<uint8_t> ShadowAlpha(float shade, float strength) noexcept;

/// The corner of the storm's shadow: the shadow is centred on the storm, a texel to each cell
[[nodiscard]] glm::vec2 ShadowCorner(glm::vec2 stormCentre) noexcept;

/// Steps due this frame, at most one: the clouds step at the screen refresh's pace whatever the frame rate, as the game
/// drew them once a frame at its vertical sync
[[nodiscard]] bool StepDue(std::chrono::duration<float, std::milli>& sinceStep,
                           std::chrono::duration<float, std::milli> frameTime) noexcept;

} // namespace openblack::storm_clouds
