/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include "ECS/Systems/CreatureHelpSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs::systems
{

class CreatureHelpSystem final: public CreatureHelpSystemInterface
{
public:
	void Raise(entt::entity creature, const creature_help::Message& message) override;
	void ProcessTurn() override;
	void PromptKnown(entt::entity creature, creature_help::Prompt prompt) override;
	void Fainting(entt::entity creature) override;

	void SetHelpOn(bool on) override { _helpOn = on; }
	[[nodiscard]] bool IsHelpOn() const override { return _helpOn; }

	[[nodiscard]] const std::vector<Told>& GetTold() const override { return _told; }

private:
	/// The help's watching of one creature this turn
	void UpdateCreature(entt::entity creature);
	void Tell(entt::entity creature, const creature_help::Message& message);

	bool _helpOn {true};
	/// The game turn a message about a creature was last told: one a turn at most (none in the first turn)
	uint32_t _toldTurn {0};
	std::vector<Told> _told;
};

} // namespace openblack::ecs::systems
