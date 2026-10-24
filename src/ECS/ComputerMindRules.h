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

#include <optional>
#include <span>

/// The rules by which a computer god finds how much it wants a town's need or a miracle. Pure functions of what the god
/// sees.
namespace openblack::computer_mind_rules
{

/// The worship site's icon of a miracle, as the god sees it
struct IconFacts
{
	/// The icon belongs to a worship site
	bool site {false};
	/// The prayer power that the icon has stored
	float store {0.0f};
	/// The prayer power that the miracle costs to make
	float cost {0.0f};
	/// The site never runs out of prayer power
	bool infinite {false};
	/// The prayer power that the site keeps, and what its dancers give this turn and what was used of it
	float battery {0.0f};
	float available {0.0f};
	float used {0.0f};
	/// The prayer power that the dancers of the site chant each turn
	float capacity {0.0f};
};

/// The prayer power that the site reports when it never runs out
constexpr float k_InfiniteChants = 1.0e6f;
/// A small amount that keeps a share from a division by zero
constexpr float k_SmallChants = 0.001f;
/// How much each request for a miracle adds to the god's wish to charge it
constexpr float k_RequestShare = 0.01f;
/// The turns to charge a miracle at which the god no longer wants it
constexpr float k_MostChargeTurns = 2000.0f;

/// The part of the miracle's cost that the icon and its site could give now. With no dancers and less than the cost, 0.
[[nodiscard]] float ChargeShare(const IconFacts& icon);
/// The turns that the dancers need to charge the rest of the miracle: 0 when the site has enough now, -1 when the icon
/// has no site or its site has no dancers
[[nodiscard]] float TurnsToCharge(const IconFacts& icon);
/// How much the god wants to charge a miracle that it asked for this many times, 0 to 1
[[nodiscard]] float RequestShare(uint32_t requests);
/// How much the god wants to cast a miracle: the share of the charge that is there, less the further the charge is from
/// done, and the place's own pull. Never more than 1.
[[nodiscard]] float SpellValue(float charge, float turnsToCharge, float placePull);

/// The town that wants a thing most: the first with the largest desire over 0
[[nodiscard]] std::optional<size_t> MostWanting(std::span<const float> desires);

/// An icon of a worship site that the god could charge
struct ChargeCandidate
{
	/// The worshippers chant at the icon now
	bool charging {false};
	/// The prayer power that the icon has stored
	float store {0.0f};
	/// How many times the god asked for the miracle of the icon
	uint32_t requests {0};
};

/// The icon that the god wants to charge most, and how much: the first with the largest wish over 0 among the icons that
/// do not charge and have nothing stored
struct ChargeChoice
{
	size_t index {0};
	float value {0.0f};
};
[[nodiscard]] std::optional<ChargeChoice> ChooseCharge(std::span<const ChargeCandidate> icons);

} // namespace openblack::computer_mind_rules
