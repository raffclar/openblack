/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>
#include <cstdint>

#include <algorithm>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "Editor/EditorMindLoads.h"
#include "Editor/EditorScriptActs.h"
#include "Editor/EditorScriptFiles.h"

using namespace openblack;
using namespace openblack::editor::script_acts;
using Act = EditorScript::Act;
using LandAct = EditorLandScript::Act;
using Use = EditorScriptProgram::Use;
using Kind = game_socket::EditorScriptFile::Kind;
namespace script_files = openblack::editor::script_files;

namespace
{

/// A script machine with two scripts, two tasks, three globals and a program of 100 instructions. It keeps each act.
class FakeMachine final: public MachineInterface
{
public:
	struct Task
	{
		bool held {false};
		uint32_t steps {0};
		std::vector<uint32_t> locals;
		std::string script;
	};

	std::map<uint32_t, ScriptInfo> scripts {{1, {.name = "Intro", .parameters = 0}}, {2, {.name = "Cow", .parameters = 2}}};
	std::map<uint32_t, Task> tasks {{10, {.locals = {0, 0}, .script = "Intro"}}, {11, {.locals = {0}, .script = "Cow"}}};
	std::vector<uint32_t> globals {0, 0, 0};
	std::set<uint32_t> breakpoints;
	std::vector<float> lastParameters;
	uint32_t nextTask {20};

	[[nodiscard]] std::optional<ScriptInfo> Script(uint32_t id) const override
	{
		const auto found = scripts.find(id);
		return found != scripts.end() ? std::optional(found->second) : std::nullopt;
	}
	[[nodiscard]] bool HasTask(uint32_t task) const override { return tasks.contains(task); }
	[[nodiscard]] std::vector<uint32_t> Tasks() const override
	{
		std::vector<uint32_t> ids;
		for (const auto& [id, task] : tasks)
		{
			ids.push_back(id);
		}
		return ids;
	}
	[[nodiscard]] bool IsHeld(uint32_t task) const override { return tasks.at(task).held; }
	[[nodiscard]] size_t CodeSize() const override { return 100; }
	[[nodiscard]] size_t GlobalCount() const override { return globals.size(); }
	[[nodiscard]] size_t TaskVariableCount(uint32_t task) const override { return tasks.at(task).locals.size(); }
	uint32_t Start(const ScriptInfo& script, std::span<const float> parameters) override
	{
		lastParameters.assign(parameters.begin(), parameters.end());
		tasks[nextTask] = {.script = script.name};
		return nextTask++;
	}
	void StopScript(const std::string& name) override
	{
		std::erase_if(tasks, [&name](const auto& entry) { return entry.second.script == name; });
	}
	void Hold(uint32_t task) override { tasks.at(task).held = true; }
	void Continue(uint32_t task) override { tasks.at(task).held = false; }
	void Step(uint32_t task) override { ++tasks.at(task).steps; }
	void Stop(uint32_t task) override { tasks.erase(task); }
	void SetBreakpoint(uint32_t address, bool on) override
	{
		on ? static_cast<void>(breakpoints.insert(address)) : static_cast<void>(breakpoints.erase(address));
	}
	[[nodiscard]] std::vector<uint32_t> Breakpoints() const override { return {breakpoints.begin(), breakpoints.end()}; }
	void SetGlobal(uint32_t id, uint32_t bits) override { globals.at(id) = bits; }
	void SetTaskVariable(uint32_t task, size_t index, uint32_t bits) override { tasks.at(task).locals.at(index) = bits; }
};

[[nodiscard]] std::vector<uint8_t> BytesOf(std::string_view text)
{
	return {text.begin(), text.end()};
}

/// A made-up land script of three commands
constexpr std::string_view k_Land = "VERSION(2.3)\n"
                                    "SET_LAND_NUMBER(2)\n"
                                    "CREATE_TOWN(0, \"10.0,20.0\", \"PLAYER_ONE\", 0, \"Norse\")\n";

/// A world that keeps each command that it does, and fails the command with a name
struct FakeLandWorld
{
	std::vector<std::string> done;
	std::string fails;
	std::optional<std::filesystem::path> land {"Land2.txt"};
	std::map<std::filesystem::path, std::string> files {{"Land2.txt", std::string(k_Land)}};

	[[nodiscard]] LandScriptWorld World()
	{
		return {
		    .loadedLand = [this]() { return land; },
		    .readText = [this](const std::filesystem::path& path) -> std::optional<std::string> {
			    const auto found = files.find(path);
			    return found != files.end() ? std::optional(found->second) : std::nullopt;
		    },
		    .run = [this](const lhscriptx::Command& command) -> std::optional<std::string> {
			    if (command.name == fails)
			    {
				    return "it fails";
			    }
			    done.push_back(command.name);
			    return std::nullopt;
		    },
		};
	}
};

} // namespace

