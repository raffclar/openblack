/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <functional>

#include "ECS/Systems/DebugCheatSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs
{
class Registry;
}

namespace openblack::ecs::systems
{

class DebugCheatSystem final: public DebugCheatSystemInterface
{
public:
	/// The registry of the world. The game gives the registry of the Locator, and the tests give their own.
	using RegistryOf = std::function<Registry*()>;

	DebugCheatSystem();
	explicit DebugCheatSystem(RegistryOf registry);

	bool Set(PlayerNames player, debug_cheats::Cheat cheat, bool on) override;
	[[nodiscard]] uint32_t CheatsOf(PlayerNames player) const override;
	void ProcessTurn() override;

private:
	RegistryOf _registry;
};

} // namespace openblack::ecs::systems
