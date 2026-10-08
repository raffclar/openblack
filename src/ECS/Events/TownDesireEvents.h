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

#include "ECS/Town/TownDesire.h"

namespace openblack::ecs::events
{
/// A town's desires ask for a guidance warning (unhappy villagers, low on food or wood)
struct TownDesireWarning
{
	entt::entity town {entt::null};
	town_desire::Warning warning {};
	float value {0.0f};
};
} // namespace openblack::ecs::events
