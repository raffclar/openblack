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

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "Editor/Scripts/LandScriptStepper.h"
#include "LHScriptX/Script.h"

namespace openblack::editor
{

/// The setup script of a land, laid out to read and to step through:
/// - the commands of the land that the game loaded, or of a land script file, each with its line and its arguments
/// - a filter on the names and the arguments of the commands
/// - a step, a continue, a run to the selected command, a skip and a restart, with breakpoints set on the commands
/// Each command that the panel does changes the world now, as the land script does it when the land loads. The steps
/// that the land does after its script, such as the footpath links and the slots of the computer gods, do not occur.
class LandScriptPanel
{
public:
	void Draw() noexcept;

private:
	/// Reads the script of the land that the game loaded last
	void ReadCurrentLand() noexcept;
	/// Reads a land script file, or the script in a land of a network game
	void ReadFile(const std::filesystem::path& path) noexcept;
	void DrawLoading() noexcept;
	void DrawControls() noexcept;
	void DrawCommands() noexcept;
	/// Does the command at the index in the world. It gives false when the command failed.
	bool RunCommand(size_t index) noexcept;
	/// The indices of the commands that agree with the filter
	void Refilter();

	std::filesystem::path _path;
	lhscriptx::ReadCommands _read;
	scripts::LandScriptStepper _stepper;
	std::string _filter;
	/// The filter needs a new list of commands, after a change of the filter or of the commands
	bool _refilter {true};
	std::vector<size_t> _visible;
	std::optional<size_t> _selected;
	std::string _message;
	bool _scrollToNext {false};
	bool _triedCurrentLand {false};
};

} // namespace openblack::editor
