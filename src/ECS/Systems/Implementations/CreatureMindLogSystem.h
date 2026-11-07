/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <unordered_map>

#include "ECS/Systems/CreatureMindLogSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs::systems
{

class CreatureMindLogSystem final: public CreatureMindLogSystemInterface
{
public:
	void Watch(entt::entity creature, bool watch) override;
	[[nodiscard]] bool IsWatched(entt::entity creature) const override;
	void TurnEnded(const Registry& registry, uint32_t turn) override;
	[[nodiscard]] const creature_mind_view::Trace* Find(entt::entity creature) const override;
	void Clear() override;

private:
	std::unordered_map<entt::entity, creature_mind_view::Trace> _traces;
};

} // namespace openblack::ecs::systems