TEST(EditorScriptActs, AStartGivesTheParametersAndTheNewTask)
{
	FakeMachine machine;
	const auto result = ApplyScriptAct({.act = Act::Start, .id = 2, .parameters = {1.5f, -3.0f}}, machine);
	EXPECT_TRUE(result.done);
	EXPECT_EQ(result.task, 20u);
	EXPECT_EQ(machine.lastParameters, (std::vector<float> {1.5f, -3.0f}));
	// The count of the parameters must be the count of the script, and the script must be in the program
	EXPECT_FALSE(ApplyScriptAct({.act = Act::Start, .id = 2, .parameters = {1.0f}}, machine).done);
	EXPECT_FALSE(ApplyScriptAct({.act = Act::Start, .id = 9}, machine).done);
	EXPECT_EQ(machine.tasks.size(), 3u);
}

TEST(EditorScriptActs, TheTaskActsNeedATaskAndAStepNeedsAHeldTask)
{
	FakeMachine machine;
	EXPECT_FALSE(ApplyScriptAct({.act = Act::StepTask, .id = 10}, machine).done);
	EXPECT_TRUE(ApplyScriptAct({.act = Act::HoldTask, .id = 10}, machine).done);
	EXPECT_TRUE(ApplyScriptAct({.act = Act::StepTask, .id = 10}, machine).done);
	EXPECT_EQ(machine.tasks.at(10).steps, 1u);
	EXPECT_TRUE(ApplyScriptAct({.act = Act::ContinueTask, .id = 10}, machine).done);
	EXPECT_FALSE(machine.tasks.at(10).held);
	EXPECT_FALSE(ApplyScriptAct({.act = Act::HoldTask, .id = 99}, machine).done);
	EXPECT_TRUE(ApplyScriptAct({.act = Act::HoldAll}, machine).done);
	EXPECT_TRUE(std::ranges::all_of(machine.tasks, [](const auto& entry) { return entry.second.held; }));
	EXPECT_TRUE(ApplyScriptAct({.act = Act::ContinueAll}, machine).done);
	EXPECT_TRUE(std::ranges::none_of(machine.tasks, [](const auto& entry) { return entry.second.held; }));
	EXPECT_TRUE(ApplyScriptAct({.act = Act::StopTask, .id = 11}, machine).done);
	EXPECT_FALSE(machine.tasks.contains(11));
	EXPECT_TRUE(ApplyScriptAct({.act = Act::StopScript, .id = 1}, machine).done);
	EXPECT_TRUE(machine.tasks.empty());
}

TEST(EditorScriptActs, ABreakpointMustBeInTheProgram)
{
	FakeMachine machine;
	EXPECT_TRUE(ApplyScriptAct({.act = Act::Breakpoint, .address = 5, .index = 1}, machine).done);
	EXPECT_TRUE(ApplyScriptAct({.act = Act::Breakpoint, .address = 7, .index = 1}, machine).done);
	EXPECT_FALSE(ApplyScriptAct({.act = Act::Breakpoint, .address = 100, .index = 1}, machine).done);
	EXPECT_EQ(machine.breakpoints, (std::set<uint32_t> {5, 7}));
	EXPECT_TRUE(ApplyScriptAct({.act = Act::Breakpoint, .address = 5, .index = 0}, machine).done);
	EXPECT_EQ(machine.breakpoints, (std::set<uint32_t> {7}));
	EXPECT_TRUE(ApplyScriptAct({.act = Act::RemoveBreakpoints}, machine).done);
	EXPECT_TRUE(machine.breakpoints.empty());
	// A breakpoint goes on or off, and nothing else
	EXPECT_TRUE(ScriptActRefusal({.act = Act::Breakpoint, .index = 2}).has_value());
}

TEST(EditorScriptActs, AValueGoesOnlyToAVariableThatIsThere)
{
	FakeMachine machine;
	EXPECT_TRUE(ApplyScriptAct({.act = Act::SetGlobal, .id = 2, .value = 0x3F800000U}, machine).done);
	EXPECT_EQ(machine.globals[2], 0x3F800000U);
	// The global 0 is the null variable of the machine
	EXPECT_FALSE(ApplyScriptAct({.act = Act::SetGlobal, .id = 0, .value = 1}, machine).done);
	EXPECT_FALSE(ApplyScriptAct({.act = Act::SetGlobal, .id = 3, .value = 1}, machine).done);
	EXPECT_TRUE(ApplyScriptAct({.act = Act::SetTaskVariable, .id = 10, .index = 1, .value = 9}, machine).done);
	EXPECT_EQ(machine.tasks.at(10).locals[1], 9u);
	EXPECT_FALSE(ApplyScriptAct({.act = Act::SetTaskVariable, .id = 10, .index = 2, .value = 9}, machine).done);
}

