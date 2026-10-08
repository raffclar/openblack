/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The script machine's debugger: breakpoints, held tasks, stepping and continuing, and changing variables. Also that a
// machine with no debugger state, or with a breakpoint no task reaches, runs exactly as before. And the two tools the
// debugger leans on: writing a program out, and taking a newer build of the running program.

#include <cstdlib>

#include <functional>
#include <sstream>
#include <string>
#include <vector>

#include <LHVM.h>
#include <gtest/gtest.h>

using namespace openblack;
using lhvm::DataType;
using lhvm::Opcode;
using lhvm::VMInstruction;
using lhvm::VMMode;
using lhvm::VMScript;
using lhvm::VMValue;

namespace
{
VMInstruction Make(Opcode code, VMValue data = VMValue(uint32_t {0}), DataType type = DataType::Int,
                   VMMode mode = VMMode::Immediate)
{
	return {code, mode, type, data, 0};
}

/// A machine running a program whose one script counts up a global by one each turn, round and round
class DebuggerTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		const std::vector<VMInstruction> code {
		    Make(Opcode::Push, VMValue(uint32_t {1}), DataType::Int, VMMode::Reference), // 0: PUSHI counter
		    Make(Opcode::Push, VMValue(int32_t {1})),                                    // 1: PUSHI 1
		    Make(Opcode::Add, VMValue(uint32_t {0})),                                    // 2: ADDI
		    Make(Opcode::Pop, VMValue(uint32_t {1}), DataType::Int, VMMode::Reference),  // 3: POPI counter
		    Make(Opcode::Jmp, VMValue(uint32_t {0})),                                    // 4: JMP 0, back, which ends the turn
		    Make(Opcode::End),                                                           // 5
		};
		const std::vector<VMScript> scripts {VMScript("Counter", "test.txt", lhvm::ScriptType::Script, 1, {}, 0, 0, 1)};
		_vm.Initialise(&_natives, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
		ASSERT_EQ(_vm.LoadBinary(lhvm::LHVMFile(lhvm::LHVMVersion::BlackAndWhite, {"counter"}, code, {}, scripts, {})),
		          EXIT_SUCCESS);
		_task = _vm.StartScript("Counter", lhvm::ScriptType::All);
		ASSERT_NE(_task, 0u);
	}

	[[nodiscard]] int32_t Counter() const { return _vm.GetVariables().at(1).value.intVal; }
	[[nodiscard]] uint32_t Address() const { return _vm.GetTasks().at(_task).instructionAddress; }

	std::vector<lhvm::NativeFunction> _natives {lhvm::NativeFunction(nullptr, 0, 0, "NONE")};
	lhvm::LHVM _vm;
	uint32_t _task {0};
};

/// What a run of the two-counter program below did, turn by turn
struct Trace
{
	std::vector<uint32_t> callers; // the task calling the native, at each call
	std::vector<int32_t> counters; // both globals, after each turn
	std::vector<uint32_t> machine; // each task's address and tick count, after each turn

	bool operator==(const Trace&) const = default;
};

