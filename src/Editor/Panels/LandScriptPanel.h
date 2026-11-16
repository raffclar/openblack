/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "Commands/PlayerCommands.h"
#include "Debug/FileBrowser.h"
#include "Editor/EditorScriptActs.h"

namespace openblack::editor
{

/// The setup script of a land, laid out to read and to step through:
/// - the commands of the land that the game loaded, or of a land script file, each with its line and its arguments
/// - a filter on the names and the arguments of the commands
/// - a step, a continue, a run to the selected command, a skip and a restart, with breakpoints set on the commands
/// Each command that the panel does changes the world, as the land script does it when the land loads. The steps that the
/// land does after its script, such as the footpath links and the slots of the computer gods, do not occur. The editor
/// system keeps the stepper. In a network game each act is a command of the turns, so every computer steps the same
/// commands at the same turn.
class LandScriptPanel
{
public:
	void Draw() noexcept;

private:
	/// Does an act on the stepper: at once in a game alone, or at the next turn in a network game. text is the text of an
	/// open.
	void Act(player_commands::EditorLandScript act, std::span<const uint8_t> text = {}) noexcept;
	/// Reads a land script file, or the script in a land of a network game
	void ReadFile(const std::filesystem::path& path) noexcept;
	void DrawLoading(const script_acts::LandScriptRun& run) noexcept;
	void DrawControls(const script_acts::LandScriptRun& run) noexcept;
	void DrawCommands(const script_acts::LandScriptRun& run) noexcept;
	/// The indices of the commands that agree with the filter
	void Refilter(const script_acts::LandScriptRun& run);

	std::string _filter;
	/// The filter needs a new list of commands, after a change of the filter or of the commands
	bool _refilter {true};
	/// The changes of the run that the list of commands shows
	uint32_t _shownChanges {0};
	std::vector<size_t> _visible;
	std::optional<size_t> _selected;
	/// What the panel did in a network game, and the file that it opened last
	std::string _sendMessage;
	std::filesystem::path _openedPath;
	/// The file browser of the panel, where the platform has no file dialog
	debug::gui::FileBrowser _fileBrowser;
	bool _scrollToNext {false};
	bool _triedCurrentLand {false};
};

} // namespace openblack::editor
