/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <map>
#include <optional>

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
	[[nodiscard]] std::array<std::optional<glm::vec3>, static_cast<size_t>(Side::_Count)>
	GetHandPositionsOf(PlayerNames player) const noexcept override;
	[[nodiscard]] std::array<std::optional<glm::vec3>, static_cast<size_t>(Side::_Count)>
	GetOwnHandPositions() const noexcept override;
	void UpdateAlignmentMorph(std::optional<map_coords::MapCoords> picked, bool holdPoint) override;
	[[nodiscard]] std::optional<glm::vec3> GetDrawnHandPosition() const noexcept override;
	[[nodiscard]] std::optional<std::array<entt::entity, static_cast<size_t>(Side::_Count)>>
	GetHandsOf(PlayerNames player) const noexcept override;
	std::array<entt::entity, static_cast<size_t>(Side::_Count)> MakeHandsOf(PlayerNames player) override;
	void SetOwnPlayer(PlayerNames player) override { _ownPlayer = player; }
	void ClearOwnPlayer() override { _ownPlayer.reset(); }
	[[nodiscard]] PlayerNames GetOwnPlayer() const noexcept override { return _ownPlayer.value_or(PlayerNames::PLAYER_ONE); }
	[[nodiscard]] bool IsOwnPlayer(PlayerNames player) const noexcept override { return _ownPlayer == player; }
	[[nodiscard]] std::optional<ReportedCamera> GetReportedCamera(PlayerNames player) const noexcept override;
	void Report(PlayerNames player, const player_commands::HandReport& report) override;
	void ReportPick(PlayerNames player, entt::entity object) override;
	void ReportView(PlayerNames player, const player_commands::ViewReport& report) override;
	[[nodiscard]] std::optional<ReportedView> GetReportedView(PlayerNames player) const noexcept override;
	[[nodiscard]] entt::entity GetReportedPick() const noexcept override;
	void ProcessTurn() override;
	[[nodiscard]] uint32_t GetHandStillTurns() const noexcept override;
	[[nodiscard]] glm::vec3 GetReportedHandVelocity() const noexcept override;
	[[nodiscard]] hand_trace::HandTrace* GetTrace() noexcept override { return &_trace; }

private:
	std::array<entt::entity, 2> _hands {entt::null, entt::null};
	/// The hands of the gods of the other players of a network game, by player
	std::map<PlayerNames, std::array<entt::entity, 2>> _godHands;
	/// None on the game server and on an observer's computer
	std::optional<PlayerNames> _ownPlayer {PlayerNames::PLAYER_ONE};
	hand_trace::HandTrace _trace;
};
} // namespace openblack::ecs::systems
