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

#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "UnderwaterStreams.h"

/// EXPERIMENT (not for merge): the splash where the hand taps the water. In the original game a flat ring shows on the
/// water. The experiment makes a splash in three dimensions: a sheet of water that rises round the hand and breaks, drops
/// that fly up and fall back with rings where they land, a fine spray, foam on the water and rings in the water's field
/// of small waves. The shader vs_underwater_tap.sc has the same drops.
namespace openblack::graphics::underwater::tap
{

/// The splash's particles in the shader: the sheet, the drops, the spray and the bits of foam
inline constexpr uint32_t k_SheetParticles = 32;
inline constexpr uint32_t k_DropParticles = 80;
inline constexpr uint32_t k_SprayParticles = 32;
inline constexpr uint32_t k_FoamParticles = 48;
inline constexpr uint32_t k_Particles = k_SheetParticles + k_DropParticles + k_SprayParticles + k_FoamParticles;
/// The most taps that show at once
inline constexpr size_t k_MostTaps = 6;
/// The most drops of one tap that make a ring where they land
inline constexpr size_t k_MostDropRings = 14;

/// How hard a tap is: 1 for a usual tap, up to k_HardestTap for a fast hand
inline constexpr float k_HardestTap = 1.6f;
inline constexpr float k_SoftestTap = 0.6f;

/// The shape of one tap's splash, in units and seconds
struct TapSplash
{
	/// How high the sheet goes, and its radius at its base
	float sheetHeight {0.0f};
	float sheetRadius {0.0f};
	/// How fast the drops leave the water, and how many there are
	float dropSpeed {0.0f};
	uint32_t drops {0};
	/// How dense the fine spray is, 0 to 1
	float spray {0.0f};
	/// How deep the hole is that starts the rings in the field, and its radius
	float ringDepth {0.0f};
	float ringRadius {0.0f};
	/// How long the splash shows
	float seconds {0.0f};
	/// Three numbers of its own, 0 to 1, that the drops' directions and speeds come from
	glm::vec3 seed {0.0f};
};

/// The splash of a tap of a strength (see k_SoftestTap and k_HardestTap). `calm` is 0 on the open sea and 1 on calm
/// water such as a pond or a stream: the splash there is smaller and makes less spray.
[[nodiscard]] TapSplash SplashFor(float strength, float calm, glm::vec3 seed);

/// How hard a tap is from the hand's speed when it taps, in units a second
[[nodiscard]] float StrengthForHandSpeed(float speed);

/// One drop of a splash: where it leaves the water, its velocity, when it leaves and how big it is
struct Drop
{
	glm::vec2 start {0.0f};
	glm::vec3 velocity {0.0f};
	float delay {0.0f};
	float size {0.0f};
	/// Whether this splash has this drop
	bool shown {false};
};

/// The drop `index` of a splash, from the splash's centre. The numbers come from a sequence that the shader also
/// makes, so the shader's drops land where their rings start.
[[nodiscard]] Drop DropOf(const TapSplash& splash, uint32_t index);

/// Where a drop lands back on the water, and when, after the tap
struct Landing
{
	glm::vec2 position {0.0f};
	float seconds {0.0f};
	float size {0.0f};
};
/// Where a drop lands: it falls under gravity back to the level that it left from
[[nodiscard]] Landing LandingOf(const Drop& drop);
/// The landings of the biggest drops of a splash that make rings, at most k_MostDropRings, the first to land first
[[nodiscard]] std::vector<Landing> RingLandings(const TapSplash& splash);

/// The depth of the small ring that a drop makes where it lands
[[nodiscard]] float DropRingDepth(float size);

} // namespace openblack::graphics::underwater::tap