TEST(EditorScriptActs, TheServerRefusesAnActWithBadParameters)
{
	EXPECT_FALSE(ScriptActRefusal({.act = Act::Start, .parameters = {1.0f, 2.0f}}).has_value());
	EXPECT_TRUE(ScriptActRefusal({.act = Act::Start, .parameters = {std::numeric_limits<float>::quiet_NaN()}}).has_value());
	EXPECT_TRUE(ScriptActRefusal({.act = Act::HoldAll, .parameters = {1.0f}}).has_value());
	EXPECT_TRUE(ScriptActRefusal(
	                {.act = Act::Start, .parameters = std::vector<float>(player_commands::k_MostEditorScriptParameters + 1)})
	                .has_value());
	EXPECT_TRUE(ScriptActRefusal({.act = static_cast<Act>(12)}).has_value());
}

TEST(EditorScriptActs, AChangeOfTheProgramNeedsItsFileAndGivesTheMessageOfTheCompiler)
{
	std::vector<std::string> compiled;
	std::vector<std::vector<uint8_t>> loaded;
	int games = 0;
	const ProgramWorld world {
	    .compile = [&compiled](uint32_t script, std::string_view source) -> std::optional<std::string> {
		    if (source.find("bad") != std::string_view::npos)
		    {
			    return "1:1: the word is not known";
		    }
		    compiled.push_back(std::to_string(script) + ":" + std::string(source));
		    return std::nullopt;
	    },
	    .load =
	        [&loaded](std::span<const uint8_t> bytes) {
		        loaded.emplace_back(bytes.begin(), bytes.end());
		        return bytes.front() == 'P';
	        },
	    .loadGames = [&games]() { return ++games > 0; },
	};
	const auto source = BytesOf("begin script X");
	EXPECT_TRUE(ApplyProgram({.use = Use::Compile, .file = 1, .script = 4}, source, world).done);
	EXPECT_EQ(compiled, (std::vector<std::string> {"4:begin script X"}));
	const auto bad = ApplyProgram({.use = Use::Compile, .file = 1, .script = 4}, BytesOf("bad"), world);
	EXPECT_FALSE(bad.done);
	EXPECT_EQ(bad.refusal, "1:1: the word is not known");
	// No file, no change
	EXPECT_FALSE(ApplyProgram({.use = Use::Open, .file = 1}, {}, world).done);
	EXPECT_TRUE(loaded.empty());
	EXPECT_TRUE(ApplyProgram({.use = Use::Open, .file = 1}, BytesOf("PROGRAM"), world).done);
	EXPECT_FALSE(ApplyProgram({.use = Use::Open, .file = 1}, BytesOf("XROGRAM"), world).done);
	// The program of the game uses no file
	EXPECT_TRUE(ApplyProgram({.use = Use::GamesProgram}, {}, world).done);
	EXPECT_EQ(games, 1);
	EXPECT_FALSE(ProgramRefusal({.use = Use::GamesProgram}, false).has_value());
	EXPECT_TRUE(ProgramRefusal({.use = Use::Open}, false).has_value());
	EXPECT_EQ(FileKindOf({.use = Use::Compile}), Kind::Source);
	EXPECT_EQ(FileKindOf({.use = Use::Open}), Kind::Program);
}

