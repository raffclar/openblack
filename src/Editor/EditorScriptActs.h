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
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <GameSocket.h>

#include "Commands/PlayerCommands.h"
#include "Editor/Scripts/LandScriptStepper.h"
#include "LHScriptX/Script.h"

/// The acts of the Scripts tab and the Land script tab of the editor. A game alone does each act at once. In a network game
/// the act is a command of the turns, and each computer does it at the same turn, so that the script machine and the land
/// script stepper stay the same on every computer. The rules take the machine and the world as interfaces, so that the
/// tests give fakes.
namespace openblack::editor::script_acts
{

using player_commands::EditorLandScript;
using player_commands::EditorScript;
using player_commands::EditorScriptProgram;

/// The script machine as the acts see it. The game gives its own machine, and the tests give a fake.
class MachineInterface
{
public:
	struct ScriptInfo
	{
		std::string name;
		uint32_t parameters {0};
	};

	virtual ~MachineInterface() = default;

	/// The script with the id, or none
	[[nodiscard]] virtual std::optional<ScriptInfo> Script(uint32_t id) const = 0;
	[[nodiscard]] virtual bool HasTask(uint32_t task) const = 0;
	/// The ids of the tasks, in the order of the machine
	[[nodiscard]] virtual std::vector<uint32_t> Tasks() const = 0;
	[[nodiscard]] virtual bool IsHeld(uint32_t task) const = 0;
	/// The count of the instructions of the program
	[[nodiscard]] virtual size_t CodeSize() const = 0;
	/// The count of the global variables, with the null variable 0
	[[nodiscard]] virtual size_t GlobalCount() const = 0;
	[[nodiscard]] virtual size_t TaskVariableCount(uint32_t task) const = 0;

