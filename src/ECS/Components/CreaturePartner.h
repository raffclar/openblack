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

#include <entt/entity/entity.hpp>

namespace openblack::ecs::components
{

/// The other creature of a joint action. Both creatures of the joint action have this, each with the other as its
/// partner. A creature with a partner is not free for another joint action. When the agenda of either creature ends or
/// changes, the joint action stops for both.
struct CreaturePartner
{
	entt::entity partner {entt::null};
	/// The agenda of this creature that the joint action runs with, by the count of its agendas, and whether that agenda
	/// had steps left when it was noted. The joint action stops when the creature takes up another agenda, or when an
	/// agenda that had steps left has none.
	uint32_t agenda {0};
	bool agendaRunning {false};
};

/// The asker of a joint action made its partner stand still in its mind: the partner does not update its desires or
/// choose what to do. It still carries out the orders of the asker.
struct CreatureDemobilised
{
};

} // namespace openblack::ecs::components
