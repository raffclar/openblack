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

#include "Enums.h"

/// The rules of the hand knocking on a town's building: which buildings take a knock, which of them knock back with a
/// sound and the hand's tap, the knocking sounds in turn, and the read-out of how many people live in each of the
/// town's houses that a knock brings up for a few seconds. Pure, for the abode knock system and its tests.
namespace openblack::ecs::abode_knock
{

/// A building of a town can be knocked on once something of it is built, unless it is the village centre
[[nodiscard]] constexpr bool IsTappable(AbodeType type, float built)
{
	return built > 0.0f && type != AbodeType::TownCentre;
}

/// Only houses (and the windmill, which counts as one) answer a knock with the knocking sound and the hand's tap
[[nodiscard]] constexpr bool KnocksBack(AbodeType type)
{
	return (static_cast<uint32_t>(type) & static_cast<uint32_t>(AbodeType::LivingQuarters)) != 0;
}

/// The knocking sounds, played one after another in this order by every knock in the game
constexpr uint32_t k_KnockSounds = 9;
/// The sound after the one just played
[[nodiscard]] constexpr uint32_t NextKnockSound(uint32_t sound)
{
	return (sound + 1) % k_KnockSounds;
}

/// The read-out shows for this long, in milliseconds: a second growing in, six held, a second shrinking away
constexpr uint32_t k_ReadoutMs = 8000;
constexpr uint32_t k_ReadoutGrowMs = 1000;
constexpr uint32_t k_ReadoutShrinkMs = 1000;
/// Knocking starts the read-out, or, once it has grown in, holds it for its full time again; while it shrinks away it
/// turns round and grows back from the size it has reached. `remaining` counts down to 0, where nothing shows.
[[nodiscard]] uint32_t Knock(uint32_t remaining);
/// The time left once a frame has gone by
[[nodiscard]] constexpr uint32_t Tick(uint32_t remaining, uint32_t frameMs)
{
	return remaining > frameMs ? remaining - frameMs : 0;
}
/// How big the read-out's markers are, from nothing to full, with this much time left: they grow in over the first
/// second and shrink away over the last, in steps of a 255th
[[nodiscard]] float ReadoutScale(uint32_t remaining);

/// A marker's colour, 0xAARRGGBB: gold for each adult living there, green for each free place; a held follower near a
/// house shows a single teal one
constexpr uint32_t k_LivedInColour = 0xFFF8D235u;
constexpr uint32_t k_FreePlaceColour = 0xFF5C822Fu;
constexpr uint32_t k_FollowerColour = 0xFF4995A9u;
/// The markers stand this far apart along the screen's sideways line, and are this big at full size
constexpr float k_MarkerSpacing = 1.0f / 1.3f;
/// They are never drawn smaller than this
constexpr float k_SmallestMarker = 0.0001f;
/// Over a building, the row is raised past twice its model's height by this much
constexpr float k_RaisedAbove = 1.5f;
/// A row of markers over a house: one per place in it. The row starts half a place to the left of the house's middle
/// for each place, so it sits a little left of centre.
struct Marker
{
	/// How far along the screen's sideways line from the house's middle
	float offset;
	uint32_t colour;
	float size;
};
/// The markers over a house with `places` places and `adults` adults living there, at a scale
[[nodiscard]] std::vector<Marker> Readout(uint32_t places, uint32_t adults, float scale);

} // namespace openblack::ecs::abode_knock
