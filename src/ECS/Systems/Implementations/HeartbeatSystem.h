/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include "ECS/Systems/HeartbeatSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs::systems
{

class HeartbeatSystem final: public HeartbeatSystemInterface
{
public:
	void ProcessTurn(uint32_t turn) override;
	[[nodiscard]] float Pulse(PlayerNames player, float turnFraction) const override;
	[[nodiscard]] std::optional<heartbeat::State> StateOf(PlayerNames player) const override;
	void KeepForNextLand() override;
	void ForgetKept() override;
	void EndDestruction(PlayerNames player) override;

private:
	heartbeat::KeptHearts _kept;
};

} // namespace openblack::ecs::systems
