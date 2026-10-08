/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

namespace openblack::ecs::systems
{
/// The villager rules a test may switch off
class VillagerRulesInterface
{
public:
	virtual ~VillagerRulesInterface() = default;

	/// Whether a hurt villager goes home (always in the game)
	[[nodiscard]] virtual bool GoHomeEnabled() const = 0;
};
} // namespace openblack::ecs::systems
