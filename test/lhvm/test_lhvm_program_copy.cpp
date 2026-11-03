/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>
#include <cstdlib>

#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include <LHVM.h>
#include <gtest/gtest.h>

using namespace openblack::lhvm;

namespace
{

/// A small program with one script, saved while it runs: a value on the main stack, two globals and one task
LHVMFile RunningProgram()
{
	const std::vector<VMInstruction> code {
	    VMInstruction(Opcode::Push, VMMode::Immediate, DataType::Float, VMValue(1.0f), 3),
	    VMInstruction(Opcode::End, VMMode::Immediate, DataType::None, VMValue(), 4),
	};
	const std::vector<VMScript> scripts {VMScript("First", "first.txt", ScriptType::Script, 0, {"local"}, 0, 0, 1)};
	const std::vector<VMVar> globals {
	    VMVar(DataType::Float, VMValue(2.5f), "Counter"),
	    VMVar(DataType::Boolean, VMValue(1), "Flag"),
	};
	VMStack stack;
	stack.values.at(0) = VMValue(3u);
	stack.types.at(0) = DataType::Float;
	stack.count = 1;
	VMTask task({VMVar(DataType::Object, VMValue(42u), "local")}, 1, 1, 1, 0, VMStack(), "First", "first.txt",
	            ScriptType::Script);
	task.ticks = 12;
	return {LHVMVersion::BlackAndWhite,
	        {"Counter", "Flag"},
	        code,
	        {1},
	        scripts,
	        {'a', 'b', '\0'},
	        stack,
	        globals,
	        {task},
	        1234,
	        56,
	        1,
	        1,
	        98765};
}

/// The same program with one more instruction at its end, as a newer build of the script gives
LHVMFile LongerProgram()
{
	auto instructions = RunningProgram().GetInstructions();
	instructions.emplace_back(Opcode::End, VMMode::Immediate, DataType::None, VMValue(), 5);
	const auto running = RunningProgram();
	return {LHVMVersion::BlackAndWhite, running.GetVariablesNames(), instructions,
	        running.GetAutostart(),     running.GetScripts(),        running.GetData()};
}

std::string Bytes(const LHVMFile& file)
{
	std::ostringstream stream(std::ios::binary);
	file.Write(stream);
	return stream.str();
}

/// A machine with a native that saves the scripts while it runs, as a save from a script does
class LhvmProgramCopy: public ::testing::Test
{
protected:
	void SetUp() override
	{
		_vm.Initialise(&_natives, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
		const auto bytes = Bytes(RunningProgram());
		ASSERT_EQ(_vm.RestoreState(std::vector<uint8_t>(bytes.begin(), bytes.end())), EXIT_SUCCESS);
	}

	LHVM _vm;
	std::vector<std::string> _saved;
	std::vector<NativeFunction> _natives {
	    NativeFunction(nullptr, 0, 0, "NONE"),
	    // Takes one text, and saves the scripts with the text of the call still in their data
	    NativeFunction(
	        [this] {
		        _saved.push_back(Bytes(_vm.StateFile()));
		        _vm.Pop();
	        },
	        1, 0, "SAVE_WITH_TEXT"),
	};
};

} // namespace

TEST_F(LhvmProgramCopy, TheSaveOfARestoredGameIsTheFileThatItCameFrom)
{
	EXPECT_EQ(Bytes(_vm.StateFile()), Bytes(RunningProgram()));
}

TEST_F(LhvmProgramCopy, TheSharedProgramAndACopiedStateMakeTheSameFile)
{
	const auto state = _vm.CopyRunningState();
	EXPECT_EQ(Bytes(LHVM::StateFile(*_vm.ProgramFile(), state)), Bytes(_vm.StateFile()));
}

TEST_F(LhvmProgramCopy, TheVmKeepsOneCopyOfTheProgramUntilTheProgramChanges)
{
	const auto first = _vm.ProgramFile();
	EXPECT_EQ(_vm.ProgramFile(), first);
	ASSERT_EQ(_vm.UpdateProgram(LongerProgram()), EXIT_SUCCESS);
	const auto second = _vm.ProgramFile();
	EXPECT_NE(second, first);
	EXPECT_EQ(second->GetInstructions().size(), 3u);
	// A writer that kept the first copy still has the program as it was
	EXPECT_EQ(first->GetInstructions().size(), 2u);
	EXPECT_FALSE(first->HasStatus());
}

TEST_F(LhvmProgramCopy, AStateCopyDoesNotChangeWhenTheScriptsContinue)
{
	const auto state = _vm.CopyRunningState();
	const auto before = Bytes(LHVM::StateFile(*_vm.ProgramFile(), state));
	_vm.Pushi(7);
	EXPECT_EQ(Bytes(LHVM::StateFile(*_vm.ProgramFile(), state)), before);
	EXPECT_NE(Bytes(_vm.StateFile()), before);
}

TEST_F(LhvmProgramCopy, ASaveInANativeCallKeepsTheTextOfTheCall)
{
	const std::vector<NativeArgument> arguments {std::string("xyz")};
	std::vector<std::pair<VMValue, DataType>> results;
	ASSERT_TRUE(_vm.CallNative(1, arguments, results));
	ASSERT_EQ(_saved.size(), 1u);
	// The save in the call has the text after the scripts' own data
	EXPECT_NE(_saved[0].find(std::string("ab\0xyz\0", 7)), std::string::npos);
	// After the call, the save has the scripts' own data only
	EXPECT_EQ(_vm.ProgramFile()->GetData(), RunningProgram().GetData());
	EXPECT_EQ(_vm.StateFile().GetData(), RunningProgram().GetData());
}

// A world state from a game server can carry any program. The game restores only the state of the program that it loaded.
TEST_F(LhvmProgramCopy, OnlyTheStateOfTheLoadedProgramRuns)
{
	EXPECT_TRUE(_vm.RunsLoadedProgram(RunningProgram()));
	EXPECT_TRUE(_vm.RunsLoadedProgram(_vm.StateFile()));
	// Another program, or the same program with other code
	EXPECT_FALSE(_vm.RunsLoadedProgram(LongerProgram()));
	const auto running = RunningProgram();
	auto changed = running.GetInstructions();
	changed.at(0).data = VMValue(2.0f);
	EXPECT_FALSE(
	    _vm.RunsLoadedProgram({LHVMVersion::BlackAndWhite, running.GetVariablesNames(), changed, running.GetAutostart(),
	                           running.GetScripts(), running.GetData(), running.GetStack(), running.GetVariablesValues(),
	                           running.GetTasks(), running.GetTicks(), running.GetCurrentLineNumber(),
	                           running.GetHighestTaskId(), running.GetHighestScriptId(), running.GetExecutedInstructions()}));
	// A task that goes on at an instruction that the program does not have
	auto tasks = running.GetTasks();
	tasks.at(0).instructionAddress = 2;
	EXPECT_FALSE(
	    _vm.RunsLoadedProgram({LHVMVersion::BlackAndWhite, running.GetVariablesNames(), running.GetInstructions(),
	                           running.GetAutostart(), running.GetScripts(), running.GetData(), running.GetStack(),
	                           running.GetVariablesValues(), tasks, running.GetTicks(), running.GetCurrentLineNumber(),
	                           running.GetHighestTaskId(), running.GetHighestScriptId(), running.GetExecutedInstructions()}));
}
