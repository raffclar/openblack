/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "TreeBend.h"

#include <cmath>

namespace openblack::nature::tree_bend
{

namespace
{
// A map cell is ten units a side
constexpr float k_CellsPerUnit = 0.1f;
// The bend falls from (1 - k_BendCore / radius) at the trunk to nothing at the radius
constexpr float k_BendCore = 0.75f;
} // namespace

uint8_t BendTable::Write(const Source& source, std::optional<uint8_t> slot)
{
	uint8_t written = 0;
	if (slot.has_value())
	{
		written = *slot % k_SlotCount;
	}
	else
	{
		written = _nextCycled;
		_nextCycled = _nextCycled >= k_LastCycledSlot ? k_FirstCycledSlot : static_cast<uint8_t>(_nextCycled + 1);
	}
	_sources.at(written) = source;
	return written;
}

std::optional<Source> HandSource(const std::optional<HeldModel>& held)
{
	if (!held.has_value())
	{
		return std::nullopt;
	}
	// A model with a scale below zero has a reach below zero, and so bends no tree
	return Source {.position = held->position, .radius = held->scale * held->halfDiagonal};
}

int CellOf(float coordinate)
{
	return static_cast<int>(coordinate * k_CellsPerUnit);
}

MarkedCells CellsMarkedBy(const glm::vec3& position)
{
	const glm::ivec2 cell {CellOf(position.x), CellOf(position.z)};
	return {.first = cell - 1, .last = cell + 1};
}

std::optional<Bend> ReadBend(const Source& source, const glm::vec3& treePosition, float treeHeight)
{
	// A point above the top of the tree bends it no more
	if (treePosition.y + treeHeight < source.position.y)
	{
		return std::nullopt;
	}
	const float acrossX = treePosition.x - source.position.x;
	const float acrossZ = treePosition.z - source.position.z;
	const float distance = std::sqrt((acrossX * acrossX) + (acrossZ * acrossZ));
	// Nor does a point that is out of reach across the ground
	if (!(distance < source.radius))
	{
		return std::nullopt;
	}
	if (distance <= 0.0f)
	{
		return Bend {.direction = glm::vec3(0.0f), .amount = 0.0f};
	}
	const float radius = source.radius;
	const float amount = 1.0f - (((((radius - k_BendCore) * distance) / radius) + k_BendCore) / radius);
	return Bend {.direction = glm::vec3(acrossX / distance, 0.0f, acrossZ / distance), .amount = amount};
}

} // namespace openblack::nature::tree_bend
