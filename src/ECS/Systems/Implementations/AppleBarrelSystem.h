/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#ifndef LOCATOR_IMPLEMENTATIONS
#error "Locator interface implementations should only be included in Locator.cpp"
#endif

#include <cstdint>

#include <optional>

#include "ECS/Systems/AppleBarrelSystemInterface.h"

namespace openblack::ecs::systems
{

class AppleBarrelSystem final: public AppleBarrelSystemInterface
{
public:
	void Update(std::chrono::milliseconds frameGameTime) override;
	[[nodiscard]] entt::entity GetBarrel() const override { return _barrel; }
	void Reset() override;

private:
	/// The storage pit that carries the barrel now, none while none does
	[[nodiscard]] std::optional<entt::entity> CarryingStore() const;
	void Remove();

	entt::entity _barrel {entt::null};
	/// The worm's place in its clip, in milliseconds: one worm for the whole game, so it carries on from land to land
	uint32_t _wormPlace {0};
};

} // namespace openblack::ecs::systems
