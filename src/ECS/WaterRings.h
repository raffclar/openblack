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

#include <glm/vec3.hpp>

namespace openblack::ecs
{

/// A ring on the water (a pool of 1024): a flat smoke.raw sprite that grows and
/// fades out over 700 ms (drawn after the landscape, render mode 13)
struct WaterRing
{
	glm::vec3 position;
	uint32_t age {0};    ///< ms of game time scaled by the rate; gone at 700
	float growth {1.0f}; ///< half size = age * growth / 700
	float angle {0.0f};  ///< turn about Y
	float aspect {1.0f}; ///< the z half size is half size x aspect
	float rate {1.0f};   ///< age speed
	uint8_t cell {0x30}; ///< cell of the 8 x 8 sheet (low 6 bits)
	/// fixed when the ring is made: the creators that follow the light write (alpha << 24) | (table[i] &
	/// 0xFFFFFF) of that moment (the hand's splash table[255], the rain table[200]), so a ring keeps its colour after
	/// dusk or a lightning flash
	uint32_t argb {0xFFFFFFFFu};
	/// shortcut for the creators of table[255] (the hand's splash, the waterfall): AddWaterRing replaces the
	/// rgb of argb with it and clears the flag
	bool seaLight {false};
	// the ambient-wind drift that some creators leave unwritten (the slot keeps the last ring's value), is not
	// kept: this pool is a vector, not the original's slot array, and the ambient wind is 0 in a game anyway
};

/// The rgb (0x00RRGGBB) of the landscape light table[index] of this frame, for the ring creators
[[nodiscard]] uint32_t LandLightRgb(uint8_t index);

/// Adds a ring; the pool holds 1024 like the original (no new ring when it is full, false). The colour is resolved
/// here (seaLight) and never again.
bool AddWaterRing(const WaterRing& ring);
/// age += (int)(gameMilliseconds * rate), removed at 700
void UpdateWaterRings(float gameMilliseconds);
[[nodiscard]] const std::vector<WaterRing>& GetWaterRings();

} // namespace openblack::ecs
