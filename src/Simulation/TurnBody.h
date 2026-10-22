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

#include <entt/entity/fwd.hpp>
#include <glm/vec3.hpp>

#include "GameTurnOrder.h"

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
	case Step::HelpProfile:
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

} // namespace openblack::simulation
