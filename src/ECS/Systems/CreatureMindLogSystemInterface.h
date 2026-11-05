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

#include <entt/entity/fwd.hpp>

#include "Creature/CreatureMindView.h"

namespace openblack::ecs
{
class Registry;
}

namespace openblack::ecs::systems
{

/// The record of the minds of the creatures that the developer view watches (see creature_mind_view::Trace). It is there
/// only in builds with the developer tools. It records only the creatures that a tool watches, so with no watched
/// creature it costs nothing. The game turns never read it, so it does not change the world.
class CreatureMindLogSystemInterface
{
public:
	virtual ~CreatureMindLogSystemInterface() = default;

	/// Starts or stops the record of a creature. A stop removes the record of the creature.
	virtual void Watch(entt::entity creature, bool watch) = 0;
	[[nodiscard]] virtual bool IsWatched(entt::entity creature) const = 0;
	/// Reads the minds of the watched creatures, as the turn left them. A watched creature that has no mind any more
	/// stops its record.
	virtual void TurnEnded(const Registry& registry, uint32_t turn) = 0;
	/// The record of a creature, or none when no tool watches it
	[[nodiscard]] virtual const creature_mind_view::Trace* Find(entt::entity creature) const = 0;
	/// Stops the records of all the creatures
	virtual void Clear() = 0;
};

} // namespace openblack::ecs::systems
