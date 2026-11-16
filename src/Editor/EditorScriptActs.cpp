/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "EditorScriptActs.h"

#include <cmath>

#include <algorithm>
#include <exception>
#include <ranges>

#include <LHVM.h>
#include <LHVMFile.h>
#include <OnlineMapFile.h>
#include <fmt/format.h>

#include "ECS/Systems/EditorSystemInterface.h"
#include "ECS/Systems/LandSystemInterface.h"
#include "ECS/Systems/NetworkGameSystemInterface.h"
#include "ECS/Systems/ScriptObjectsSystemInterface.h"
#include "Editor/EditorMindLoads.h"
#include "Editor/EditorScriptFiles.h"
#include "Editor/Scripts/Decompiler.h"
#include "FileSystem/FileSystemInterface.h"
#include "Locator.h"
#include "Online/OnlineMapLand.h"

namespace openblack::editor::script_acts
{

namespace
{
using Act = EditorScript::Act;
using LandAct = EditorLandScript::Act;
using Use = EditorScriptProgram::Use;
using FileKind = game_socket::EditorScriptFile::Kind;

/// The script machine of the game, for the acts
class GameMachine final: public MachineInterface
{
public:
	explicit GameMachine(lhvm::LHVM& vm)
	    : _vm(vm)
	{
	}