TEST(EditorScriptActs, TheLandScriptActsStepTheSameCommandsOnEveryRun)
{
	FakeLandWorld world;
	LandScriptRun run;
	ASSERT_TRUE(ApplyLandScript({.act = LandAct::LoadedLand}, {}, run, world.World()).done);
	ASSERT_EQ(run.read.commands.size(), 3u);
	EXPECT_EQ(run.name, "Land2.txt");
	EXPECT_TRUE(ApplyLandScript({.act = LandAct::Breakpoint, .command = 2}, {}, run, world.World()).done);
	EXPECT_TRUE(run.stepper.HasBreakpoint(2));
	EXPECT_EQ(ApplyLandScript({.act = LandAct::Step}, {}, run, world.World()).commands, 1u);
	// The continue stops before the command with the breakpoint
	EXPECT_EQ(ApplyLandScript({.act = LandAct::Continue}, {}, run, world.World()).commands, 1u);
	EXPECT_EQ(world.done, (std::vector<std::string> {"VERSION", "SET_LAND_NUMBER"}));
	EXPECT_TRUE(ApplyLandScript({.act = LandAct::Skip}, {}, run, world.World()).done);
	EXPECT_FALSE(run.stepper.Next().has_value());
	EXPECT_FALSE(ApplyLandScript({.act = LandAct::Step}, {}, run, world.World()).done);
	EXPECT_TRUE(ApplyLandScript({.act = LandAct::Restart}, {}, run, world.World()).done);
	EXPECT_EQ(ApplyLandScript({.act = LandAct::RunTo, .command = 2}, {}, run, world.World()).commands, 2u);
	EXPECT_FALSE(ApplyLandScript({.act = LandAct::RunTo, .command = 3}, {}, run, world.World()).done);
	EXPECT_FALSE(ApplyLandScript({.act = LandAct::Breakpoint, .command = 3}, {}, run, world.World()).done);
	// A command that fails tells why
	world.fails = "CREATE_TOWN";
	EXPECT_EQ(ApplyLandScript({.act = LandAct::Step}, {}, run, world.World()).commands, 1u);
	EXPECT_EQ(run.stepper.StateOf(2), editor::scripts::LandScriptStepper::State::Failed);
	EXPECT_NE(run.message.find("failed: it fails"), std::string::npos);
}

TEST(EditorScriptActs, AKeptRunOfTheLandScriptComesBackAtTheSamePosition)
{
	FakeLandWorld world;
	LandScriptRun run;
	ASSERT_TRUE(ApplyLandScript({.act = LandAct::LoadedLand}, {}, run, world.World()).done);
	ASSERT_TRUE(ApplyLandScript({.act = LandAct::Breakpoint, .command = 2}, {}, run, world.World()).done);
	ASSERT_TRUE(ApplyLandScript({.act = LandAct::Step}, {}, run, world.World()).done);
	ASSERT_TRUE(ApplyLandScript({.act = LandAct::Skip}, {}, run, world.World()).done);
	const auto kept = KeepLandScriptRun(run);
	EXPECT_EQ(kept.name, "Land2.txt");
	EXPECT_EQ(kept.text, k_Land);

	// A computer with no run, such as a restarted server, gets the same run
	LandScriptRun restored;
	ASSERT_TRUE(RestoreLandScriptRun(restored, kept));
	EXPECT_EQ(KeepLandScriptRun(restored).stepper, kept.stepper);
	EXPECT_EQ(restored.read.commands.size(), 3u);
	EXPECT_EQ(restored.stepper.Next(), std::optional<size_t> {2});
	EXPECT_TRUE(restored.stepper.HasBreakpoint(2));
	EXPECT_EQ(restored.stepper.StateOf(1), editor::scripts::LandScriptStepper::State::Skipped);
	EXPECT_EQ(restored.message, run.message);
	EXPECT_GT(restored.changes, kept.changes);

	// The next step does the same command on both runs
	FakeLandWorld other;
	ASSERT_TRUE(ApplyLandScript({.act = LandAct::Step}, {}, run, world.World()).done);
	ASSERT_TRUE(ApplyLandScript({.act = LandAct::Step}, {}, restored, other.World()).done);
	EXPECT_EQ(world.done.back(), "CREATE_TOWN");
	EXPECT_EQ(other.done, (std::vector<std::string> {"CREATE_TOWN"}));
}

TEST(EditorScriptActs, AKeptRunThatDoesNotAgreeWithItsTextStartsAtTheFirstCommand)
{
	LandScriptRun run;
	KeptLandScriptRun kept {.name = "Land2.txt", .text = std::string(k_Land), .changes = 4};
	kept.stepper.states.assign(2, editor::scripts::LandScriptStepper::State::Done);
	kept.stepper.next = 2;
	EXPECT_FALSE(RestoreLandScriptRun(run, kept));
	EXPECT_EQ(run.read.commands.size(), 3u);
	EXPECT_EQ(run.stepper.Next(), std::optional<size_t> {0});
	// A kept run that read no script gives a run with no script
	ASSERT_TRUE(RestoreLandScriptRun(run, {}));
	EXPECT_EQ(run.changes, 0u);
	EXPECT_TRUE(run.read.commands.empty());
}

