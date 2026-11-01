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

#include <vector>

#include <LHVM.h>
#include <gtest/gtest.h>

using namespace openblack::lhvm;

namespace
{

VMInstruction Make(Opcode code, VMValue data = VMValue(uint32_t {0}), DataType type = DataType::Int,
                   VMMode mode = VMMode::Immediate)
{
	return {code, mode, type, data, 0};
}

constexpr uint32_t k_FirstRan = 1;
constexpr uint32_t k_SecondRan = 2;
constexpr uint32_t k_TopDone = 3;
constexpr uint32_t k_RunSecondAddress = 4;

/// A top script that runs a first script, then a second script, then sets a global, as the story's top script runs the
/// control script of each land in turn
LHVMFile TopScript()
{
	const std::vector<VMInstruction> code {
	    Make(Opcode::Except, VMValue(uint32_t {9})),                                  // 0: EXCEPT handler
	    Make(Opcode::EndExcept, VMValue(uint32_t {0}), DataType::Int, VMMode::Yield), // 1: yield
	    Make(Opcode::Line),                                                           // 2
	    Make(Opcode::Run, VMValue(uint32_t {2}), DataType::Int, VMMode::Sync),        // 3: run First
	    Make(Opcode::Run, VMValue(uint32_t {3}), DataType::Int, VMMode::Sync),        // 4: run Second
	    Make(Opcode::Push, VMValue(int32_t {1})),                                     // 5
	    Make(Opcode::Pop, VMValue(k_TopDone), DataType::Int, VMMode::Reference),      // 6
	    Make(Opcode::EndExcept),                                                      // 7
	    Make(Opcode::Jmp, VMValue(uint32_t {10}), DataType::Int, VMMode::Forward),    // 8
	    Make(Opcode::FailExcept),                                                     // 9
	    Make(Opcode::End),                                                            // 10
	    Make(Opcode::Push, VMValue(int32_t {1})),                                     // 11: First
	    Make(Opcode::Pop, VMValue(k_FirstRan), DataType::Int, VMMode::Reference),     // 12
	    Make(Opcode::End),                                                            // 13
	    Make(Opcode::Push, VMValue(int32_t {1})),                                     // 14: Second
	    Make(Opcode::Pop, VMValue(k_SecondRan), DataType::Int, VMMode::Reference),    // 15
	    Make(Opcode::End),                                                            // 16
	};
	const std::vector<VMScript> scripts {
	    VMScript("Top", "top.txt", ScriptType::Script, 3, {}, 0, 0, 1),
	    VMScript("First", "first.txt", ScriptType::Script, 3, {}, 11, 0, 2),
	    VMScript("Second", "second.txt", ScriptType::Script, 3, {}, 14, 0, 3),
	};
	return {LHVMVersion::BlackAndWhite, {"first", "second", "done"}, code, {}, scripts, {}};
}

class LhvmStartAt: public ::testing::Test
{
protected:
	void SetUp() override
	{
		_vm.Initialise(&_natives, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
		ASSERT_EQ(_vm.LoadBinary(TopScript()), EXIT_SUCCESS);
	}

	[[nodiscard]] int32_t Global(uint32_t id) const { return _vm.GetVariables().at(id).value.intVal; }

	std::vector<NativeFunction> _natives {NativeFunction(nullptr, 0, 0, "NONE")};
	LHVM _vm;
};

} // namespace

TEST_F(LhvmStartAt, FindsTheStepThatRunsAScript)
{
	EXPECT_EQ(_vm.FindRunOfScript("Top", "First"), 3U);
	EXPECT_EQ(_vm.FindRunOfScript("Top", "Second"), k_RunSecondAddress);
	// A script that the caller does not run, and a script that is not in the program, have no step
	EXPECT_FALSE(_vm.FindRunOfScript("Top", "Top").has_value());
	EXPECT_FALSE(_vm.FindRunOfScript("Top", "Missing").has_value());
	EXPECT_FALSE(_vm.FindRunOfScript("Missing", "Second").has_value());
}

TEST_F(LhvmStartAt, AScriptStartedAtAStepSkipsTheStepsBeforeIt)
{
	const auto task = _vm.StartScriptAt("Top", ScriptType::All, k_RunSecondAddress);
	ASSERT_NE(task, 0U);
	// The task has the exception handler of its start
	ASSERT_EQ(_vm.GetTasks().at(task).exceptionHandlerIps.size(), 1U);
	EXPECT_EQ(_vm.GetTasks().at(task).exceptionHandlerIps.front(), 9U);
	for (int turn = 0; turn < 4; ++turn)
	{
		_vm.LookIn(ScriptType::All);
	}
	EXPECT_EQ(Global(k_FirstRan), 0);
	EXPECT_EQ(Global(k_SecondRan), 1);
	EXPECT_EQ(Global(k_TopDone), 1);
	EXPECT_TRUE(_vm.GetTasks().empty());
}

TEST_F(LhvmStartAt, AnAddressOutsideTheScriptStartsNothing)
{
	// The address of an instruction of another script, and an address after the end of the program
	EXPECT_EQ(_vm.StartScriptAt("Top", ScriptType::All, 12), 0U);
	EXPECT_EQ(_vm.StartScriptAt("Top", ScriptType::All, 100), 0U);
	EXPECT_EQ(_vm.StartScriptAt("Missing", ScriptType::All, 3), 0U);
	// A script of a type that the mask does not allow
	EXPECT_EQ(_vm.StartScriptAt("Top", ScriptType::Help, k_RunSecondAddress), 0U);
	EXPECT_TRUE(_vm.GetTasks().empty());
}
