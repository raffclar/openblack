/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <array>

#include <entt/entity/entity.hpp>

#include "ECS/Systems/HelpMessageSystemInterface.h"
#include "Help/HelpQuery.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs::systems
{

class HelpMessageSystem final: public HelpMessageSystemInterface
{
public:
	bool RunMessage(uint32_t first, uint32_t last, help_messages::Script script,
	                std::optional<std::pair<uint32_t, float>> townNeed = std::nullopt) override;
	bool RunMessageSet(size_t set, std::optional<help_messages::Texts> thing = std::nullopt) override;
	void TriggerCategory(uint32_t category) override;
	void HelpQuery() override;
	void ProcessTurn() override;

	[[nodiscard]] const help_messages::SetCounts& GetSentCounts() const override { return _sent; }
	[[nodiscard]] const std::vector<Started>& GetStarted() const override { return _started; }

private:
	/// Says the Help key's answer; a set that says a thing's texts says these
	void Say(const help_query::Answer& answer, std::optional<help_messages::Texts> thing);
	void Remember(Started started);

	/// Each set's count of being said, and the turn it last was; each category's turn
	help_messages::SetCounts _sent {};
	help_messages::SetCounts _sentTurn {};
	std::array<uint32_t, 16> _categoryTurn {};
	/// The banter sets used since they were last all cleared
	uint16_t _banterUsed {0};
	/// The last set said, and the turn the last message started
	std::optional<size_t> _lastSet;
	uint32_t _messageTurn {0};
	std::vector<Started> _started;
};

} // namespace openblack::ecs::systems
