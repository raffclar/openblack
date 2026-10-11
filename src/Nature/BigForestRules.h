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

/// How a big forest gives up its wood and its trees.
///
/// A big forest is one model standing for many trees. It holds an amount of wood, at first its size times its kind's
/// wood, which its life scales into the wood it is worth. Wood taken from it is divided by its life before it comes off.
/// When what is taken would leave it with none (or less), it is used up and goes from the land, leaving nothing behind;
/// its forest stays, with any trees it planted. Otherwise, once the wood it holds has drifted more than 250 from what its
/// drawn size stands for, it shrinks to the size of the wood it has left, and plants a young pine at its new edge.
///
/// The hand takes a tree from a big forest: a full grown conifer, as much wood as one is worth taken from the forest
/// first, is made upright at the big forest's own place, facing the land's north, and goes straight into the hand. It is
/// one of the big forest's own forest's trees.
namespace openblack::big_forest
{

/// How far the wood a big forest holds may drift from what its drawn size stands for before it is shrunk
inline constexpr float k_ShrinkDrift = 250.0f;

/// The tree the hand pulls out of a big forest: full grown, at its full size, facing the land's north
inline constexpr float k_GivenTreeSize = 1.0f;
inline constexpr float k_GivenTreeFacing = 0.0f;

/// A shrinking big forest plants a pine round its edge: up to this many spots are tried
inline constexpr uint32_t k_PineTries = 10;
/// A spot is refused when something in its cell stands within this far of it, its own radius and all
inline constexpr float k_PineClearance = 4.0f;
/// The pine's full size, this and a random part of the next more, and the size it starts at
inline constexpr float k_PineFullSize = 0.75f;
inline constexpr float k_PineFullSizeMore = 0.5f;
inline constexpr float k_PineStartSize = 0.05f;

/// The wood a big forest of a size starts with, from its kind's wood
[[nodiscard]] float StartWood(float scale, uint32_t kindWood);
/// The wood a big forest is worth: what it holds, at its life
[[nodiscard]] float WoodValue(float life, float wood);

/// What taking wood from a big forest does
struct WoodTaken
{
	/// The wood it holds after
	float woodLeft {0.0f};
	/// It had no more than was taken: it is used up and goes
	bool usedUp {false};
	/// It shrinks to this size (and plants a pine round its new edge)
	std::optional<float> shrinksTo;
	/// How much wood was taken: all that was asked for, or what it was worth when it was used up, in whole units
	uint32_t taken {0};
};
/// Wood taken from a big forest of a life, holding wood at a size, of a kind with a wood
[[nodiscard]] WoodTaken TakeWood(float life, float wood, float scale, uint32_t kindWood, uint32_t amount);

/// Whether something in a spot's cell, so far from the spot and of a radius, keeps a pine from being planted there
[[nodiscard]] bool BlocksPine(float distance, float radius);

} // namespace openblack::big_forest
