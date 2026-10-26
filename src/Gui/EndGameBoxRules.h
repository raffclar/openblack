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
#include <span>
#include <string>
#include <string_view>

#include "ECS/GameEndRules.h"
#include "Enums.h"

// The boxes that the end of a skirmish or a network game opens on this computer. The world gives the events of each
// turn (see game_end::ProcessTurn). This computer opens its boxes from those events and from its own god:
//
// - When the game is decided by its winning conditions or its time limit, the end box tells the winner that it won.
//   It tells each other god among the first four players that it lost. A looker-on is told who won.
// - When the god of this computer is out, the end box tells the player that the game is lost. While two or more gods are
//   still in, the player can watch the game.
// - When another god is out and one god is left, the end box tells the player that the game is won. The game does not
//   look at whether the god of this computer is out (vanilla-bugs 469).
// - When another god is out and the game goes on, a box with OK tells the player that the god is out. It opens only
//   when no end box opened on this computer before in the game, also an end box that the player closed.
//
// An end box closes the box with OK.
//
// Pure, tested with fakes.

namespace openblack::end_game_box
{

/// What the end box says
enum class Kind : uint8_t
{
	/// The player asks to leave the network game
	LeaveQuestion = 0,
	/// The god of this computer is out, and the game goes on
	Lost = 1,
	Won = 2,
	/// The god of this computer is out, and the game is decided
	LostAndDecided = 3,
};

/// An end box that opens
struct Box
{
	Kind kind;
	/// The player can watch the game: the box has Watch Game
	bool watch {false};
};

/// What the boxes of this computer keep between turns
struct State
{
	/// An end box opened in the game. Nothing clears it.
	bool endBoxOpened {false};
	/// The game goes on for the player of this computer. Nothing but an end box of a lost game sets it.
	bool gameGoesOn {false};
};

/// The boxes that open in a turn. A later box of the turn takes the place of an earlier one.
struct Opened
{
	std::optional<Box> endBox;
	/// The box with OK that tells that this god is out
	std::optional<PlayerNames> godOut;
};

/// The boxes that the events of a turn open on the computer of the local god
[[nodiscard]] Opened Follow(std::span<const game_end::Event> events, PlayerNames local, State& state);

/// True when the box asks the game server to stop the turns of the game: an end box of a decided game, which the player
/// cannot watch
[[nodiscard]] bool StopsTheGame(const Box& box, const State& state);

/// True when the player of this computer only looks on: players five to eight never win or lose
[[nodiscard]] constexpr bool LooksOn(PlayerNames local)
{
	return !game_end::CanLose(local);
}

/// The title of the end box. The texts are the game's: leave is "Are you sure you want to quit this game?", lost is
/// "You have lost the game.", won is "Congratulations! You've won the game!", gameOver is "Game Over" and winnerFormat is
/// "%s - %s Won". A looker-on that the game tells "won" sees "Game Over - <the winner> Won".
struct Titles
{
	std::u16string_view leave;
	std::u16string_view lost;
	std::u16string_view won;
	std::u16string_view gameOver;
	std::u16string_view winnerFormat;
};
[[nodiscard]] std::u16string Title(Kind kind, bool looksOn, std::u16string_view winner, const Titles& titles);

/// Puts each word in place of each %s of a text, in order
[[nodiscard]] std::u16string Format(std::u16string_view text, std::span<const std::u16string_view> words);

/// The name of a god in the boxes: the name of the human that plays it, or "Player[n]", where n is the number of the god
/// from 1
[[nodiscard]] std::u16string GodName(PlayerNames god, std::optional<std::u16string_view> humanName);

} // namespace openblack::end_game_box
