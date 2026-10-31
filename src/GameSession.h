/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <optional>
#include <string>
#include <string_view>

/// The rules of a game from its start to its end. The program plays one game after the other. Each game starts in one
/// of a few ways, and the way that the last game ended gives the way that the next game starts. A plain start of the
/// program continues the last game of the player: it loads the save on exit and shows the menu over it. These rules
/// keep no game state, so that the game and the tests use them both.
namespace openblack::game_session
{

/// How a game starts
enum class Start
{
	/// The save on exit of the player loads, and the menu shows over it. With no save on exit, a new game starts.
	Continue,
	/// A new game starts on the first land
	NewGame,
	/// A saved game that the player chose loads
	SavedGame,
	/// A game on the network starts
	Online,
	/// A skirmish starts
	Skirmish,
};

/// How a game ends, by what the player chose
enum class End
{
	/// The player started a new game
	NewGame,
	/// The game starts again as the profile that the player picked
	Continue,
	/// The player went to the network game
	Online,
	/// The player loaded a saved game
	SavedGame,
	/// The player started a skirmish
	Skirmish,
	/// The player quit: the program closes
	Quit,
};

/// The start of the next game after a game that ended, none when the program closes
[[nodiscard]] constexpr std::optional<Start> NextStart(End end) noexcept
{
	switch (end)
	{
	case End::NewGame:
		return Start::NewGame;
	case End::Continue:
		return Start::Continue;
	case End::Online:
		return Start::Online;
	case End::SavedGame:
		return Start::SavedGame;
	case End::Skirmish:
		return Start::Skirmish;
	case End::Quit:
		break;
	}
	return std::nullopt;
}

/// The controls that let a player leave a network game
enum class NetworkLeave
{
	/// Leave Game in the end box, after the question of the menu or at the end of the game
	LeaveGame,
	/// Back in the land information box
	LandInfoBoxBack,
};

/// How the game of a player ends when the player leaves a network game. Each control closes the network game of this
/// computer in the same way, and the other computers see a god that leaves. Leave Game goes to the network front end.
/// Back in the land information box goes to the skirmish box, also in a network game.
[[nodiscard]] constexpr End EndAfterNetworkLeave(NetworkLeave leave) noexcept
{
	switch (leave)
	{
	case NetworkLeave::LeaveGame:
		break;
	case NetworkLeave::LandInfoBoxBack:
		return End::Skirmish;
	}
	return End::Online;
}

/// What the program was told as it started
struct FirstStartFacts
{
	/// The player had a profile before the program started. A first start makes the profile in the new player's box.
	bool hadProfile {false};
	/// A run of a developer or of a tool, which starts a new game as the new game option does
	bool developerRun {false};
	/// The command line asked for a new game
	bool newGameAsked {false};
	/// The command line asked to continue as a plain start does, a run of a developer too
	bool continueAsked {false};
};

/// How the first game of the program starts. A plain start continues. A first start, with no profile, always starts a
/// new game.
[[nodiscard]] constexpr Start FirstStart(const FirstStartFacts& facts) noexcept
{
	if (!facts.hadProfile || facts.newGameAsked)
	{
		return Start::NewGame;
	}
	if (facts.developerRun && !facts.continueAsked)
	{
		return Start::NewGame;
	}
	return Start::Continue;
}

/// The start that the game does: a continue with no save on exit that the game can open starts a new game
[[nodiscard]] constexpr Start Resolve(Start start, bool exitSaveOpens) noexcept
{
	return start == Start::Continue && !exitSaveOpens ? Start::NewGame : start;
}

/// Only a game that continued from the save on exit shows the menu as it starts. The game is paused under the menu.
[[nodiscard]] constexpr bool ShowsMenuOnStart(Start start, bool exitSaveLoaded) noexcept
{
	return start == Start::Continue && exitSaveLoaded;
}

/// A close of the window ends the game at the end of a frame. While a script's cut scene runs, the game continues, and
/// the game ends at the end of the first frame after the cut scene. The game keeps the close until then.
[[nodiscard]] constexpr bool CloseEndsGame(bool closeAsked, bool scriptCutScene) noexcept
{
	return closeAsked && !scriptCutScene;
}

/// The profile that the save on exit goes to, as a game ends
enum class ExitSave
{
	/// The game ends with no save
	None,
	/// The save goes to the profile that the player plays now
	Current,
	/// The save goes to the profile that the player played before they picked another one on the Players page
	Previous,
};

/// What the game knows as it ends
struct ExitSaveFacts
{
	End end {End::Quit};
	bool gameOver {false};
	bool multiplayer {false};
	bool skirmish {false};
	/// The land is the Gods' Playground
	bool godsPlayground {false};
	/// The profile that the player played as the Players page last opened in this game, none when it did not open
	std::optional<std::u16string> previousProfile;
	/// The profile that the player plays now
	std::u16string currentProfile;
	/// The previous profile is still there: the player did not delete it
	bool previousExists {false};
};

/// Two profile names are the same when only the case of their plain letters is not the same
[[nodiscard]] bool SameProfileName(std::u16string_view a, std::u16string_view b);

/// The save as a game ends, into the slot of the save on exit. A game that is over, a game on the network, a skirmish, a
/// load of a saved game and the Gods' Playground have no save. When the player picked another profile, the save goes
/// to the profile that they played before. When the game starts again as the same profile, it has no save, and the
/// next game continues from the last save on exit.
[[nodiscard]] ExitSave ExitSaveFor(const ExitSaveFacts& facts);

} // namespace openblack::game_session
