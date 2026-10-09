/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include "ECS/Systems/CreatureFizzSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs::systems
{

class CreatureFizzSystem final: public CreatureFizzSystemInterface
{
public:
	void SetFizz(entt::entity creature, float target, float seconds, bool goesForGood) override;
	void ProcessTurn() override;
	[[nodiscard]] float FizzOf(entt::entity creature) const override;
};

} // namespace openblack::ecs::systems
