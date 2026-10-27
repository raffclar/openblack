/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <optional>

#include <glm/vec3.hpp>

#include "ComputerGameHandCasts.h"
#include "ComputerGameHandCreature.h"
#include "ComputerGameHandMoves.h"
#include "ComputerGameHandObjects.h"
#include "ComputerGameHandTotem.h"
#include "ComputerMindRuleTable.h"
#include "ECS/Components/ComputerPlayer.h"
#include "ECS/ComputerHand.h"
#include "ECS/ComputerMind.h"
#include "Enums.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "Locator interface implementations should only be included in Locator.cpp, use interface instead."
#endif

/// The mind and the hand of a computer god in the game's own world
namespace openblack::ecs::systems::computer_world
{

/// The rules of the god's needs in the game's world. Each node finds its rules in the table of rules by its kind.
class GameMindWorld final: public computer_mind::World
{
public:
	GameMindWorld(PlayerNames player, components::ComputerPlayer& state);
	/// The parts of the hand refer to the parts of this world. Thus the code does not copy or move this world.
	GameMindWorld(const GameMindWorld&) = delete;
	GameMindWorld& operator=(const GameMindWorld&) = delete;
	GameMindWorld(GameMindWorld&&) = delete;
	GameMindWorld& operator=(GameMindWorld&&) = delete;
	~GameMindWorld() override = default;

	float Intensity(computer_mind::Mind& mind, uint16_t node) override;
	bool Perform(computer_mind::Mind& mind, uint16_t node) override;
	[[nodiscard]] bool ActionBusy() const override;
	bool StepAction() override;
	void ClearAction() override;
	void StartAction() override;
	[[nodiscard]] bool ActionLocked() const override;
	void UnlockAction() override;
	bool ForceQueued() override;
	[[nodiscard]] bool HeldByHelp() const override;

private:
	PlayerNames _player;
	components::ComputerPlayer& _state;
	GameHandMoves _moves;
	GameHandObjects _objects;
	GameHandCasts _casts;
	GameHandCreature _creature;
	GameHandTotem _totem;
	computer_hand::World _hand;
	RuleContext _context;
};

/// Before the mind's turn: the god forgets its miracles that are gone, and stops an action whose things are gone
void Validate(components::ComputerPlayer& state);
/// Where the hand of the god starts on the land: at its temple
[[nodiscard]] std::optional<glm::vec3> StartingHand(PlayerNames player);

} // namespace openblack::ecs::systems::computer_world