	/// Starts a task of the script with the parameters. Gives the new task, or 0 when the machine did not start it.
	virtual uint32_t Start(const ScriptInfo& script, std::span<const float> parameters) = 0;
	/// Stops each task of the script with the name
	virtual void StopScript(const std::string& name) = 0;
	virtual void Hold(uint32_t task) = 0;
	virtual void Continue(uint32_t task) = 0;
	virtual void Step(uint32_t task) = 0;
	virtual void Stop(uint32_t task) = 0;
	virtual void SetBreakpoint(uint32_t address, bool on) = 0;
	[[nodiscard]] virtual std::vector<uint32_t> Breakpoints() const = 0;
	/// Sets the bits of the value of a global variable or of a local variable of a task. The type stays.
	virtual void SetGlobal(uint32_t id, uint32_t bits) = 0;
	virtual void SetTaskVariable(uint32_t task, size_t index, uint32_t bits) = 0;
};

/// The name of an act, for the log of the game
[[nodiscard]] std::string_view NameOf(EditorScript::Act act) noexcept;
[[nodiscard]] std::string_view NameOf(EditorLandScript::Act act) noexcept;
[[nodiscard]] std::string_view NameOf(EditorScriptProgram::Use use) noexcept;

/// Why no computer takes the act, or none when the act can go. The rule keeps no state, so the game server uses it before
/// the turn. The machine of each computer checks the ids at the turn.
[[nodiscard]] std::optional<std::string_view> ScriptActRefusal(const EditorScript& act) noexcept;

/// What an act did at the turn
struct ActResult
{
	bool done {false};
	/// Why the machine did not take the act. Empty when it took it.
	std::string_view refusal;
	/// The new task of a start, or 0
	uint32_t task {0};
};

/// Does an act of the editor on a script machine
[[nodiscard]] ActResult ApplyScriptAct(const EditorScript& act, MachineInterface& machine);
/// Does an act of the editor on the script machine of the Locator
[[nodiscard]] ActResult RunScriptAct(const EditorScript& act);

/// What the world of a computer gives to a change of the program at the turn. The game gives the world of the Locator,
/// and the tests give fakes.
struct ProgramWorld
{
	/// Compiles the source of the script with the id into the program. Gives the first message of the compiler when the
	/// source does not compile, or none when the machine runs the new code.
	std::function<std::optional<std::string>(uint32_t script, std::string_view source)> compile;
	/// Loads the bytes of a compiled program in place of the program of the machine. False when the bytes do not read.
	std::function<bool(std::span<const uint8_t> bytes)> load;
	/// Loads the program of the game again. False when the program of the game does not read.
	std::function<bool()> loadGames;
};

/// What a change of the program did at the turn
struct ProgramResult
{
	bool done {false};
	std::string refusal;
};

/// Whether the bytes read as a compiled program, as the script machine reads a program that it loads
[[nodiscard]] bool ReadsProgram(std::span<const uint8_t> bytes);

/// The bytes of a file of the game folder or of the disk. It throws when the file does not read.
[[nodiscard]] std::vector<uint8_t> ReadProgramFile(const std::string& path);

/// Why no computer takes the change of the program, or none when it can go. kept tells whether the store of the computer
/// keeps the file of the command.
[[nodiscard]] std::optional<std::string_view> ProgramRefusal(const EditorScriptProgram& change, bool kept) noexcept;
/// The kind of the file that a change of the program uses, or none when it uses no file
[[nodiscard]] std::optional<game_socket::EditorScriptFile::Kind> FileKindOf(const EditorScriptProgram& change) noexcept;
/// Does a change of the program. file is the bytes of the file with the hash of the command, from the store of this
/// computer, or none.
[[nodiscard]] ProgramResult ApplyProgram(const EditorScriptProgram& change, std::span<const uint8_t> file,
                                         const ProgramWorld& world);
/// Does a change of the program on the script machine of the Locator
[[nodiscard]] ProgramResult RunProgram(const EditorScriptProgram& change, std::span<const uint8_t> file);

/// The land script that the Land script tab steps through: the commands, which of them are done, and the breakpoints. The
/// editor system keeps it, so that each computer of a network game steps the same commands.
struct LandScriptRun
{
	/// The name of the file of the script
	std::string name;
	/// The text that the run read. A world state keeps it, so that a load reads the same commands again.
	std::string text;
	lhscriptx::ReadCommands read;
	scripts::LandScriptStepper stepper;
	/// What the last act did
	std::string message;
	/// Goes up at each act, so that a panel knows that the run changed
	uint32_t changes {0};
	/// The count of the resets of the world when the run read its script. None until the run reads a script.
	std::optional<uint32_t> world;
};

/// Reads the text of a land script into the run: the commands, none done and no breakpoints
void ReadLandScript(LandScriptRun& run, std::string name, const std::string& text);

/// The run of the land script as a world state keeps it. The commands are not in it: a load reads the text again.
struct KeptLandScriptRun
{
	std::string name;
	std::string text;
	scripts::LandScriptStepper::Kept stepper;
	std::string message;
	uint32_t changes {0};

