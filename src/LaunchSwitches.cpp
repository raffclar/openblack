/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "LaunchSwitches.h"

#include <cstddef>

#include <algorithm>
#include <array>
#include <ranges>

using namespace openblack;
using namespace openblack::launch_switches;

namespace
{

constexpr std::string_view k_NewGame = "NEWGAME";
constexpr std::string_view k_Multiplayer = "MULTIPLAYER";
constexpr std::string_view k_Land = "LAND";
constexpr std::string_view k_Skirmish = "SKIRMISH";
constexpr std::string_view k_PreIntroVideo = "PREINTROVIDEO";
constexpr std::string_view k_ForceInternetCheck = "FORCEINETCONN";
constexpr std::string_view k_NoInternetCheck = "NOINETCONN";
constexpr std::string_view k_Editor = "EDITOR";
constexpr std::string_view k_Convert = "CONVERT";
constexpr std::string_view k_Version = "VERSION";
constexpr std::string_view k_NoLoadMusic = "NOLOADMUSIC";
constexpr std::string_view k_Settings = "SETTINGS:";

constexpr std::array k_Names {k_NewGame,         k_Multiplayer, k_Land,    k_Skirmish, k_PreIntroVideo, k_ForceInternetCheck,
                              k_NoInternetCheck, k_Editor,      k_Convert, k_Version,  k_NoLoadMusic,   k_Settings};

constexpr std::string_view k_QuestFolderWord = "QUESTDIR";
/// The words of a line of the settings file are divided by these characters
constexpr std::string_view k_Dividers = " ,\t\n";
/// The game reads at most this many characters of a line of the settings file at a time
constexpr size_t k_LineCharacters = 0x1000 - 1;

[[nodiscard]] constexpr char Lower(char c) noexcept
{
	return c >= 'A' && c <= 'Z' ? static_cast<char>(c + ('a' - 'A')) : c;
}

[[nodiscard]] constexpr bool IsSwitchStart(char c) noexcept
{
	return c == '-' || c == '/';
}

/// Whether the text starts with the name, in any case. The rest of the command line must be as long as the name.
[[nodiscard]] bool StartsWithName(std::string_view text, std::string_view name)
{
	return text.size() >= name.size() &&
	       std::ranges::equal(text.substr(0, name.size()), name, [](char a, char b) { return Lower(a) == Lower(b); });
}

/// The next word of a line, from a position, as the game divides the words of a line
[[nodiscard]] std::optional<std::string_view> NextWord(std::string_view line, size_t& position)
{
	const auto start = line.find_first_not_of(k_Dividers, position);
	if (start == std::string_view::npos)
	{
		position = line.size();
		return std::nullopt;
	}
	auto end = line.find_first_of(k_Dividers, start);
	if (end == std::string_view::npos)
	{
		end = line.size();
	}
	// The divider after the word is used up with the word
	position = std::min(end + 1, line.size());
	return line.substr(start, end - start);
}

} // namespace

Switches launch_switches::Scan(std::string_view commandLine)
{
	Switches switches;
	size_t index = 0;
	while (index < commandLine.size())
	{
		if (!IsSwitchStart(commandLine[index]))
		{
			++index;
			continue;
		}
		const auto rest = commandLine.substr(index + 1);
		// Of -NEWGAME and -MULTIPLAYER, the second never counts
		if (StartsWithName(rest, k_NewGame) && !switches.multiplayer)
		{
			switches.newGame = true;
		}
		if (StartsWithName(rest, k_Multiplayer) && !switches.newGame)
		{
			switches.multiplayer = true;
		}
		switches.land = switches.land || StartsWithName(rest, k_Land);
		switches.skirmish = switches.skirmish || StartsWithName(rest, k_Skirmish);
		switches.preIntroVideo = switches.preIntroVideo || StartsWithName(rest, k_PreIntroVideo);
		switches.forceInternetCheck = switches.forceInternetCheck || StartsWithName(rest, k_ForceInternetCheck);
		switches.noInternetCheck = switches.noInternetCheck || StartsWithName(rest, k_NoInternetCheck);
		switches.editor = switches.editor || StartsWithName(rest, k_Editor);
		switches.convert = switches.convert || StartsWithName(rest, k_Convert);
		switches.version = switches.version || StartsWithName(rest, k_Version);
		switches.noLoadMusic = switches.noLoadMusic || StartsWithName(rest, k_NoLoadMusic);
		if (StartsWithName(rest, k_Settings))
		{
			// Each -SETTINGS: replaces the file of the one before it. An empty name gives no file.
			const auto name = rest.substr(k_Settings.size());
			const auto file = name.substr(0, name.find(' '));
			switches.settingsFile = file.empty() ? std::nullopt : std::optional<std::string>(std::string(file));
		}
		// The character directly after the "-" or "/" is not examined as the start of a switch
		index += 2;
	}
	return switches;
}

bool launch_switches::IsSwitchArgument(std::string_view argument)
{
	return !argument.empty() && IsSwitchStart(argument.front()) &&
	       std::ranges::any_of(k_Names,
	                           [rest = argument.substr(1)](std::string_view name) { return StartsWithName(rest, name); });
}

std::optional<std::string> launch_switches::QuestFolderOf(std::string_view settingsText)
{
	// The game reads the file as text: a carriage return before a line end goes, and a Ctrl+Z ends the file
	std::string text(settingsText.substr(0, settingsText.find('\x1a')));
	for (auto found = text.find("\r\n"); found != std::string::npos; found = text.find("\r\n", found))
	{
		text.erase(found, 1);
	}

	std::optional<std::string> folder;
	const std::string_view all(text);
	size_t start = 0;
	while (start < all.size())
	{
		// A line ends after its line end, or after the most characters that the game reads at a time
		auto end = all.find('\n', start);
		end = end == std::string_view::npos ? all.size() : end + 1;
		end = std::min(end, start + k_LineCharacters);
		const auto line = all.substr(start, end - start);
		start = end;

		size_t position = 0;
		const auto first = NextWord(line, position);
		if (!first.has_value() || first->front() == '/' || (first->size() > 1 && (*first)[1] == '/'))
		{
			continue;
		}
		if (*first != k_QuestFolderWord)
		{
			continue;
		}
		if (const auto second = NextWord(line, position); second.has_value())
		{
			folder = std::string(*second);
		}
	}
	return folder;
}

std::string launch_switches::ChallengePath(std::string_view questFolder)
{
	std::string path(questFolder);
	std::ranges::replace(path, '\\', '/');
	if (path.starts_with("./"))
	{
		path.erase(0, 2);
	}
	path += k_ChallengeFile;
	return path;
}
