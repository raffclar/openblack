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

#include <functional>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "LHScriptX/Script.h"

namespace openblack::editor::scripts
{

/// The text of an argument of a land script command, as the script gives it
[[nodiscard]] std::string ArgumentText(const lhscriptx::Token& argument);

/// The arguments of a command, with a comma between each two
[[nodiscard]] std::string ArgumentsText(const lhscriptx::Command& command);

/// Whether a command agrees with a filter text. An empty filter agrees with all commands. The name of the command and
/// the text of its arguments must hold the filter, with no difference between upper and lower case.
[[nodiscard]] bool CommandMatches(const lhscriptx::Command& command, std::string_view filter);

/// Steps through the commands of a land script one at a time, as a debugger steps through code. The stepper does not do
/// the commands itself: it gives the index of each command to a function that does it. Breakpoints stop a run before
/// their command.
class LandScriptStepper
{
public:
	enum class State : uint8_t
	{
		NotDone,
		Done,
		Skipped,
		Failed,
	};

	/// Does the command at the index. It gives true when the command is done, and false when the command failed.
	using Run = std::function<bool(size_t index)>;

	LandScriptStepper() noexcept = default;
	explicit LandScriptStepper(size_t count);

	/// Starts again with a number of commands, none done and no breakpoints
	void Reset(size_t count);
	/// Starts again at the first command, none done. The breakpoints stay.
	void Restart() noexcept;

	[[nodiscard]] size_t Count() const noexcept { return _states.size(); }
	/// The command to do next, or none after the last command
	[[nodiscard]] std::optional<size_t> Next() const noexcept;
	[[nodiscard]] State StateOf(size_t index) const noexcept;

	void ToggleBreakpoint(size_t index);
	[[nodiscard]] bool HasBreakpoint(size_t index) const noexcept { return _breakpoints.contains(index); }

	/// Does the next command. It gives false when no command was left.
	bool Step(const Run& run);
	/// Does the next command, then the commands after it. The run stops before a command with a breakpoint, after a
	/// command that failed, or after the last command. It gives the number of commands that it did.
	size_t Continue(const Run& run);
	/// Does the commands from the next command up to the command at the index. The command at the index is not done.
	/// The run stops after a command that failed. It gives the number of commands that it did.
	size_t RunTo(size_t index, const Run& run);
	/// Moves past the next command and does not do it
	void Skip() noexcept;

private:
	/// Does the next command and moves past it. It gives false when the command failed.
	bool DoNext(const Run& run);

	std::vector<State> _states;
	std::set<size_t> _breakpoints;
	size_t _next {0};
};

} // namespace openblack::editor::scripts
