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

#include "ECS/Town/TownBelief.h"

namespace openblack::ecs::events
{
/// A town's belief asks for a help sprite
struct TownBeliefHelp
{
	town_belief::detail::HelpSprite kind {};
	entt::entity town {entt::null}; ///< the town the sprite is about
};

/// The local player's gained believers ask for a tooltip
struct TownBeliefToolTip
{
	uint32_t text {0};
	float value {0.0f};
};
} // namespace openblack::ecs::events
