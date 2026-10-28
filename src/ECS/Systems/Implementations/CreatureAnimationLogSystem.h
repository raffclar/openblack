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

#include "ECS/Systems/CreatureAnimationLogSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs::systems
{

class CreatureAnimationLogSystem final: public CreatureAnimationLogSystemInterface
{
public:
	void Posed(entt::entity creature, uint32_t turn, std::span<const creature_animation_log::Clip> clips,
	           const creature_animation_log::Cause& cause) override;
	void Note(entt::entity creature, creature_animation_log::ProblemEntry problem) override;
	void Refused(entt::entity creature, creature_animation_log::Layer layer, size_t animation, std::string_view by,
	             size_t playing) override;
	[[nodiscard]] const creature_animation_log::Log* Find(entt::entity creature) const override;
	void Clear() override;

private:
	[[nodiscard]] creature_animation_log::Log& LogOf(entt::entity creature);

	std::unordered_map<entt::entity, creature_animation_log::Log> _logs;
};

} // namespace openblack::ecs::systems
