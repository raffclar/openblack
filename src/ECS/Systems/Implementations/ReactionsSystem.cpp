/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "ReactionsSystem.h"

#include <algorithm>

namespace openblack::ecs::systems
{
// Inside the namespace, so these win over openblack's own Reaction enum
using effects::reactions::LivingClass;
using effects::reactions::LivingReactionHandler;
using effects::reactions::LivingShutDownHandler;
using effects::reactions::Reaction;

uint32_t ReactionsSystem::Add(Reaction reaction)
{
	reaction.id = _nextId++;
	_reactions.push_back(reaction);
	return reaction.id;
}

Reaction* ReactionsSystem::Find(uint32_t id)
{
	if (id == 0)
	{
		return nullptr;
	}
	const auto it = std::ranges::find(_reactions, id, &Reaction::id);
	return it != _reactions.end() ? &*it : nullptr;
}

std::vector<Reaction>& ReactionsSystem::List()
{
	return _reactions;
}

const std::vector<Reaction>& ReactionsSystem::List() const
{
	return _reactions;
}

void ReactionsSystem::Erase(std::span<const uint32_t> ids)
{
	std::erase_if(_reactions, [ids](const Reaction& reaction) { return std::ranges::find(ids, reaction.id) != ids.end(); });
}

void ReactionsSystem::SetInTurn(bool inTurn)
{
	_inTurn = inTurn;
}

bool ReactionsSystem::InTurn() const
{
	return _inTurn;
}

uint32_t ReactionsSystem::NextJoinOrder()
{
	return ++_joinOrder;
}

void ReactionsSystem::ResetJoinOrder()
{
	_joinOrder = 0;
}

void ReactionsSystem::SetReactionHandler(LivingClass living, LivingReactionHandler handler)
{
	_handlers.at(static_cast<std::size_t>(living)) = handler;
}

LivingReactionHandler ReactionsSystem::ReactionHandler(LivingClass living) const
{
	return _handlers.at(static_cast<std::size_t>(living));
}

void ReactionsSystem::SetShutDownHandler(LivingClass living, LivingShutDownHandler handler)
{
	_shutDownHandlers.at(static_cast<std::size_t>(living)) = handler;
}

std::span<const LivingShutDownHandler> ReactionsSystem::ShutDownHandlers() const
{
	return _shutDownHandlers;
}

void ReactionsSystem::Clear()
{
	_reactions.clear();
	_nextId = 1;
	_inTurn = false;
}
} // namespace openblack::ecs::systems
