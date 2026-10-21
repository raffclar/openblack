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

/// How a villager looks at the trees of a forest miracle. It goes from tree to tree of the forest, in the forest's own
/// order, from one picked at random. At each tree it turns to the tree and looks at it for a while. After it has looked
/// at about half to all of the trees, it goes back to what it was doing. The rules here are pure, free of the game.
namespace openblack::ecs::magic_tree_watch
{

/// The number of the first tree the villager looks at, from a roll of 0 to one less than the number of trees
[[nodiscard]] constexpr uint32_t FirstTreeRollRange(uint32_t trees)
{
	return trees - 1;
}

/// The roll of the trees that the villager will not look at, 0 to one less than half the trees
[[nodiscard]] constexpr uint32_t SkippedRollRange(uint32_t trees)
{
	return trees >> 1u;
}

/// Whether the villager looks at one more tree: it has looked at fewer trees than all the trees less the roll of the
/// ones it will not look at. The roll is made again before each tree.
[[nodiscard]] constexpr bool LooksAtAnother(uint32_t looked, uint32_t trees, uint32_t skippedRoll)
{
	return looked < trees - skippedRoll;
}

/// The number of the tree after a tree, back to the first after the last
[[nodiscard]] constexpr uint32_t NextTree(uint32_t tree, uint32_t trees)
{
	return (tree + 1) % trees;
}

/// Where a tree's number points in the forest: the forest's growing trees come first, then its grown trees
struct TreeSlot
{
	bool growing;
	uint32_t index;
};
[[nodiscard]] constexpr TreeSlot SlotOf(uint32_t tree, uint32_t growingTrees)
{
	return tree < growingTrees ? TreeSlot {.growing = true, .index = tree}
	                           : TreeSlot {.growing = false, .index = tree - growingTrees};
}

/// The range of the roll for how long the villager looks at a tree: three seconds of game turns
[[nodiscard]] constexpr uint32_t LookRollRange(uint32_t turnsPerSecond)
{
	return turnsPerSecond * 3;
}

/// How many turns the villager looks at a tree: the roll, and two seconds more. The count is kept in 8 bits.
[[nodiscard]] constexpr uint8_t LookTurns(uint32_t roll, uint32_t turnsPerSecond)
{
	return static_cast<uint8_t>(roll + (turnsPerSecond * 2));
}

/// The look's count of turns, as the villager keeps it: the turns left in the low 8 bits, all the turns in the high
[[nodiscard]] constexpr uint16_t LookCounter(uint8_t turns)
{
	return static_cast<uint16_t>((static_cast<uint32_t>(turns) << 8u) | turns);
}

/// Whether the villager changes how it looks at the tree on this turn: when the turns left are half of all the turns
[[nodiscard]] constexpr bool ChangesLookNow(uint16_t counter)
{
	const auto left = static_cast<uint8_t>(counter & 0xFFu);
	const auto all = static_cast<uint8_t>(counter >> 8u);
	return left == static_cast<uint8_t>(all >> 1u);
}

/// The look's count after a turn: one turn fewer left. The turns left are 8 bits, so 0 becomes 255.
[[nodiscard]] constexpr uint16_t CountDown(uint16_t counter)
{
	const auto left = static_cast<uint8_t>((counter & 0xFFu) - 1u);
	return static_cast<uint16_t>((counter & 0xFF00u) | left);
}

} // namespace openblack::ecs::magic_tree_watch
