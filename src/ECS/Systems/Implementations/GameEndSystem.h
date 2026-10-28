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

private:
	std::vector<game_end::Event> _events;
	/// The events that the view did not take yet
	std::vector<game_end::Event> _eventsToShow;
};

} // namespace openblack::ecs::systems