	bool operator==(const KeptLandScriptRun&) const = default;
};
[[nodiscard]] KeptLandScriptRun KeepLandScriptRun(const LandScriptRun& run);
/// Puts back a kept run. A kept run that read no script gives a run with no script. For other runs, it reads the text
/// again and puts back the position of the stepper. It gives false when the position does not agree with the commands of
/// the text. Then the run has the commands with none done. The count of the resets of the world after the load is the
/// world of the run: a load of a world state resets the registry, and a run with an older count would start again at its
/// next act.
bool RestoreLandScriptRun(LandScriptRun& run, const KeptLandScriptRun& kept, std::optional<uint32_t> worldResets);

/// The text of the land script in a file: the text of a land script file, or the script in a land of a network game. None
/// when the file does not read.
[[nodiscard]] std::optional<std::string> LandScriptText(const std::filesystem::path& path);

/// What the world of a computer gives to an act of the land script at the turn. The game gives the world of the Locator,
/// and the tests give fakes.
struct LandScriptWorld
{
	/// The path of the land that the game loaded last, or none
	std::function<std::optional<std::filesystem::path>()> loadedLand;
	/// The text of the land script of a file, or none
	std::function<std::optional<std::string>(const std::filesystem::path&)> readText;
	/// Does a command in the world. Gives the text of the error when the command failed, or none.
	std::function<std::optional<std::string>(const lhscriptx::Command&)> run;
	/// The count of the resets of the world. A load of a land or of a saved world adds one. With none, the count is 0.
	std::function<uint32_t()> worldResets;
};

/// Why no computer takes the act, or none when it can go. kept tells whether the store of the computer keeps the file
/// of an Open.
[[nodiscard]] std::optional<std::string_view> LandScriptRefusal(const EditorLandScript& act, bool kept) noexcept;

/// What an act of the land script did
struct LandScriptResult
{
	bool done {false};
	std::string_view refusal;
	/// The count of the commands that the act did in the world
	size_t commands {0};
};

/// Starts the run again with the script of the loaded land when the game loaded another world after the last read. A run
/// that read no script stays empty. Gives true when the run started again. The run changes only the state of the stepper,
/// so a frame and a turn can both do it with the same result.
bool RestartAtNewWorld(LandScriptRun& run, const LandScriptWorld& world);

/// Does an act of the land script on the run. file is the text of an Open, from the store of this computer, or none. The
/// run first starts again when the game loaded another world (RestartAtNewWorld).
[[nodiscard]] LandScriptResult ApplyLandScript(const EditorLandScript& act, std::span<const uint8_t> file, LandScriptRun& run,
                                               const LandScriptWorld& world);
/// The world of the Locator for the acts of the land script
[[nodiscard]] LandScriptWorld LocatorLandScriptWorld();
/// Does an act of the land script on the run of the editor system of the Locator
[[nodiscard]] LandScriptResult RunLandScript(const EditorLandScript& act, std::span<const uint8_t> file);
/// Starts the run of the editor system of the Locator again when the game loaded another world
void RestartLandScriptAtNewWorld();

/// What an act of the editor of this computer did at the turn of a network game, for the Scripts tab
struct ActReport
{
	enum class Kind : uint8_t
	{
		/// An act on the script machine (EditorScript)
		Script,
		/// A change of the program (EditorScriptProgram)
		Program,
	};

	Kind kind {Kind::Script};
	/// The act of a Script report
	EditorScript::Act act {EditorScript::Act::Start};
	/// The use of a Program report
	EditorScriptProgram::Use use {EditorScriptProgram::Use::Compile};
	uint32_t turn {0};
	bool done {false};
	/// The new task of a start, or 0
	uint32_t task {0};
	/// The line that the tab shows
	std::string text;
	/// The value of ActReports::changes that came with the report
	uint32_t number {0};
};

/// The last reports of the acts of the editor of this computer. The tab compares changes with the value that it showed.
struct ActReports
{
	/// The last report of an act on the script machine
	std::optional<ActReport> script;
	/// The last report of a change of the program
	std::optional<ActReport> program;
	/// Goes up at each report
	uint32_t changes {0};
};

/// The report of an act on the script machine at the turn
[[nodiscard]] ActReport ReportOf(const EditorScript& act, const ActResult& result, uint32_t turn);
/// The report of a change of the program at the turn
[[nodiscard]] ActReport ReportOf(const EditorScriptProgram& change, const ProgramResult& result, uint32_t turn);
/// Keeps the report as the last report of its kind, with the next number
void Keep(ActReports& reports, ActReport report);

/// Sends the bytes of a file on the game socket and gives the hash of the bytes. None when the game socket did not take
/// the bytes: then the command that uses the file must not go.
[[nodiscard]] std::optional<uint64_t> SendFile(game_socket::EditorScriptFile::Kind kind, std::span<const uint8_t> bytes);

} // namespace openblack::editor::script_acts
