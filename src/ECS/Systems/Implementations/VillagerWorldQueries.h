/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include "ECS/Systems/VillagerWorldQueriesInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "Locator interface implementations should only be included in Locator.cpp, use interface instead."
#endif

namespace openblack::ecs::systems
{
/// The game's day / night clock and the towns' graveyards
class VillagerWorldQueries final: public VillagerWorldQueriesInterface
{
public:
	[[nodiscard]] bool IsVisualNight() const override;
	[[nodiscard]] std::optional<bool> Graveyard(entt::entity town) const override;
};
} // namespace openblack::ecs::systems
