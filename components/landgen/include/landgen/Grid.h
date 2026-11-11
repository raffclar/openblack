/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <array>
#include <span>
#include <vector>

namespace openblack::landgen
{
/// A square grid of values, one for each corner of the map. The value at (x, z) is at x * side + z, as the cells of a
/// landscape block are.
template <typename T>
struct Grid
{
	int side {0};
	std::vector<T> values;

	Grid() = default;
	Grid(int gridSide, T fill)
	    : side(gridSide)
	    , values(static_cast<size_t>(gridSide) * static_cast<size_t>(gridSide), fill)
	{
	}

	[[nodiscard]] size_t Index(int x, int z) const
	{
		return (static_cast<size_t>(x) * static_cast<size_t>(side)) + static_cast<size_t>(z);
	}
	[[nodiscard]] bool Contains(int x, int z) const { return x >= 0 && z >= 0 && x < side && z < side; }
	[[nodiscard]] T& At(int x, int z) { return values[Index(x, z)]; }
	[[nodiscard]] const T& At(int x, int z) const { return values[Index(x, z)]; }
	[[nodiscard]] int XOf(size_t index) const { return static_cast<int>(index / static_cast<size_t>(side)); }
	[[nodiscard]] int ZOf(size_t index) const { return static_cast<int>(index % static_cast<size_t>(side)); }
	[[nodiscard]] std::span<T> Span() { return values; }
	[[nodiscard]] std::span<const T> Span() const { return values; }
};

/// A step to one of the eight corners next to a corner
struct Step
{
	int dx;
	int dz;
};
/// The eight steps, the four along the axes first
constexpr std::array<Step, 8> k_Neighbours {{{1, 0}, {-1, 0}, {0, 1}, {0, -1}, {1, 1}, {1, -1}, {-1, 1}, {-1, -1}}};
/// The length of each of the eight steps, in cells
constexpr std::array<float, 8> k_NeighbourDistances {1.0f,        1.0f,        1.0f,        1.0f,
                                                     1.41421356f, 1.41421356f, 1.41421356f, 1.41421356f};
} // namespace openblack::landgen
