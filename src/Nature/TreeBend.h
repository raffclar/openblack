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
#include <optional>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

// What bends the trees, and how far. Each frame, three kinds of thing write a bend point into a small table:
// - the thing that the hand holds, in its own slot;
// - each body that moves in the physics, in the next slot of a cycle of eleven;
// - the primary creature of the local player, in its own slot.
// Each point marks the trees in the three by three map cells round its own cell with its slot. The last mark on a tree
// replaces the marks before it. A tree reads only the point in its slot. When that point is above the top of the tree
// or out of its reach, the tree loses its mark. A slot keeps its last point until something writes it again.

namespace openblack::nature::tree_bend
{

/// A point trees within its radius lean away from
struct Source
{
	glm::vec3 position {0.0f};
	float radius {0.0f};
};

/// The model of the thing the hand holds, as it is drawn
struct HeldModel
{
	glm::vec3 position {0.0f};
	float scale {1.0f};
	/// Half the diagonal of its mesh's bounding box
	float halfDiagonal {0.0f};
};

/// A tree with this slot has no mark
inline constexpr uint8_t k_NoSlot = 0;
/// The slot of the thing that the hand holds
inline constexpr uint8_t k_HandSlot = 1;
/// The slot of the primary creature of the local player
inline constexpr uint8_t k_CreatureSlot = 2;
/// The moving bodies use the slots from this one to k_LastCycledSlot in turn
inline constexpr uint8_t k_FirstCycledSlot = 3;
inline constexpr uint8_t k_LastCycledSlot = 13;
/// A mark has four bits
inline constexpr uint8_t k_SlotCount = 16;

/// The bend points of all slots. Each slot starts at the origin with no reach.
class BendTable
{
public:
	/// Writes the point into its slot. With no slot, the point goes into the next slot of the cycle. Returns the slot.
	uint8_t Write(const Source& source, std::optional<uint8_t> slot = std::nullopt);
	[[nodiscard]] const Source& At(uint8_t slot) const { return _sources.at(slot % k_SlotCount); }
	/// The slot that the next moving body uses
	[[nodiscard]] uint8_t NextCycledSlot() const { return _nextCycled; }

private:
	std::array<Source, k_SlotCount> _sources {};
	uint8_t _nextCycled {k_FirstCycledSlot};
};

/// What the hand bends the trees with: the thing it holds, reaching its drawn size; nothing with an empty hand
[[nodiscard]] std::optional<Source> HandSource(const std::optional<HeldModel>& held);

/// The map cell of a position along one axis: the tenth of the position, cut to a whole number towards zero
[[nodiscard]] int CellOf(float coordinate);

/// The cells a bend point marks: the three by three cells round its own cell, as a column and row from first to last
struct MarkedCells
{
	glm::ivec2 first;
	glm::ivec2 last;
};
[[nodiscard]] MarkedCells CellsMarkedBy(const glm::vec3& position);

/// How a bend point bends a tree that it marked
struct Bend
{
	/// Horizontal, from the bend point to the tree
	glm::vec3 direction;
	/// 1 at the most. The amount can be less than 0 near a point of a small reach: then the tree leans towards it.
	float amount;
};

/// What a tree does with the point in its slot. The tree stands at treePosition and is treeHeight tall:
/// - it loses its mark (no value) when the point is above its top, or out of reach across the ground;
/// - it keeps its mark but does not bend (an amount of 0) when the point is exactly at its trunk;
/// - else it bends away from the point, more near the point.
[[nodiscard]] std::optional<Bend> ReadBend(const Source& source, const glm::vec3& treePosition, float treeHeight);

} // namespace openblack::nature::tree_bend
