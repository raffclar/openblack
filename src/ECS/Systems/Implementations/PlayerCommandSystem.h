/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <map>
#include <utility>

#include "ECS/Systems/PlayerCommandSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs::systems
{

class PlayerCommandSystem final: public PlayerCommandSystemInterface
{
public:
	void Submit(player_commands::Action action) override;
	void SendOpenBlock() override;
	void RunSent(uint32_t turn) override;
	void Reset() override;
	void SetTurnListener(TurnListener listener) override { _listener = std::move(listener); }
	void Receive(uint32_t turn, std::vector<player_commands::Command> commands) override;
	void SetSender(PlayerNames player) override { _sender = player; }
	[[nodiscard]] PlayerNames GetSender() const override { return _sender; }
	[[nodiscard]] size_t PendingCount() const override { return _queue.OpenCount() + _queue.SentCount(); }
	void SetSource(player_commands::TurnCommandSourceInterface* source) override;
	[[nodiscard]] bool TurnReady(uint32_t turn) const override { return _source == nullptr || _source->HasTurn(turn); }
	[[nodiscard]] bool HasSource() const override { return _source != nullptr; }
	[[nodiscard]] bool IsBehind(uint32_t turn) const override { return _source != nullptr && _source->IsBehind(turn); }

private:
	player_commands::CommandQueue _queue;
	PlayerNames _sender {PlayerNames::PLAYER_ONE};
	TurnListener _listener;
	player_commands::TurnCommandSourceInterface* _source {nullptr};
	/// The commands from other computers, by the turn that does them
	std::map<uint32_t, std::vector<player_commands::Command>> _received;
};

} // namespace openblack::ecs::systems