TEST(EditorScriptActs, AnOpenReadsTheTextOfTheFileAndNeedsTheFile)
{
	FakeLandWorld world;
	LandScriptRun run;
	const auto text = BytesOf("VERSION(2.3)\nSET_LAND_NUMBER(1)\n");
	const auto before = run.changes;
	EXPECT_TRUE(ApplyLandScript({.act = LandAct::Open, .file = 5}, text, run, world.World()).done);
	EXPECT_EQ(run.read.commands.size(), 2u);
	EXPECT_GT(run.changes, before);
	EXPECT_FALSE(ApplyLandScript({.act = LandAct::Open, .file = 5}, {}, run, world.World()).done);
	EXPECT_TRUE(LandScriptRefusal({.act = LandAct::Open}, false).has_value());
	EXPECT_FALSE(LandScriptRefusal({.act = LandAct::Step}, false).has_value());
	// No land: the run stays
	world.land.reset();
	EXPECT_FALSE(ApplyLandScript({.act = LandAct::LoadedLand}, {}, run, world.World()).done);
	EXPECT_EQ(run.read.commands.size(), 2u);
}

TEST(EditorScriptFiles, TheSplitPartsComeTogetherAgainWithTheHash)
{
	std::vector<uint8_t> file((2 * game_socket::k_MostEditorScriptPartBytes) + 10);
	for (size_t i = 0; i < file.size(); ++i)
	{
		file[i] = static_cast<uint8_t>(i * 13);
	}
	const auto parts = script_files::SplitFile(Kind::Program, file);
	ASSERT_EQ(parts.size(), 3u);
	EXPECT_EQ(parts[0].bytes.size(), game_socket::k_MostEditorScriptPartBytes);
	EXPECT_EQ(parts[2].bytes.size(), 10u);
	EXPECT_EQ(parts[1].hash, editor::mind_loads::HashOf(file));
	script_files::PartAssembly assembly;
	EXPECT_EQ(assembly.Add(parts[0]), script_files::PartAssembly::Step::Waiting);
	EXPECT_FALSE(assembly.Take().has_value());
	EXPECT_EQ(assembly.Add(parts[1]), script_files::PartAssembly::Step::Waiting);
	EXPECT_EQ(assembly.Add(parts[2]), script_files::PartAssembly::Step::Done);
	const auto whole = assembly.Take();
	ASSERT_TRUE(whole.has_value());
	EXPECT_EQ(whole->bytes, file);
	EXPECT_EQ(whole->kind, Kind::Program);
	// No file, and no file over the limit
	EXPECT_TRUE(script_files::SplitFile(Kind::Source, {}).empty());
	EXPECT_TRUE(
	    script_files::SplitFile(Kind::Source, std::vector<uint8_t>(game_socket::k_MostEditorScriptFileBytes + 1)).empty());
}

TEST(EditorScriptFiles, APartOutOfOrderOrBytesWithAnotherHashStopTheFile)
{
	std::vector<uint8_t> file((game_socket::k_MostEditorScriptPartBytes) + 5, 3);
	auto parts = script_files::SplitFile(Kind::LandScript, file);
	ASSERT_EQ(parts.size(), 2u);
	script_files::PartAssembly assembly;
	// The second part with no first part
	EXPECT_EQ(assembly.Add(parts[1]), script_files::PartAssembly::Step::Bad);
	EXPECT_EQ(assembly.Add(parts[0]), script_files::PartAssembly::Step::Waiting);
	auto changed = parts[1];
	changed.bytes[0] = 4;
	EXPECT_EQ(assembly.Add(changed), script_files::PartAssembly::Step::Bad);
	// A new first part starts the file again
	EXPECT_EQ(assembly.Add(parts[0]), script_files::PartAssembly::Step::Waiting);
	EXPECT_EQ(assembly.Add(parts[1]), script_files::PartAssembly::Step::Done);
}

TEST(EditorScriptFiles, TheStoreKeepsEachFileOneTimeByItsKindAndHash)
{
	script_files::FileStore store;
	const auto file = BytesOf("VERSION(2.3)");
	const auto [added, hash] = store.Add(Kind::LandScript, file);
	EXPECT_EQ(added, script_files::FileStore::Added::New);
	EXPECT_EQ(store.Add(Kind::LandScript, file).first, script_files::FileStore::Added::Known);
	EXPECT_TRUE(store.Find(Kind::Source, hash).empty());
	EXPECT_EQ(store.Find(Kind::LandScript, hash).size(), file.size());
	EXPECT_EQ(store.Add(Kind::Source, {}).first, script_files::FileStore::Added::Refused);
	for (size_t i = 1; i < script_files::k_MostKeptFiles; ++i)
	{
		EXPECT_EQ(store.Add(Kind::Source, {static_cast<uint8_t>(i)}).first, script_files::FileStore::Added::New);
	}
	EXPECT_EQ(store.Add(Kind::Source, {200}).first, script_files::FileStore::Added::Refused);
}
