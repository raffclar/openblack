/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "AnimalAISystem.h"

using namespace openblack::ecs::systems;

float AnimalAISystem::VisualTime() const
{
	return _visualTime;
}

void AnimalAISystem::SetVisualTime(float hours)
{
	_visualTime = hours;
}

uint32_t AnimalAISystem::AddDeathListener(DeathCallback callback)
{
	const uint32_t id = _nextListenerId++;
	_deathListeners.emplace_back(id, std::move(callback));
	return id;
}

void AnimalAISystem::RemoveDeathListener(uint32_t id)
{
	std::erase_if(_deathListeners, [id](const auto& listener) { return listener.first == id; });
}

const AnimalAISystem::DeathListeners& AnimalAISystem::GetDeathListeners() const
{
	return _deathListeners;
}

uint32_t AnimalAISystem::SingleSlotId() const
{
	return _singleSlotId;
}

void AnimalAISystem::SetSingleSlotId(uint32_t id)
{
	_singleSlotId = id;
}

void AnimalAISystem::SetSpeciesDying(std::size_t species, DeathCallback dying)
{
	if (species >= _speciesDying.size())
	{
		_speciesDying.resize(species + 1);
	}
	_speciesDying[species] = std::move(dying);
}

const AnimalAISystem::DeathCallback* AnimalAISystem::SpeciesDying(std::size_t species) const
{
	return species < _speciesDying.size() && _speciesDying[species] ? &_speciesDying[species] : nullptr;
}
