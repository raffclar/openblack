/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <entt/entity/entity.hpp>

#include "ECS/Components/GodHand.h"
#include "ECS/Registry.h"
#include "Enums.h"

namespace openblack::ecs
{

/// The player whose hand it is. The hands of the other gods have the mark of their god. The hands of the first player
/// have no mark.
[[nodiscard]] inline PlayerNames HandPlayer(const Registry& registry, entt::entity hand)
{
	const auto* god = registry.Valid(hand) ? registry.TryGet<const components::GodHand>(hand) : nullptr;
	return god != nullptr ? god->player : PlayerNames::PLAYER_ONE;
}

} // namespace openblack::ecs
