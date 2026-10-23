/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <entt/entity/entity.hpp>

#include "Commands/PlayerCommands.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "Hand/HandTrace.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "Locator interface implementations should only be included in Locator.cpp, use interface instead."
#endif

namespace openblack::ecs::systems
{

class HandSystem final: public HandSystemInterface
{
public:
	bool Initialize() noexcept override;
	void FindAfterLoad() override;
	[[nodiscard]] std::array<entt::entity, static_cast<size_t>(Side::_Count)> GetPlayerHands() const noexcept override;
	[[nodiscard]] std::array<std::optional<glm::vec3>, static_cast<size_t>(Side::_Count)>
	GetPlayerHandPositions() const noexcept override;
	void UpdateAlignmentMorph(std::optional<map_coords::MapCoords> picked, bool holdPoint) override;
	[[nodiscard]] std::optional<glm::vec3> GetDrawnHandPosition() const noexcept override;
	void Report(PlayerNames player, const player_commands::HandReport& report) override;
	void ProcessTurn() override;
	[[nodiscard]] uint32_t GetHandStillTurns() const noexcept override;
	[[nodiscard]] hand_trace::HandTrace* GetTrace() noexcept override { return &_trace; }

private:
	std::array<entt::entity, 2> _hands {entt::null, entt::null};
	hand_trace::HandTrace _trace;
};
} // namespace openblack::ecs::systems