/// Runs a program of two scripts for some turns, each counting up its own global and calling a native that notes which
/// task called it. `prepare` may set up debugger state before the first turn.
Trace RunTwoCounters(const std::function<void(lhvm::LHVM&, uint32_t, uint32_t)>& prepare)
{
	const std::vector<VMInstruction> code {
	    Make(Opcode::Push, VMValue(uint32_t {1}), DataType::Int, VMMode::Reference), // 0: PUSHI first
	    Make(Opcode::Push, VMValue(int32_t {1})),                                    // 1: PUSHI 1
	    Make(Opcode::Add, VMValue(uint32_t {0})),                                    // 2: ADDI
	    Make(Opcode::Pop, VMValue(uint32_t {1}), DataType::Int, VMMode::Reference),  // 3: POPI first
	    Make(Opcode::Sys, VMValue(uint32_t {1})),                                    // 4: SYS NOTE
	    Make(Opcode::Jmp, VMValue(uint32_t {0})),                                    // 5: JMP 0
	    Make(Opcode::Push, VMValue(uint32_t {2}), DataType::Int, VMMode::Reference), // 6: PUSHI second
	    Make(Opcode::Push, VMValue(int32_t {2})),                                    // 7: PUSHI 2
	    Make(Opcode::Add, VMValue(uint32_t {0})),                                    // 8: ADDI
	    Make(Opcode::Pop, VMValue(uint32_t {2}), DataType::Int, VMMode::Reference),  // 9: POPI second
	    Make(Opcode::Sys, VMValue(uint32_t {1})),                                    // 10: SYS NOTE
	    Make(Opcode::Jmp, VMValue(uint32_t {6})),                                    // 11: JMP 6
	    Make(Opcode::End),                                                           // 12: never reached
	};
	const std::vector<VMScript> scripts {
	    VMScript("First", "test.txt", lhvm::ScriptType::Script, 2, {}, 0, 0, 1),
	    VMScript("Second", "test.txt", lhvm::ScriptType::Script, 2, {}, 6, 0, 2),
	};

	Trace trace;
	lhvm::LHVM vm;
	const std::vector<lhvm::NativeFunction> natives {
	    lhvm::NativeFunction(nullptr, 0, 0, "NONE"),
	    lhvm::NativeFunction([&]() { trace.callers.push_back(vm.GetCurrentTaskNumber()); }, 0, 0, "NOTE"),
	};
	vm.Initialise(&natives, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
	EXPECT_EQ(vm.LoadBinary(lhvm::LHVMFile(lhvm::LHVMVersion::BlackAndWhite, {"first", "second"}, code, {}, scripts, {})),
	          EXIT_SUCCESS);
	const auto first = vm.StartScript("First", lhvm::ScriptType::All);
	const auto second = vm.StartScript("Second", lhvm::ScriptType::All);
	prepare(vm, first, second);
	for (int turn = 0; turn < 5; ++turn)
	{
		vm.LookIn(lhvm::ScriptType::All);
		trace.counters.push_back(vm.GetVariables().at(1).value.intVal);
		trace.counters.push_back(vm.GetVariables().at(2).value.intVal);
		for (const auto& [id, task] : vm.GetTasks())
		{
			trace.machine.push_back(id);
			trace.machine.push_back(task.instructionAddress);
			trace.machine.push_back(task.ticks);
		}
	}
	return trace;
}
} // namespace

TEST_F(DebuggerTest, RunsFreelyWithoutTheDebugger)
{
	_vm.LookIn(lhvm::ScriptType::All);
	_vm.LookIn(lhvm::ScriptType::All);
	EXPECT_EQ(Counter(), 2);
	EXPECT_FALSE(_vm.IsTaskHeld(_task));
}

TEST_F(DebuggerTest, BreakpointsHoldTasksBeforeTheirInstruction)
{
	_vm.SetBreakpoint(3, true);
	_vm.LookIn(lhvm::ScriptType::All);
	EXPECT_TRUE(_vm.IsTaskHeld(_task));
	EXPECT_EQ(Address(), 3u);
	EXPECT_EQ(Counter(), 0);
	// Held, it sits out the turns
	_vm.LookIn(lhvm::ScriptType::All);
	EXPECT_EQ(Address(), 3u);
	EXPECT_EQ(Counter(), 0);
}

TEST_F(DebuggerTest, SteppingRunsOneInstructionAtATime)
{
	_vm.SetBreakpoint(3, true);
	_vm.LookIn(lhvm::ScriptType::All);
	_vm.StepTask(_task);
	_vm.LookIn(lhvm::ScriptType::All);
	EXPECT_EQ(Counter(), 1);
	EXPECT_EQ(Address(), 4u);
	EXPECT_TRUE(_vm.IsTaskHeld(_task));
}

