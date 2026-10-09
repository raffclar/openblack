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
#include <vector>

#include "ECS/Systems/TempleDestructionSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "Locator interface implementations should only be included in Locator.cpp, use interface instead."
#endif

namespace openblack::ecs::systems
{

class TempleDestructionSystem final: public TempleDestructionSystemInterface
{
public:
	void Start(entt::entity temple) override;
	void ProcessTurn() override;
	void EndTurn() override;

private:
	/// The loops sounding over temples being destroyed, by temple, stopped when their temple goes another way
	std::vector<std::pair<entt::entity, entt::entity>> _loops;
};

} // namespace openblack::ecs::systems
