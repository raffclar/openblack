/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <entt/fwd.hpp>

#include "ECS/Components/Alignment.h"
#include "ECS/Components/PlayerMagic.h"
#include "Enums.h"

namespace openblack::ecs::systems
{
/// The players: their entities, every player's alignment, and the magic of the players without an entity.
/// The alignment lasts the whole game and is kept when another land loads, but a player's entity is made again on each
/// land, so the alignment cannot be a component on that entity: the service keeps it for every player, the neutral one
/// included. A player with an entity has its PlayerMagic on it; the others (the neutral player, a script player not
/// made yet) use the service's, which a land load clears.
class PlayerSystemInterface
{
public:
	virtual ~PlayerSystemInterface() = default;

	virtual void RegisterPlayers() = 0;
	virtual void AddPlayer(entt::entity playerEntity) = 0;
	[[nodiscard]] virtual entt::entity GetPlayer(PlayerNames name) const = 0;
	/// The player at this machine's interface: whose hand it is, whose creature it leads and whose fights it watches
	[[nodiscard]] virtual PlayerNames LocalPlayer() const = 0;
	/// A land is loaded: its player entities went with the registry, so none is listed until the new land makes them
	virtual void ClearPlayers() = 0;

	/// Kept across lands. A name out of range is the neutral player's
	[[nodiscard]] virtual components::Alignment& Alignment(PlayerNames name) = 0;
	/// The magic of a player without an entity. A name out of range is the neutral player's
	[[nodiscard]] virtual components::PlayerMagic& MagicWithoutEntity(PlayerNames name) = 0;
	/// A land is loaded: the magic of the players without an entity back to its start values (the alignment stays)
	virtual void ClearMagicWithoutEntity() = 0;
};
} // namespace openblack::ecs::systems