TEST_F(DebuggerTest, ContinuingRunsPastTheBreakpointUntilItComesRound)
{
	_vm.SetBreakpoint(3, true);
	_vm.LookIn(lhvm::ScriptType::All);
	_vm.ContinueTask(_task);
	EXPECT_FALSE(_vm.IsTaskHeld(_task));
	_vm.LookIn(lhvm::ScriptType::All);
	EXPECT_EQ(Counter(), 1);
	// The next time round it stops there again
	_vm.LookIn(lhvm::ScriptType::All);
	EXPECT_TRUE(_vm.IsTaskHeld(_task));
	EXPECT_EQ(Address(), 3u);
	EXPECT_EQ(Counter(), 1);
	_vm.SetBreakpoint(3, false);
	_vm.ContinueTask(_task);
	_vm.LookIn(lhvm::ScriptType::All);
	_vm.LookIn(lhvm::ScriptType::All);
	EXPECT_EQ(Counter(), 3);
}

TEST_F(DebuggerTest, HoldsByHandAndChangesVariables)
{
	_vm.HoldTask(_task);
	_vm.LookIn(lhvm::ScriptType::All);
	EXPECT_EQ(Counter(), 0);
	_vm.SetVariable(1, VMValue(int32_t {40}));
	_vm.ContinueTask(_task);
	_vm.LookIn(lhvm::ScriptType::All);
	EXPECT_EQ(Counter(), 41);
	// Stopping a held task forgets it was held
	_vm.HoldTask(_task);
	_vm.StopTask(_task);
	EXPECT_FALSE(_vm.IsTaskHeld(_task));
}

TEST(DebuggerIdle, NoDebuggerStateRunsAsBefore)
{
	const auto trace = RunTwoCounters([](lhvm::LHVM&, uint32_t, uint32_t) {});
	// Each turn, the first task then the second, each running its script once round
	ASSERT_EQ(trace.callers.size(), 10u);
	for (size_t i = 0; i < trace.callers.size(); i += 2)
	{
		EXPECT_EQ(trace.callers[i], 1u);
		EXPECT_EQ(trace.callers[i + 1], 2u);
	}
	const std::vector<int32_t> counters {1, 2, 2, 4, 3, 6, 4, 8, 5, 10};
	EXPECT_EQ(trace.counters, counters);
	// After each turn both tasks are back at the start of their scripts, a tick older
	ASSERT_EQ(trace.machine.size(), 30u);
	for (uint32_t turn = 0; turn < 5; ++turn)
	{
		const std::vector<uint32_t> expected {1, 0, turn + 2, 2, 6, turn + 2};
		EXPECT_EQ(std::vector<uint32_t>(trace.machine.begin() + turn * 6, trace.machine.begin() + turn * 6 + 6), expected);
	}
}

TEST(DebuggerIdle, BreakpointsNoTaskReachesChangeNothing)
{
	const auto plain = RunTwoCounters([](lhvm::LHVM&, uint32_t, uint32_t) {});
	// A breakpoint on an instruction that never runs, one set and taken away again, and steps of tasks not held
	const auto unreached = RunTwoCounters([](lhvm::LHVM& vm, uint32_t first, uint32_t second) {
		vm.SetBreakpoint(12, true);
		vm.SetBreakpoint(3, true);
		vm.SetBreakpoint(3, false);
		vm.StepTask(first);
		vm.ContinueTask(second);
	});
	EXPECT_EQ(unreached, plain);
	// Held and continued before it ran: it starts where it was, once, and then runs freely
	const auto continued = RunTwoCounters([](lhvm::LHVM& vm, uint32_t first, uint32_t /*second*/) {
		vm.HoldTask(first);
		vm.ContinueTask(first);
	});
	EXPECT_EQ(continued, plain);
}

