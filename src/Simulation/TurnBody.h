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

#include <entt/entity/fwd.hpp>
#include <glm/vec3.hpp>

#include "GameTurnOrder.h"
#include "TurnStepTimes.h"

namespace openblack::simulation
{

/// The parts of a game turn that belong to the view of one player: the sounds, the music, the tooltips of the hand and
/// the help that watches the screen. The game's client does them. A server has no view, and does nothing in them.
class TurnViewInterface
{
public:
	virtual ~TurnViewInterface() = default;

	/// Do a step of the turn that only the view has. The camera is where the scripts of the turn left it.
	virtual void PlayViewStep(game_turn::Step step, glm::vec3 cameraPosition) = 0;

	/// The frames each second that the statistics of the game keep
	[[nodiscard]] virtual int32_t FrameRate() const = 0;

	/// The audio of a turn of the temple, while the player is in the temple and the world is paused. A view with no
	/// audio does nothing.
	virtual void PlayTempleAudioTurn() {}

	/// The developer tools of this computer that act before each turn, as a testbed scenario of a network game does. A
	/// view with no such tools does nothing.
	virtual void BeforeTurn() {}
};

/// A view that does nothing, for a world with no player in front of it. It gives the statistics a fixed frame rate.
class NoTurnView final: public TurnViewInterface
{
public:
	explicit NoTurnView(int32_t frameRate) noexcept
	    : _frameRate(frameRate)
	{
	}
	void PlayViewStep(game_turn::Step /*step*/, glm::vec3 /*cameraPosition*/) override {}
	[[nodiscard]] int32_t FrameRate() const override { return _frameRate; }

private:
	int32_t _frameRate;
};

/// True for the steps of the turn that the view does, and that the world leaves to it
[[nodiscard]] constexpr bool IsViewStep(game_turn::Step step)
{
	using game_turn::Step;
	switch (step)
	{
	case Step::TurnCheering:
	case Step::HandToolTips:
	case Step::Ambience:
	case Step::Music:
		return true;
	default:
		return false;
	}
}

/// Do one step of the game turn. The steps of the view go to the view.
void PlayTurnStep(game_turn::Step step, TurnViewInterface& view);

/// Do one step of the own turn of a creature
void PlayCreatureTurnStep(game_turn::Step step, entt::entity creature);

/// Do all the steps of one game turn, in the order of the game
void PlayTurn(TurnViewInterface& view);

class MindFrameBudget;

/// A turn that stopped between two players of its players' step, so that the frame can draw before the turn continues
/// (deviation row 288). Nothing of the world changes until the turn continues.
struct SplitTurn
{
	/// The place where the turn continues
	game_turn::TurnPlace place;
	/// The number of the turn, as its checksum and its record have it
	uint32_t number {0};
	/// The time of each step of the turn until now, for the developer log of long turns
	TurnStepTimes stepTimes;
	/// The frames that the turn played in until now
	uint32_t frames {1};
};

/// Do the steps of one game turn, in the order of the game. With a budget, the turn stops before the next player when
/// the computer minds of this frame used the budget, and it gives the turn that waits to continue. With no budget, or
/// when the turn did all of its steps, there is no such turn.
[[nodiscard]] std::optional<SplitTurn> StartTurn(TurnViewInterface& view, MindFrameBudget* budget);

/// Continue a turn that stopped between two players, as StartTurn does. With no budget, the turn does all of its steps.
[[nodiscard]] std::optional<SplitTurn> ContinueTurn(TurnViewInterface& view, SplitTurn turn, MindFrameBudget* budget);

/// The count of the turns that the world did completely: the turn count of the clock, less one while a turn waits to
/// continue. The clock counts a turn at its start.
[[nodiscard]] uint32_t WholeTurns();

} // namespace openblack::simulation
