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

#include <array>
#include <vector>

#include <glm/vec3.hpp>

// How a worship site shows what it has and what it needs: the three need flags, the ring of light round its dance and
// the throb of its strain. Pure rules, free of the game.

namespace openblack::ecs::worship_site_looks
{

/// A built site shows three need flags: food, rest and hiding. Flag i uses row i of the need flags' info and flies at
/// point i of the site's model.
inline constexpr size_t k_NeedFlags = 3;
enum class Need : uint8_t
{
	Food,
	Rest,
	Hiding,
};

/// How many people hide in the site, as the hiding flag shows it: each worshipper that does not dance adds a
/// hundredth, at most 1. A site without a dance shows none.
[[nodiscard]] float HidingIndicator(bool hasDance, uint32_t worshippers, uint32_t dancers);

/// The ring of light round a site's dance: ten points on a circle of this radius, the last on the first, raised to this
/// height and spread out at the top by this much. Its strength moves one point along every this many seconds.
inline constexpr size_t k_RingPoints = 10;
inline constexpr float k_RingRadius = 3.0f;
inline constexpr float k_RingHeight = 10.0f;
inline constexpr float k_RingSpread = 1.5f;
inline constexpr float k_RingShiftSeconds = 0.03f;

/// The ring's points round its centre, as the game works them out
[[nodiscard]] std::vector<glm::vec3> RingPoints(glm::vec3 centre);

/// How strong the ring is: how full the site's store of prayer power is, 0 to 1
[[nodiscard]] float RingStrength(float battery, float maxBattery);

/// The colour of the ring: the player's colour, which throbs dark and back with the pulse while the site strains
[[nodiscard]] uint32_t RingRgb(uint32_t playerRgb, float strain, float pulse);

/// The stars slide down the ring as fast as the site strains
[[nodiscard]] constexpr float RingSlideSpeed(float strain)
{
	return -strain;
}

/// The seconds that the ring moves on in a frame of so many milliseconds of game time
[[nodiscard]] float FrameSeconds(int32_t frameMilliseconds);

/// The throb of a straining site, which runs every frame: its phase in radians and its pulse, 0 to 1
struct Throb
{
	float phase {0.0f};
	float pulse {0.0f};
};
/// The throb moves on through a frame: faster the more the site strains, from 5 to 10 radians a second. A site that
/// does not strain keeps its throb as it is.
[[nodiscard]] Throb NextThrob(Throb throb, float strain, int32_t frameMilliseconds);

/// How clear the motes of prayer power from the site are, 0 to 255: they throb with the site while it strains
[[nodiscard]] uint32_t MoteAlpha(float strain, float pulse);

} // namespace openblack::ecs::worship_site_looks
