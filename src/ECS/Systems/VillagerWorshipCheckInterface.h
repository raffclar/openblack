/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <entt/entity/fwd.hpp>

namespace openblack::ecs::systems
{
/// The worship check of an idle villager's decision
class VillagerWorshipCheckInterface
{
public:
	virtual ~VillagerWorshipCheckInterface() = default;

	/// Sends the villager to worship when its town needs worshippers; whether it went
	[[nodiscard]] virtual bool WorshipCheck(entt::entity villager) = 0;
};
} // namespace openblack::ecs::systems