TEST(LhvmProgram, WrittenProgramsReadBack)
{
	const std::vector<VMInstruction> code {
	    Make(Opcode::Push, VMValue(uint32_t {1}), DataType::Int, VMMode::Reference),
	    Make(Opcode::Push, VMValue(2.5f), DataType::Float),
	    Make(Opcode::Jmp, VMValue(uint32_t {0}), DataType::Int, VMMode::Forward),
	    Make(Opcode::End),
	};
	const std::vector<VMScript> scripts {VMScript("Main", "main.txt", lhvm::ScriptType::Script, 2, {"local"}, 0, 1, 1)};
	const std::vector<char> data {'h', 'i', '\0'};
	const lhvm::LHVMFile file(lhvm::LHVMVersion::BlackAndWhite, {"first", "second"}, code, {1}, scripts, data);

	std::stringstream stream(std::ios::in | std::ios::out | std::ios::binary);
	file.Write(stream);
	const auto text = stream.str();
	lhvm::LHVMFile read;
	read.Open(std::vector<uint8_t>(text.begin(), text.end()));
	ASSERT_TRUE(read.IsLoaded());
	EXPECT_EQ(read.GetVersion(), lhvm::LHVMVersion::BlackAndWhite);
	EXPECT_EQ(read.GetVariablesNames(), file.GetVariablesNames());
	ASSERT_EQ(read.GetInstructions().size(), code.size());
	for (size_t i = 0; i < code.size(); ++i)
	{
		EXPECT_EQ(read.GetInstructions()[i].code, code[i].code);
		EXPECT_EQ(read.GetInstructions()[i].mode, code[i].mode);
		EXPECT_EQ(read.GetInstructions()[i].type, code[i].type);
		EXPECT_EQ(read.GetInstructions()[i].data.uintVal, code[i].data.uintVal);
		EXPECT_EQ(read.GetInstructions()[i].line, code[i].line);
	}
	EXPECT_EQ(read.GetAutostart(), file.GetAutostart());
	ASSERT_EQ(read.GetScripts().size(), 1u);
	const auto& script = read.GetScripts()[0];
	EXPECT_EQ(script.name, "Main");
	EXPECT_EQ(script.filename, "main.txt");
	EXPECT_EQ(script.type, lhvm::ScriptType::Script);
	EXPECT_EQ(script.variablesOffset, 2u);
	EXPECT_EQ(script.variables, std::vector<std::string> {"local"});
	EXPECT_EQ(script.instructionAddress, 0u);
	EXPECT_EQ(script.parameterCount, 1u);
	EXPECT_EQ(script.scriptId, 1u);
	EXPECT_EQ(read.GetData(), data);
}

TEST_F(DebuggerTest, UpdatingTheProgramKeepsTasksAndValues)
{
	_vm.LookIn(lhvm::ScriptType::All);
	ASSERT_EQ(Counter(), 1);
	auto code = _vm.GetInstructions();
	auto scripts = _vm.GetScripts();
	// A newer build adds a script that sets a new global, after the existing code
	code.push_back(Make(Opcode::Push, VMValue(int32_t {7})));
	code.push_back(Make(Opcode::Pop, VMValue(uint32_t {2}), DataType::Int, VMMode::Reference));
	code.push_back(Make(Opcode::End));
	scripts.emplace_back("Setter", "test.txt", lhvm::ScriptType::Script, 2, std::vector<std::string> {}, 6, 0, 2);
	const lhvm::LHVMFile newer(lhvm::LHVMVersion::BlackAndWhite, {"counter", "added"}, code, {}, scripts, {});
	ASSERT_EQ(_vm.UpdateProgram(newer), EXIT_SUCCESS);
	EXPECT_EQ(_vm.GetVariables().size(), 3u);
	EXPECT_EQ(Counter(), 1);
	ASSERT_TRUE(_vm.GetTasks().contains(_task));
	_vm.LookIn(lhvm::ScriptType::All);
	EXPECT_EQ(Counter(), 2);
	ASSERT_NE(_vm.StartScript("Setter", lhvm::ScriptType::All), 0u);
	_vm.LookIn(lhvm::ScriptType::All);
	EXPECT_EQ(_vm.GetVariables().at(2).value.intVal, 7);
	// A build that has lost code is refused
	const lhvm::LHVMFile older(lhvm::LHVMVersion::BlackAndWhite, {"counter"}, {}, {}, {}, {});
	EXPECT_EQ(_vm.UpdateProgram(older), EXIT_FAILURE);
	EXPECT_EQ(_vm.GetInstructions().size(), code.size());
}
