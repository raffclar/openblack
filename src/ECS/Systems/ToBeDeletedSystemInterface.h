/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <entt/entity/fwd.hpp>

namespace openblack::ecs::systems
{
/// The game's dead list: the objects marked unavailable that wait to be freed, and whether deletions wait on it at all
/// (ecs::ToBeDeleted and ecs::ProcessDeadList go through it)
class ToBeDeletedSystemInterface
{
public:
	virtual ~ToBeDeletedSystemInterface() = default;

	/// Puts an entity just marked unavailable at the head of the dead list, not yet through a pass
	virtual void Push(entt::entity entity) = 0;
	/// From the head, each entity marked before this pass: the first pass only notes it, the next one frees it. `drain`
	/// frees them all, again until the list is empty
	virtual void Process(bool drain) = 0;
	/// Whether a deletion marks the entity and waits for the list (true) or destroys it at once (false)
	virtual void SetDeferred(bool deferred) = 0;
	[[nodiscard]] virtual bool Deferred() const = 0;
};
} // namespace openblack::ecs::systems
