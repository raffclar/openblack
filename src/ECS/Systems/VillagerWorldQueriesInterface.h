/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <optional>

#include <entt/entity/fwd.hpp>

namespace openblack::ecs::systems
{
/// What the villagers read of the world around them: the time of day and the town's graveyard
class VillagerWorldQueriesInterface
{
public:
	virtual ~VillagerWorldQueriesInterface() = default;

	/// Whether the sky shows night (day without a game)
	[[nodiscard]] virtual bool IsVisualNight() const = 0;
	/// The town's graveyard: nullopt without one, else whether it is functional
	[[nodiscard]] virtual std::optional<bool> Graveyard(entt::entity town) const = 0;
};
} // namespace openblack::ecs::systems
