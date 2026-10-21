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

/// The switches of the original game's command line, such as -NEWGAME or /SKIRMISH, and the settings file that the
/// -SETTINGS: switch names. These rules keep no state, so that the game and the tests use them both.
namespace openblack::launch_switches
{

/// The switches that the command line gave
struct Switches
{
	/// Start a new game on the first land
	bool newGame {false};
	/// Go to the network game
	bool multiplayer {false};
	/// The game reads this switch, but nothing uses it
	bool land {false};
	/// Open the skirmish box
	bool skirmish {false};
	/// Play the pre-intro film, also when a player profile is there
	bool preIntroVideo {false};
	/// Look for a connection to the internet, which only the online parts use
	bool forceInternetCheck {false};
	/// Do not look for a connection to the internet
	bool noInternetCheck {false};
	/// The game reads this switch, but nothing uses it
	bool editor {false};
	/// Load the first land, then close the program before the first frame of play
	bool convert {false};
	/// Write the number that the network game uses to compare its versions
	bool version {false};
	/// Load no music
	bool noLoadMusic {false};
	/// The settings file to read
	std::optional<std::string> settingsFile;
};

/// The switches in the text of a command line. A switch starts after a "-" or a "/". Its name can be in any case, and
/// other text can follow the name. The character directly after each "-" or "/" never starts a switch, thus "--NEWGAME"
/// is not a switch. The -SETTINGS: switch takes the text after the colon up to the next space. Of -NEWGAME and
/// -MULTIPLAYER, only the first on the line counts.
[[nodiscard]] Switches Scan(std::string_view commandLine);

/// Whether one argument of the command line is a switch of the original game: a "-" or a "/" and then the name of a
/// switch
[[nodiscard]] bool IsSwitchArgument(std::string_view argument);

/// The first game that the switches ask for
enum class FirstGame
{
	/// The game starts as it does with no switch
	Plain,
	/// A new game on the first land
	NewGame,
	/// The skirmish box
	Skirmish,
	/// The network game
	Multiplayer,
};

/// -NEWGAME comes before -SKIRMISH, and -SKIRMISH comes before -MULTIPLAYER. With no player profile, the game always
/// asks for a new player and starts a new game.
[[nodiscard]] constexpr FirstGame FirstGameOf(const Switches& switches) noexcept
{
	if (switches.newGame)
	{
		return FirstGame::NewGame;
	}
	if (switches.skirmish)
	{
		return FirstGame::Skirmish;
	}
	if (switches.multiplayer)
	{
		return FirstGame::Multiplayer;
	}
	return FirstGame::Plain;
}

/// The folder of the challenge file when no settings file changes it. It is relative to the game's folder.
constexpr std::string_view k_DefaultQuestFolder = ".\\Scripts\\Quests\\";
/// The name of the challenge file. The game puts it directly after the folder text.
constexpr std::string_view k_ChallengeFile = "Challenge.chl";

/// The challenge folder from the text of a settings file, or none when the file has no QUESTDIR line. The game reads
/// the file line by line. On each line the words are divided by spaces, commas, tabs and line ends. A line whose first
/// word starts with "/", or has "/" as its second character, is a comment. On a line whose first word is QUESTDIR in
/// capitals, the second word is the folder. The last such line counts.
[[nodiscard]] std::optional<std::string> QuestFolderOf(std::string_view settingsText);

/// The path of the challenge file, relative to the game's folder or absolute, from the folder text. The game puts the
/// file name directly after the folder text, so a folder without a "\" at its end makes a different file name. A "\"
/// becomes a "/", and a ".\" or "./" at the start goes.
[[nodiscard]] std::string ChallengePath(std::string_view questFolder);

} // namespace openblack::launch_switches
