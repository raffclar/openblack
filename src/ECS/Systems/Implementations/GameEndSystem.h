/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <utility>

#include "ECS/GameEndConditions.h"
#include "ECS/Systems/GameEndSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs::systems
{

class GameEndSystem final: public GameEndSystemInterface
{
public:
	void ProcessTurn(uint32_t turn) override;
	[[nodiscard]] const std::vector<game_end::Event>& LastEvents() const override { return _events; }
	[[nodiscard]] bool IsDecided() const override;
	[[nodiscard]] std::vector<game_end::Event> TakeEventsToShow() override { return std::exchange(_eventsToShow, {}); }
	[[nodiscard]] std::optional<PlayerNames> LastGodIn() const override;
	[[nodiscard]] std::vector<GodStanding> Standings() const override;
	void SetTimeLimit(uint32_t minutes) override { _timeLimitMinutes = minutes; }
	[[nodiscard]] uint32_t GetTimeLimit() const override;
	void AddToCondition(std::optional<PlayerNames> player, game_end::Condition condition, int32_t amount) override;
	void MeasureConditions(PlayerNames player) override;
	void BuildingWorks(entt::entity building) override;

private:
	/// The time limit that the game server sent. None: the time limit of a test switch, or the default time limit.
	std::optional<uint32_t> _timeLimitMinutes;
	std::vector<game_end::Event> _events;
	/// The events that the view did not take yet
	std::vector<game_end::Event> _eventsToShow;
};

} // namespace openblack::ecs::systems
