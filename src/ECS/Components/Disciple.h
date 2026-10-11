/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <entt/entity/entity.hpp>

#include "Enums.h"

namespace openblack::ecs::components
{

/// A villager the player's hand has set to a work, which it keeps at until set to another
struct Disciple
{
	VillagerDisciple type {VillagerDisciple::None};
	/// A trader's: the town whose storage pit it trades with
	entt::entity tradeTown {entt::null};
	/// A trader's: what it is carrying to that town this trip
	ResourceType tradeResource {ResourceType::Food};
};

/// A missionary preaching in a town: it follows the missionary about, and the villagers of the town near it come to
/// believe in the player who sent it. It goes when the missionary stops preaching.
struct MissionaryControl
{
	entt::entity villager {entt::null};
	/// The town it preaches in
	entt::entity town {entt::null};
	/// The player it wins belief for: the one whose villager it was
	PlayerNames player {PlayerNames::NEUTRAL};
	/// Its reaction, which impresses the villagers about it
	uint32_t reaction {0};
};

/// A villager picked up by the player's hand, until it has landed and set about its work
struct DiscipleHeld
{
	/// The work it had as it was picked up, which it goes back to when let go near nothing
	VillagerDisciple atPickUp {VillagerDisciple::None};
	/// Its first turn on the ground has been dealt with
	bool touchedDown {false};
};

/// What a disciple put down walks up to and looks at before it decides
struct DiscipleInspect
{
	entt::entity thing {entt::null};
};

/// A sprite drawn over a disciple: its glow, or the symbol of its work shown in or under the hand
struct DiscipleMark
{
	entt::entity villager {entt::null};
	bool symbol {false};
};

} // namespace openblack::ecs::components
