/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "LandBalanceSystem.h"

using namespace openblack::ecs::systems;

void LandBalanceSystem::Reset()
{
	_values.fill(1.0f);
	_lostTownScale = 1.0f;
}

void LandBalanceSystem::Set(int index, float value)
{
	if (index >= 0 && static_cast<std::size_t>(index) < _values.size())
	{
		_values.at(static_cast<std::size_t>(index)) = value;
	}
}

float LandBalanceSystem::Get(std::size_t index) const
{
	return index < _values.size() ? _values.at(index) : 1.0f;
}

void LandBalanceSystem::SetLostTownScale(float scale)
{
	_lostTownScale = scale;
}

float LandBalanceSystem::LostTownScale() const
{
	return _lostTownScale;
}
