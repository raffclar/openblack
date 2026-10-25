/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

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

private:
	std::vector<game_end::Event> _events;
};

} // namespace openblack::ecs::systems
