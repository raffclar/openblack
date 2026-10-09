/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <vector>

#include "Creature/CreatureCarryOver.h"
#include "ECS/Systems/CreatureCarryOverSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs::systems
{

class CreatureCarryOverSystem final: public CreatureCarryOverSystemInterface
{
public:
	void KeepPlayersCreature() override;
	void Keep(std::shared_ptr<const creaturemind::MindFileData> file) override;
	[[nodiscard]] std::shared_ptr<const creaturemind::MindFileData> Kept() const override;
	std::optional<entt::entity> LoadPlayersCreature(glm::vec2 place) override;
	void ProcessTurn() override;
	void Reset() override;

private:
	struct Arriving
	{
		entt::entity creature;
		creature_carry_over::Fizz fizz;
	};

	std::shared_ptr<const creaturemind::MindFileData> _kept;
	std::vector<Arriving> _arriving;
};

} // namespace openblack::ecs::systems
