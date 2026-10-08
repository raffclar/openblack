/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "ObjectCreationIndexSystem.h"

#include <algorithm>

#include "ECS/ObjectCreationIndex.h"

using namespace openblack::ecs;
using namespace openblack::ecs::systems;

namespace
{
/// The most spell icons a town centre shows
constexpr uint32_t k_MaxIcons = 6;
} // namespace

void ObjectCreationIndexSystem::OnLoadMap()
{
	_counter = _loaded ? 0 : 2;
	_loaded = true;
	_towns.clear();
}

uint32_t ObjectCreationIndexSystem::Next()
{
	return _counter++;
}

void ObjectCreationIndexSystem::Skip(uint32_t count)
{
	_counter += count;
}

void ObjectCreationIndexSystem::AddTownSpell(uint32_t town, const std::string& spell)
{
	auto& spells = _towns[town];
	if (std::ranges::find(spells.seeds, spell) != spells.seeds.end())
	{
		return;
	}
	spells.seeds.push_back(spell);
	if (spells.centre && spells.icons < k_MaxIcons)
	{
		++spells.icons;
		Skip(1);
	}
}

void ObjectCreationIndexSystem::OnTownCentre(uint32_t town)
{
	auto& spells = _towns[town];
	spells.centre = true;
	const auto icons = std::min<uint32_t>(static_cast<uint32_t>(spells.seeds.size()), k_MaxIcons);
	spells.icons = icons;
	Skip(icons);
}
