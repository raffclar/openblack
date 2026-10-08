/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// What a native function that takes nothing off the stack leaves there. The land script pushes the object before
// GET_ACTION_TEXT_FOR_OBJECT, whose game function only pushes its text number: the object stays under it, as in the
// game. Bound with no argument, openblack's machine keeps it; bound with one, the machine would drop it.

#include <cstdlib>

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
/// The text number GET_ACTION_TEXT_FOR_OBJECT gives
constexpr int32_t k_ActionText = 828;

/// Runs PUSH object, the action-text native bound with `boundArguments`, then a native that reads the whole stack
std::vector<int32_t> StackAfterActionText(int32_t boundArguments)
{
	const std::vector<VMInstruction> code {
	    {Opcode::Push, VMMode::Immediate, DataType::Int, VMValue(int32_t {5}), 0},
	    {Opcode::Sys, VMMode::Immediate, DataType::Int, VMValue(uint32_t {1}), 0},
	    {Opcode::Sys, VMMode::Immediate, DataType::Int, VMValue(uint32_t {2}), 0},
	    {Opcode::End, VMMode::Immediate, DataType::Int, VMValue(uint32_t {0}), 0},
	};
	const std::vector<VMScript> scripts {VMScript("Text", "test.txt", lhvm::ScriptType::Script, 1, {}, 0, 0, 1)};

	std::vector<int32_t> stack;
	lhvm::LHVM vm;
	const std::vector<lhvm::NativeFunction> natives {
	    lhvm::NativeFunction(nullptr, 0, 0, "NONE"),
	    lhvm::NativeFunction([&]() { vm.Pushi(k_ActionText); }, boundArguments, 1, "GET_ACTION_TEXT_FOR_OBJECT"),
	    lhvm::NativeFunction(
	        [&]() {
		        const auto& task = vm.GetTasks().begin()->second;
		        for (uint32_t i = 0; i < task.stack.count; ++i)
		        {
			        stack.push_back(task.stack.values.at(i).intVal);
		        }
	        },
	        0, 0, "SEE"),
	};
	vm.Initialise(&natives, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
	EXPECT_EQ(vm.LoadBinary(lhvm::LHVMFile(lhvm::LHVMVersion::BlackAndWhite, {}, code, {}, scripts, {})), EXIT_SUCCESS);
	EXPECT_NE(vm.StartScript("Text", lhvm::ScriptType::All), 0u);
	vm.LookIn(lhvm::ScriptType::All);
	return stack;
}
} // namespace

TEST(LhvmNativeStack, ANativeBoundWithoutArgumentsLeavesWhatTheScriptPushed)
{
	const std::vector<int32_t> expected {5, k_ActionText};
	EXPECT_EQ(StackAfterActionText(0), expected);
}

TEST(LhvmNativeStack, ANativeBoundWithAnArgumentItDoesNotPopHasItDropped)
{
	const std::vector<int32_t> expected {k_ActionText};
	EXPECT_EQ(StackAfterActionText(1), expected);
}
