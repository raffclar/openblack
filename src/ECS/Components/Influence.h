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

#include "Common/VirtualInfluence.h"
#include "Enums.h"

namespace openblack::ecs::components
{

/// Influence a player has round something that is neither a town nor a citadel, reaching so far across the land. The
/// testbed gives its player influence this way, as it has no temple, so that the miracles cast only in influence may be
/// cast there.
struct InfluenceSource
{
	PlayerNames player {PlayerNames::PLAYER_ONE};
	float radius {0.0f};
	/// Anti-influence, which a script makes: the player has no influence at all within it
	bool anti {false};
	/// The object it goes about with, which a script made it on; none for one that stays where it is
	entt::entity follows {entt::null};
};

/// How far a town's influence reaches, worked out each turn from its own and its buildings', and how far it reached
/// when its border was last drawn
struct TownInfluence
{
	float radius {0.0f};
	float drawnRadius {0.0f};
};

/// How far a citadel's influence reaches before the land's multiplier, fixed when it is first asked for, and how far it
/// reached when its border was last drawn
struct CitadelInfluence
{
	float reach {0.0f};
	float drawnRadius {0.0f};
};

/// What the player's hand keeps of their influence past the border, on the player's entity, and the hum it makes there
struct VirtualInfluence
{
	virtual_influence::State state;
	entt::entity hum {entt::null};
};

} // namespace openblack::ecs::components
