/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "SpellSystem.h"

using namespace openblack::ecs::systems;
using openblack::ecs::components::SpellClass;

std::vector<entt::entity>& SpellSystem::Spells()
{
	return _spells;
}

void SpellSystem::SetSink(entt::entity spell, std::unique_ptr<psys::SpellSink> sink)
{
	_sinks[spell] = std::move(sink);
}

void SpellSystem::EraseSink(entt::entity spell)
{
	_sinks.erase(spell);
}

void SpellSystem::Clear()
{
	_spells.clear();
	_sinks.clear();
}

openblack::magic::SpellOps& SpellSystem::Ops(SpellClass spellClass)
{
	return _ops[static_cast<size_t>(spellClass)];
}

bool SpellSystem::TakeOpsRegistration()
{
	if (_opsRegistered)
	{
		return false;
	}
	_opsRegistered = true;
	return true;
}

bool SpellSystem::TakeNotPortedWarning(SpellClass spellClass)
{
	auto& warned = _notPortedWarned[static_cast<size_t>(spellClass)];
	if (warned)
	{
		return false;
	}
	warned = true;
	return true;
}

openblack::magic::SpellGrid& SpellSystem::Grid()
{
	return _grid;
}

std::vector<entt::entity>& SpellSystem::ShieldSpells()
{
	return _shieldSpells;
}

std::vector<entt::entity>& SpellSystem::StormSpells()
{
	return _stormSpells;
}
