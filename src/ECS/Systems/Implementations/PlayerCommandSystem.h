/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

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
	void RunSent() override;
	void Reset() override;
	void SetSender(PlayerNames player) override { _sender = player; }
	[[nodiscard]] PlayerNames GetSender() const override { return _sender; }
	[[nodiscard]] size_t PendingCount() const override { return _queue.OpenCount() + _queue.SentCount(); }

private:
	player_commands::CommandQueue _queue;
	PlayerNames _sender {PlayerNames::PLAYER_ONE};
};

} // namespace openblack::ecs::systems
