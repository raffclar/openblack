/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <concepts>
#include <cstddef>
#include <cstdint>

#include <optional>

/// How the land's forests grow, once a game turn.
///
/// Only trees that belong to a forest grow, and only while the forest counts them among its growing trees. A tree made
/// smaller than it may grow joins a forest as a growing tree; every turn its clock counts down, and when it runs out it
/// grows a little (faster in the rain or on lying snow, and on good land), up to its largest size. The first time the
/// clock runs out with the tree full grown, or with it never made to grow, it becomes one of the forest's grown trees
/// for good. Trees never age, wither or die of old age; their mesh never changes.
///
/// A forest with grown trees spreads now and then: the chance rises with every turn since it last spread, with the turns
/// since any forest last gained a tree, and with its number of grown trees up to twenty. A young tree of the same kind
/// then grows beside one of the forest's nearer grown trees. A town's scenic forest never grows or spreads. A forest left
/// with no trees goes 1999 turns after it is first seen empty.
namespace openblack::forest_growth
{

/// What a kind of tree does as it grows
struct Kind
{
	/// Turns between growths
	uint32_t turnsBetween;
	/// How much it grows each time in plain weather on neutral land
	float amount;
	/// How much faster rain or lying snow makes it grow, for each point of it, in hundredths
	float rainAccelerator;
};

/// How wet a tree's ground is for growing: the more of the rain falling and the snow lying on it, either of which may be
/// below nothing
[[nodiscard]] int Wetness(int8_t rain, int8_t snowCover);
/// How much it grows: its amount, more by the share of the wetness times its accelerator, and by half the land's
/// alignment
[[nodiscard]] float Growth(const Kind& kind, int wetness, float landAlignment);
/// Its size after growing by an amount, no larger than its largest
[[nodiscard]] float Grown(float size, float amount, float largest);

/// The turns a tree made short of its size first waits to grow, from a draw below its kind's wait. Its clock is 16 bits
/// and is counted down before it is looked at, so a wait of none goes the whole clock round.
[[nodiscard]] uint16_t FirstWait(uint32_t draw);

/// What a growing tree's turn did
struct TreeTurn
{
	/// Its new size, when it grew
	std::optional<float> size;
	/// Whether it stays among the forest's growing trees; once not it is among the grown ones for good
	bool staysGrowing;
};

/// A growing tree's turn: its clock counts down, and when it runs out it is wound back to its kind's wait and the tree
/// grows by the amount asked for (only then, as the weather and the land are looked at only then), if it was made to
/// grow and is still short of its largest size
template <std::invocable<> Amount>
[[nodiscard]] TreeTurn TreeStep(uint16_t& countdown, bool madeToGrow, float size, float largest, uint32_t turnsBetween,
                                Amount&& amount)
{
	--countdown;
	if (countdown != 0)
	{
		return {.size = std::nullopt, .staysGrowing = true};
	}
	countdown = static_cast<uint16_t>(turnsBetween);
	if (!madeToGrow || !(size < largest))
	{
		return {.size = std::nullopt, .staysGrowing = false};
	}
	const float grown = Grown(size, amount(), largest);
	return {.size = grown, .staysGrowing = grown < largest};
}

/// Whether a forest spreads this turn. Its counter goes up every turn it is looked at; it spreads when a roll of 2000
/// to 3000 (2000 and a draw below 1000) falls below the counter times the turns since any forest last gained a tree,
/// over 300, times a twentieth of its grown trees (no more than all). Spreading starts the counter again.
[[nodiscard]] bool Spreads(float roll, uint16_t& counter, uint32_t turnsSinceLastGain, size_t grownTrees);
/// The parent of a forest's new tree is drawn from its grown trees nearest the forest first: the draw is below this
[[nodiscard]] uint32_t ParentDraws(size_t grownTrees);

/// A forest with no trees left (and no big forest) counts down from 2000, set the first turn it is seen empty, and goes
/// when the count falls below 2. The count is never set again, so one emptied a second time goes sooner. Whether it
/// goes this turn.
[[nodiscard]] bool EmptyForestGoes(uint16_t& countdown);

} // namespace openblack::forest_growth
