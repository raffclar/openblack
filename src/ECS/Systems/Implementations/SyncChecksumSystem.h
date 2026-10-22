/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <memory>

#include <entt/meta/fwd.hpp>

#include "ECS/Systems/SyncChecksumSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs::systems
{

class SyncChecksumSystem final: public SyncChecksumSystemInterface
{
public:
	SyncChecksumSystem();
	~SyncChecksumSystem() override;

	void ProcessTurn() override;
	void SetEnabled(bool enabled) override { _enabled = enabled; }
	[[nodiscard]] bool IsEnabled() const override { return _enabled; }
	[[nodiscard]] std::optional<TurnChecksum> GetLast() const override { return _last; }

private:
	/// The names of the saved components, by which the checksum reads the world
	std::unique_ptr<entt::meta_ctx> _reflection;
	bool _enabled {false};
	std::optional<TurnChecksum> _last;
};

} // namespace openblack::ecs::systems
