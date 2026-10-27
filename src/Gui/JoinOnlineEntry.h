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

/// The ways in to the multiplayer front end that the browser page gives. The page has a Join Online Game button
/// beside the game, which asks the game to open the front end at any time, also while a cut scene has the cinema bars
/// in. These rules are free of game state, so that the game and the tests share them.
namespace openblack::join_online_entry
{

/// What the game does when a request to open the front end comes
struct Situation
{
	/// The multiplayer front end is open
	bool frontEndOpen {false};
	/// The game is in a network game, or it loads the land of one
	bool networkGame {false};
	/// A box that the player must answer first is open: the new player's box, the boxes of the save game room or the
	/// tattoo editor. A game that ends or a land that loads also make the request wait.
	bool boxToAnswer {false};
	/// A script has the cinema bars in for its cut scene, so Escape does not open the menu
	bool scriptHoldsCinemaBars {false};
	/// The start-of-game question is open
	bool startQuestionOpen {false};
};

enum class Outcome : uint8_t
{
	/// The game does nothing, and it forgets the request
	Ignore,
	/// The game keeps the request until the situation changes
	Wait,
	/// The front end opens now, as Join Online Game of the menu opens it
	Open,
};

struct Decision
{
	Outcome outcome {Outcome::Ignore};
	/// The land stops (the player's pause) while the front end is open, and it continues when the player leaves the
	/// front end. Thus the cut scene continues from the point where it stopped.
	bool holdLand {false};
	/// The start-of-game question closes now, and it opens again when the player leaves the front end
	bool askQuestionAfter {false};
};

/// What the game does with a request of the page to open the multiplayer front end
[[nodiscard]] Decision Decide(const Situation& situation) noexcept;

/// What the first start of the program knows about the -MULTIPLAYER switch
struct FirstStart
{
	/// -MULTIPLAYER is the first game switch on the command line
	bool multiplayerSwitch {false};
	/// The player had a profile before the program started
	bool hadProfile {false};
	/// The player has a profile after the new player's box
	bool hasProfile {false};
	/// The game plays in a browser page
	bool browser {false};
};

/// Whether the multiplayer front end opens before the first frame. With a profile, -MULTIPLAYER opens it. With no
/// profile, the desktop game starts a new game. In a browser, the game asks for the new player's name, and then it
/// opens the front end.
[[nodiscard]] bool FrontEndOnStart(const FirstStart& start) noexcept;

} // namespace openblack::join_online_entry
