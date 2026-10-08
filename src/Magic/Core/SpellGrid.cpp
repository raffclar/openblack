/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "SpellGrid.h"

#include <cstdio>
#include <cstdlib>

#include "ECS/Systems/SpellSystemInterface.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::magic;

namespace
{
/// the MapCoords' cell (x >> 16) >> 3 (8 map cells of 10 m)
bool CellOf(const glm::vec3& position, int& x, int& z)
{
	if (position.x < 0.0f || position.z < 0.0f)
	{
		return false;
	}
	x = static_cast<int>(position.x / 10.0f) >> 3;
	z = static_cast<int>(position.z / 10.0f) >> 3;
	return x < SpellGrid::k_Side && z < SpellGrid::k_Side;
}

/// The game's grid (Locator::spellSystem)
SpellGrid& GameGrid()
{
	if (!Locator::spellSystem::has_value())
	{
		std::fputs("magic::spell_grid: no spell system in the locator (Locator::spellSystem)\n", stderr);
		std::abort();
	}
	return Locator::spellSystem::value().Grid();
}
} // namespace

void SpellGrid::Mark(const glm::vec3& position, uint8_t value)
{
	int x = 0;
	int z = 0;
	if (CellOf(position, x, z))
	{
		_cells[static_cast<size_t>(x)][static_cast<size_t>(z)] = value;
	}
}

void SpellGrid::Decay()
{
	for (auto& column : _cells)
	{
		for (auto& cell : column)
		{
			if (cell != 0)
			{
				cell = cell <= _decay ? 0 : static_cast<uint8_t>(cell - _decay);
			}
		}
	}
	_decay = k_StartDecay;
}

uint8_t SpellGrid::At(const glm::vec3& position) const
{
	int x = 0;
	int z = 0;
	return CellOf(position, x, z) ? _cells[static_cast<size_t>(x)][static_cast<size_t>(z)] : 0;
}

void SpellGrid::Clear()
{
	for (auto& column : _cells)
	{
		column.fill(0);
	}
	_decay = k_StartDecay;
}

void spell_grid::Mark(const glm::vec3& position, uint8_t value)
{
	GameGrid().Mark(position, value);
}

void spell_grid::Decay()
{
	GameGrid().Decay();
}

uint8_t spell_grid::At(const glm::vec3& position)
{
	return GameGrid().At(position);
}

void spell_grid::Clear()
{
	GameGrid().Clear();
}