	[[nodiscard]] std::optional<ScriptInfo> Script(uint32_t id) const override
	{
		const auto& scripts = _vm.GetScripts();
		const auto found = std::ranges::find(scripts, id, &lhvm::VMScript::scriptId);
		if (found == scripts.end())
		{
			return std::nullopt;
		}
		return ScriptInfo {.name = found->name, .parameters = found->parameterCount};
	}
	[[nodiscard]] bool HasTask(uint32_t task) const override { return _vm.GetTasks().contains(task); }
	[[nodiscard]] std::vector<uint32_t> Tasks() const override
	{
		std::vector<uint32_t> ids;
		for (const auto& [id, task] : _vm.GetTasks())
		{
			ids.push_back(id);
		}
		return ids;
	}
	[[nodiscard]] bool IsHeld(uint32_t task) const override { return _vm.IsTaskHeld(task); }
	[[nodiscard]] size_t CodeSize() const override { return _vm.GetInstructions().size(); }
	[[nodiscard]] size_t GlobalCount() const override { return _vm.GetVariables().size(); }
	[[nodiscard]] size_t TaskVariableCount(uint32_t task) const override
	{
		const auto found = _vm.GetTasks().find(task);
		return found != _vm.GetTasks().end() ? found->second.localVars.size() : 0;
	}
	uint32_t Start(const ScriptInfo& script, std::span<const float> parameters) override
	{
		// The parameters go on the stack as the parameters of a script that calls, then the script starts as the game
		// starts scripts
		for (const auto value : parameters)
		{
			_vm.Pushf(value);
		}
		return _vm.StartScript(script.name, lhvm::ScriptType::All);
	}
	void StopScript(const std::string& name) override
	{
		_vm.StopScripts([&name](const std::string& taskName, const std::string&) { return taskName == name; });
	}
	void Hold(uint32_t task) override { _vm.HoldTask(task); }
	void Continue(uint32_t task) override { _vm.ContinueTask(task); }
	void Step(uint32_t task) override { _vm.StepTask(task); }
	void Stop(uint32_t task) override { _vm.StopTask(task); }
	void SetBreakpoint(uint32_t address, bool on) override { _vm.SetBreakpoint(address, on); }
	[[nodiscard]] std::vector<uint32_t> Breakpoints() const override
	{
		return {_vm.GetBreakpoints().begin(), _vm.GetBreakpoints().end()};
	}
	void SetGlobal(uint32_t id, uint32_t bits) override
	{
		lhvm::VMValue value;
		value.uintVal = bits;
		_vm.SetVariable(id, value);
	}
	void SetTaskVariable(uint32_t task, size_t index, uint32_t bits) override
	{
		lhvm::VMValue value;
		value.uintVal = bits;
		_vm.SetTaskVariable(task, index, value);
	}

private:
	lhvm::LHVM& _vm;
};

[[nodiscard]] bool UsesTask(Act act) noexcept
{
	return act == Act::StepTask || act == Act::ContinueTask || act == Act::HoldTask || act == Act::StopTask ||
	       act == Act::SetTaskVariable;
}
} // namespace

std::string_view NameOf(EditorScript::Act act) noexcept
{
	switch (act)
	{
	case Act::Start:
		return "start";
	case Act::StopScript:
		return "stop the tasks of";
	case Act::HoldAll:
		return "hold all tasks";
	case Act::ContinueAll:
		return "continue all tasks";
	case Act::StepTask:
		return "step";
	case Act::ContinueTask:
		return "continue";
	case Act::HoldTask:
		return "hold";
	case Act::StopTask:
		return "stop";
	case Act::Breakpoint:
		return "change a breakpoint";
	case Act::RemoveBreakpoints:
		return "remove all breakpoints";
	case Act::SetGlobal:
		return "set the global";
	case Act::SetTaskVariable:
		return "set a local of";
	}
	return "?";
}

std::string_view NameOf(EditorLandScript::Act act) noexcept
{
	switch (act)
	{
	case LandAct::LoadedLand:
		return "read the loaded land";
	case LandAct::Open:
		return "open a land script";
	case LandAct::Step:
		return "step";
	case LandAct::Continue:
		return "continue";
	case LandAct::RunTo:
		return "run to a command";
	case LandAct::Skip:
		return "skip";
	case LandAct::Restart:
		return "restart";
	case LandAct::Breakpoint:
		return "change a breakpoint";
	}
	return "?";
}

std::string_view NameOf(EditorScriptProgram::Use use) noexcept
{
	switch (use)
	{
	case Use::Compile:
		return "compile a script";
	case Use::Open:
		return "open a program";
	case Use::GamesProgram:
		return "open the program of the game";
	}
	return "?";
}

std::optional<std::string_view> ScriptActRefusal(const EditorScript& act) noexcept
{
	if (act.act > Act::SetTaskVariable)
	{
		return "the act is not one of the editor";
	}
	if (act.parameters.size() > player_commands::k_MostEditorScriptParameters)
	{
		return "the start has too many parameters";
	}
	if (act.act != Act::Start && !act.parameters.empty())
	{
		return "only a start has parameters";
	}
	if (!std::ranges::all_of(act.parameters, [](float value) { return std::isfinite(value); }))
	{
		return "a parameter is not a number";
	}
	if (act.act == Act::Breakpoint && act.index > 1)
	{
		return "a breakpoint goes on or off";
	}
	return std::nullopt;
}

ActResult ApplyScriptAct(const EditorScript& act, MachineInterface& machine)
{
	if (const auto refusal = ScriptActRefusal(act))
	{
		return {.refusal = *refusal};
	}
	if (UsesTask(act.act) && !machine.HasTask(act.id))
	{
		return {.refusal = "the machine has no such task"};
	}
	switch (act.act)
	{
	case Act::Start:
	{
		const auto script = machine.Script(act.id);
		if (!script.has_value())
		{
			return {.refusal = "the program has no such script"};
		}
		if (act.parameters.size() != script->parameters)
		{
			return {.refusal = "the count of the parameters is not the count of the script"};
		}
		const auto task = machine.Start(*script, act.parameters);
		if (task == 0)
		{
			return {.refusal = "the machine did not start the script"};
		}
		return {.done = true, .task = task};
	}
	case Act::StopScript:
	{
		const auto script = machine.Script(act.id);
		if (!script.has_value())
		{
			return {.refusal = "the program has no such script"};
		}
		machine.StopScript(script->name);
		return {.done = true};
	}
	case Act::HoldAll:
		for (const auto task : machine.Tasks())
		{
			machine.Hold(task);
		}
		return {.done = true};
	case Act::ContinueAll:
		for (const auto task : machine.Tasks())
		{
			machine.Continue(task);
		}
		return {.done = true};
	case Act::StepTask:
	case Act::ContinueTask:
		if (!machine.IsHeld(act.id))
		{
			return {.refusal = "the task is not held"};
		}
		act.act == Act::StepTask ? machine.Step(act.id) : machine.Continue(act.id);
		return {.done = true};
	case Act::HoldTask:
		machine.Hold(act.id);
		return {.done = true};
	case Act::StopTask:
		machine.Stop(act.id);
		return {.done = true};
	case Act::Breakpoint:
		if (act.address >= machine.CodeSize())
		{
			return {.refusal = "the address is not in the program"};
		}
		machine.SetBreakpoint(act.address, act.index == 1);
		return {.done = true};
	case Act::RemoveBreakpoints:
		for (const auto address : machine.Breakpoints())
		{
			machine.SetBreakpoint(address, false);
		}
		return {.done = true};
	case Act::SetGlobal:
		// The global 0 is the null variable of the machine
		if (act.id == 0 || act.id >= machine.GlobalCount())
		{
			return {.refusal = "the program has no such global"};
		}
		machine.SetGlobal(act.id, act.value);
		return {.done = true};
	case Act::SetTaskVariable:
		if (act.index >= machine.TaskVariableCount(act.id))
		{
			return {.refusal = "the task has no such local"};
		}
		machine.SetTaskVariable(act.id, act.index, act.value);
		return {.done = true};
	}
	return {.refusal = "the act is not one of the editor"};
}

ActResult RunScriptAct(const EditorScript& act)
{
	if (!Locator::vm::has_value())
	{
		return {.refusal = "the game has no script machine"};
	}
	GameMachine machine(Locator::vm::value());
	return ApplyScriptAct(act, machine);
}

std::optional<game_socket::EditorScriptFile::Kind> FileKindOf(const EditorScriptProgram& change) noexcept
{
	switch (change.use)
	{
	case Use::Compile:
		return FileKind::Source;
	case Use::Open:
		return FileKind::Program;
	case Use::GamesProgram:
		return std::nullopt;
	}
	return std::nullopt;
}

bool ReadsProgram(std::span<const uint8_t> bytes)
{
	if (bytes.empty())
	{
		return false;
	}
	try
	{
		lhvm::LHVMFile file;
		file.Open(std::vector<uint8_t>(bytes.begin(), bytes.end()));
		return file.IsLoaded() && !file.HasStatus();
	}
	catch (const std::exception&)
	{
		return false;
	}
}

std::optional<std::string_view> ProgramRefusal(const EditorScriptProgram& change, bool kept) noexcept
{
	if (change.use > Use::GamesProgram)
	{
		return "the change is not one of the editor";
	}
	if (FileKindOf(change).has_value() && !kept)
	{
		return "the game server has no file with the hash";
	}
	return std::nullopt;
}

ProgramResult ApplyProgram(const EditorScriptProgram& change, std::span<const uint8_t> file, const ProgramWorld& world)
{
	if (const auto refusal = ProgramRefusal(change, !file.empty()))
	{
		return {.refusal = std::string(*refusal)};
	}
	switch (change.use)
	{
	case Use::Compile:
	{
		const std::string_view source(reinterpret_cast<const char*>(file.data()), file.size());
		if (auto message = world.compile(change.script, source))
		{
			return {.refusal = std::move(*message)};
		}
		return {.done = true};
	}
	case Use::Open:
		if (!world.load(file))
		{
			return {.refusal = "the program does not read"};
		}
		return {.done = true};
	case Use::GamesProgram:
		if (!world.loadGames())
		{
			return {.refusal = "the program of the game does not read"};
		}
		return {.done = true};
	}
	return {.refusal = "the change is not one of the editor"};
}

ProgramResult RunProgram(const EditorScriptProgram& change, std::span<const uint8_t> file)
{
	if (!Locator::vm::has_value() || !Locator::scriptObjects::has_value())
	{
		return {.refusal = "the game has no script machine"};
	}
	auto& vm = Locator::vm::value();
	// As when the game starts its scripts again, the objects of the old program go first: what it made goes, the rest goes
	// back to the game
	const auto loadBytes = [&vm](std::span<const uint8_t> bytes) {
		try
		{
			Locator::scriptObjects::value().Reset();
			return vm.LoadBinary(std::vector<uint8_t>(bytes.begin(), bytes.end())) == EXIT_SUCCESS;
		}
		catch (const std::exception&)
		{
			return false;
		}
	};
	const ProgramWorld world {
	    .compile = [&vm](uint32_t scriptId, std::string_view source) -> std::optional<std::string> {
		    const auto* natives = vm.GetFunctions();
		    const scripts::Program program {
		        .code = vm.GetInstructions(),
		        .scripts = vm.GetScripts(),
		        .globals = vm.GetVariables(),
		        .natives = natives != nullptr ? std::span<const lhvm::NativeFunction>(*natives)
		                                      : std::span<const lhvm::NativeFunction> {},
		        .data = vm.GetData(),
		    };
		    const auto found = std::ranges::find(program.scripts, scriptId, &lhvm::VMScript::scriptId);
		    if (found == program.scripts.end())
		    {
			    return "the program has no such script";
		    }
		    const auto result = scripts::Compile(program, *found, source);
		    if (result.program == nullptr)
		    {
			    return result.diagnostics.empty() ? std::string("the source does not compile") : result.diagnostics.front();
		    }
		    if (vm.UpdateProgram(*result.program) != EXIT_SUCCESS)
		    {
			    return "the machine did not take the new program";
		    }
		    return std::nullopt;
	    },
	    .load = loadBytes,
	    .loadGames =
	        [&loadBytes]() {
		        if (!Locator::filesystem::has_value())
		        {
			        return false;
		        }
		        auto& fileSystem = Locator::filesystem::value();
		        const auto path = fileSystem.GetPath<filesystem::Path::Quests>() / "challenge.chl";
		        try
		        {
			        return fileSystem.Exists(path) && loadBytes(fileSystem.ReadAll(path));
		        }
		        catch (const std::exception&)
		        {
			        return false;
		        }
	        },
	};
	return ApplyProgram(change, file, world);
}

void ReadLandScript(LandScriptRun& run, std::string name, const std::string& text)
{
	run.name = std::move(name);
	try
	{
		run.read = lhscriptx::Script::Read(text);
		run.message = fmt::format("Read {} commands of {}", run.read.commands.size(), run.name);
	}
	catch (const std::exception& error)
	{
		run.read = {};
		run.message = fmt::format("Could not read {}: {}", run.name, error.what());
	}
	run.stepper.Reset(run.read.commands.size());
	++run.changes;
}

std::optional<std::string> LandScriptText(const std::filesystem::path& path)
{
	if (!Locator::filesystem::has_value())
	{
		return std::nullopt;
	}
	try
	{
		auto& fileSystem = Locator::filesystem::value();
		if (path.empty() || !fileSystem.Exists(path))
		{
			return std::nullopt;
		}
		const auto data = fileSystem.ReadAll(path);
		// A land of a network game holds its script in the same file as its landscape
		if (online_map_land::IsOnlineMap(path.generic_string()))
		{
			const auto file = onlinemap::Parse(std::as_bytes(std::span(data)));
			return file.has_value() ? std::optional(std::string(file->script)) : std::nullopt;
		}
		return std::string(reinterpret_cast<const char*>(data.data()), data.size());
	}
	catch (const std::exception&)
	{
		return std::nullopt;
	}
}

std::optional<std::string_view> LandScriptRefusal(const EditorLandScript& act, bool kept) noexcept
{
	if (act.act > LandAct::Breakpoint)
	{
		return "the act is not one of the editor";
	}
	if (act.act == LandAct::Open && !kept)
	{
		return "the game server has no file with the hash";
	}
	return std::nullopt;
}

LandScriptResult ApplyLandScript(const EditorLandScript& act, std::span<const uint8_t> file, LandScriptRun& run,
                                 const LandScriptWorld& world)
{
	if (const auto refusal = LandScriptRefusal(act, !file.empty()))
	{
		return {.refusal = *refusal};
	}
	auto& stepper = run.stepper;
	const auto doCommand = [&run, &world](size_t index) {
		if (index >= run.read.commands.size())
		{
			return false;
		}
		const auto& command = run.read.commands[index];
		if (const auto error = world.run(command))
		{
			run.message = fmt::format("Line {}: {} failed: {}", command.line, command.name, *error);
			return false;
		}
		run.message = fmt::format("Line {}: {} done", command.line, command.name);
		return true;
	};
	LandScriptResult result {.done = true};
	switch (act.act)
	{
	case LandAct::LoadedLand:
	{
		const auto path = world.loadedLand();
		if (!path.has_value())
		{
			return {.refusal = "no land is loaded"};
		}
		const auto text = world.readText(*path);
		if (!text.has_value())
		{
			run.read = {};
			stepper.Reset(0);
			run.name = path->filename().generic_string();
			run.message = fmt::format("The land script {} is not a file", path->generic_string());
			++run.changes;
			return {.refusal = "the land script does not read"};
		}
		ReadLandScript(run, path->filename().generic_string(), *text);
		return result;
	}
	case LandAct::Open:
		ReadLandScript(run, fmt::format("the land script {:016x} of the editor", act.file),
		               std::string(reinterpret_cast<const char*>(file.data()), file.size()));
		return result;
	case LandAct::Step:
		if (!stepper.Step(doCommand))
		{
			return {.refusal = "no command is left"};
		}
		result.commands = 1;
		break;
	case LandAct::Continue:
		result.commands = stepper.Continue(doCommand);
		break;
	case LandAct::RunTo:
		if (act.command >= stepper.Count())
		{
			return {.refusal = "the script has no such command"};
		}
		result.commands = stepper.RunTo(act.command, doCommand);
		break;
	case LandAct::Skip:
		if (!stepper.Next().has_value())
		{
			return {.refusal = "no command is left"};
		}
		stepper.Skip();
		break;
	case LandAct::Restart:
		stepper.Restart();
		break;
	case LandAct::Breakpoint:
		if (act.command >= stepper.Count())
		{
			return {.refusal = "the script has no such command"};
		}
		stepper.ToggleBreakpoint(act.command);
		break;
	}
	++run.changes;
	return result;
}

LandScriptWorld LocatorLandScriptWorld()
{
	return {
	    .loadedLand = []() -> std::optional<std::filesystem::path> {
		    if (!Locator::landSystem::has_value())
		    {
			    return std::nullopt;
		    }
		    return Locator::landSystem::value().GetLandPath();
	    },
	    .readText = LandScriptText,
	    .run = [](const lhscriptx::Command& command) -> std::optional<std::string> {
		    try
		    {
			    lhscriptx::Script script;
			    script.Run(command);
			    return std::nullopt;
		    }
		    catch (const std::exception& error)
		    {
			    return std::string(error.what());
		    }
	    },
	};
}

LandScriptResult RunLandScript(const EditorLandScript& act, std::span<const uint8_t> file)
{
	if (!Locator::editorSystem::has_value())
	{
		return {.refusal = "the game has no editor"};
	}
	return ApplyLandScript(act, file, Locator::editorSystem::value().GetLandScriptRun(), LocatorLandScriptWorld());
}

std::optional<uint64_t> SendFile(game_socket::EditorScriptFile::Kind kind, std::span<const uint8_t> bytes)
{
	if (bytes.empty() || bytes.size() > game_socket::k_MostEditorScriptFileBytes || !Locator::networkGame::has_value())
	{
		return std::nullopt;
	}
	if (!Locator::networkGame::value().SendEditorScriptFile(kind, {bytes.begin(), bytes.end()}))
	{
		return std::nullopt;
	}
	return mind_loads::HashOf(bytes);
}

} // namespace openblack::editor::script_acts
