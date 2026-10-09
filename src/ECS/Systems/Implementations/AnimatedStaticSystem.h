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

#include <optional>

#include "ECS/Systems/AnimatedStaticSystemInterface.h"

namespace openblack::ecs::systems
{

class AnimatedStaticSystem final: public AnimatedStaticSystemInterface
{
public:
	bool SetOpenState(entt::entity object, int32_t openState) override;
	[[nodiscard]] std::optional<uint32_t> GateStoneValue(entt::entity object) const override;
	bool LayGateStone(entt::entity plinth, entt::entity stone) override;
	void Update(uint32_t turn, float turnFraction) override;
	void Reset() override;

private:
	/// The game's clock when the scenery was last drawn; none before the first frame of a level
	std::optional<uint32_t> _drawTime;
};

} // namespace openblack::ecs::systems
