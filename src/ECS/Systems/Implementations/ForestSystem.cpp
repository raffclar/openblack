/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "ForestSystem.h"

#include <algorithm>

using namespace openblack::ecs::systems;

uint32_t ForestSystem::Create(uint32_t id, glm::vec3 centre)
{
	if (id == 0)
	{
		while (_forests.contains(_nextId))
		{
			++_nextId;
		}
		id = _nextId++;
	}
	else
	{
		_nextId = std::max(_nextId, id + 1);
	}
	_forests.insert_or_assign(id, ForestData {centre, 0, 0, ++_created});
	return id;
}

ForestSystem::Forests& ForestSystem::All()
{
	return _forests;
}

ForestSystem::TownForestLists& ForestSystem::TownLists()
{
	return _townLists;
}

uint32_t ForestSystem::LastTreeCreatedTurn() const
{
	return _lastTreeCreatedTurn;
}

void ForestSystem::SetLastTreeCreatedTurn(uint32_t turn)
{
	_lastTreeCreatedTurn = turn;
}

void ForestSystem::NoteForestLostAMagicTree(uint32_t forestId)
{
	_forestsThatLostAMagicTree.push_back(forestId);
}

bool ForestSystem::TakeForestLostAMagicTree(uint32_t forestId)
{
	if (std::ranges::find(_forestsThatLostAMagicTree, forestId) == _forestsThatLostAMagicTree.end())
	{
		return false;
	}
	std::erase(_forestsThatLostAMagicTree, forestId);
	return true;
}

void ForestSystem::ClearForestsThatLostAMagicTree()
{
	_forestsThatLostAMagicTree.clear();
}

void ForestSystem::Clear()
{
	_townLists.clear();
	_forests.clear();
	_nextId = 1;
	_lastTreeCreatedTurn = 0;
}
