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

#include <bit>
#include <span>

// The rules of a wonder. A scaffold of seven or more scaffolds offers its town a wonder. The power of the wonder comes
// from how much the town wants a wonder and from the things near the scaffold. The size of the wonder comes from its
// power. A finished wonder adds its power to the power that its tribe gives its player. Pure rules, tested on their own.

namespace openblack::ecs::wonder
{

/// The search for the things near a wonder reaches this far for each unit of the desire of the town for a wonder, and
/// at least this far, in metres
inline constexpr float k_ReachPerDesire = 15.0f;
/// Each thing in reach adds this part of its impressiveness to the power
inline constexpr float k_ImpressivenessShare = std::bit_cast<float>(0x3DCCCCCDU); // 0.1
/// The power is at least this
inline constexpr float k_LeastPower = 0.25f;
/// The size is worked from a power of no more than this, and loses this part of that power for each unit of it
inline constexpr float k_MostPowerForScale = 5.0f;
inline constexpr float k_ScaleLossPerPower = std::bit_cast<float>(0x3DCCCCCDU); // 0.1
/// The cells across the search are its reach times this
inline constexpr float k_CellsPerReach = std::bit_cast<float>(0x3E4CCCCDU); // 0.2
/// A tribe gives its player at least this power
inline constexpr float k_LeastTribalPower = 1.0f;

/// A thing in the map cell of the place of a wonder, as the search sees it
struct Neighbour
{
	/// The distance across the land from the place, in metres
	float distance {0.0f};
	/// Its worth as an artefact (0 when it is not an artefact) and its impressiveness
	float artefactWorth {0.0f};
	float impressiveness {0.0f};
};

/// How far the search reaches for a desire of the town for a wonder: the desire times 15 metres, at least 15 metres
[[nodiscard]] float Reach(float wonderDesire);

/// The count of the steps of the search for a reach: the cells across it, at least 1, squared
[[nodiscard]] uint32_t SearchSteps(float reach);

/// The power of a wonder at a place. It starts at the desire of the town for a wonder. Each step of the search adds, for
/// each thing nearer than the reach, its worth as an artefact and a tenth of its impressiveness. The power is at least a
/// quarter.
///
/// The game moves the search from cell to cell, but it always looks in the cell of the place. Thus each step counts the
/// things of that one cell again.
[[nodiscard]] float Power(float wonderDesire, std::span<const Neighbour> inCell);

/// The size of a wonder for its power: the power (5 at most) times 1 minus a tenth of it
[[nodiscard]] float Scale(float power);

/// The most power that a tribe gives a player, after the player gets a finished wonder of that tribe
[[nodiscard]] float WithWonder(float mostTribalPower, float wonderPower);
/// The most power that a tribe gives a player, after the player loses a finished wonder of that tribe
[[nodiscard]] float WithoutWonder(float mostTribalPower, float wonderPower);

/// The power of a tribe for a player at each turn: the most that the tribe gave the player, and at least 1. The most
/// power also becomes at least 1.
[[nodiscard]] float SettledTribalPower(float mostTribalPower);

} // namespace openblack::ecs::wonder
