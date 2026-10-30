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
	[[nodiscard]] std::optional<glm::vec3> MarkerPlace(entt::entity thing) const override;
	[[nodiscard]] bool HeldByHelp() const override;
	[[nodiscard]] const computer_mind::NodeNames& Names() const override;
	/// Tools: the world adds the time of each step of the hand, for the developer log of the times of the gods
	void TimeHand(bool on) { _timeHand = on; }
	/// The time of the steps of the hand in this turn, in milliseconds
	[[nodiscard]] double HandMs() const { return _handMs; }

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
	bool _timeHand {false};
	double _handMs {0.0};
};

/// Before the mind's turn: the god forgets its miracles, its seen things and its queued actions whose things are gone,
/// and stops an action whose things are gone. Then its hand puts down what it holds where the hand is.
void Validate(PlayerNames player, components::ComputerPlayer& state);
/// The thing is in the world, and it is not a villager on its way to dying
[[nodiscard]] bool Available(entt::entity thing);
/// The cinema bars hold the mind of the god while it has no script action and no queued action
[[nodiscard]] bool HeldByHelp(const components::ComputerPlayer& state);
/// The god adds the forests and the seeds of miracles near its hand to its list of seen things
void SeeNearHand(components::ComputerPlayer& state, uint32_t turn);
/// The names of the kinds of node that all the gods of the land share
[[nodiscard]] const computer_mind::NodeNames& SharedNodeNames();
/// The shared names of the kinds of node, to change them. The first change makes the entity that keeps them.
[[nodiscard]] computer_mind::NodeNames& SharedNodeNamesToChange();
/// Where the hand of the god starts on the land: at its temple
[[nodiscard]] std::optional<glm::vec3> StartingHand(PlayerNames player);

} // namespace openblack::ecs::systems::computer_world
