/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <initializer_list>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include <LHVMFile.h>
#include <gtest/gtest.h>

using namespace openblack::lhvm;

namespace
{

VMStack StackOf(std::initializer_list<std::pair<uint32_t, DataType>> values)
{
	VMStack stack;
	for (const auto& [value, type] : values)
	{
		stack.values.at(stack.count) = VMValue(value);
		stack.types.at(stack.count) = type;
		++stack.count;
	}
	stack.pushCount = 7;
	stack.popCount = 5;
	return stack;
}

// A small program with two scripts, saved while running: a value on the main stack, two globals and one task per script
LHVMFile RunningProgram()
{
	const std::vector<std::string> globals {"Counter", "Flag"};
	const std::vector<VMInstruction> code {
	    VMInstruction(Opcode::Push, VMMode::Immediate, DataType::Float, VMValue(1.0f), 3),
	    VMInstruction(Opcode::End, VMMode::Immediate, DataType::None, VMValue(), 4),
	};
	const std::vector<VMScript> scripts {
	    VMScript("First", "first.txt", ScriptType::Script, 0, {"local"}, 0, 0, 1),
	    VMScript("Second", "second.txt", ScriptType::Help, 2, {}, 1, 1, 2),
	};
	const std::vector<VMVar> globalValues {
	    VMVar(DataType::Float, VMValue(2.5f), "Counter"),
	    VMVar(DataType::Boolean, VMValue(1), "Flag"),
	};

	VMTask first({VMVar(DataType::Object, VMValue(42u), "local")}, 1, 3, 1, 0,
	             StackOf({{9u, DataType::Int}, {4u, DataType::Object}}), "First", "first.txt", ScriptType::Script);
	first.pevInstructionAddress = 0;
	first.waitingTaskId = 4;
	first.currentExceptionHandlerIndex = 1;
	first.exceptionHandlerIps = {0, 1};
	first.ticks = 12;
	first.inExceptionHandler = true;
	first.sleeping = true;

	// A task of the last script must load too
	VMTask second({}, 2, 4, 1, 2, VMStack(), "Second", "second.txt", ScriptType::Help);
	second.stop = true;
	second.iield = true;

	return {LHVMVersion::BlackAndWhite,
	        globals,
	        code,
	        {1},
	        scripts,
	        {'a', 'b', '\0'},
	        StackOf({{3u, DataType::Float}}),
	        globalValues,
	        {first, second},
	        1234,
	        56,
	        4,
	        2,
	        98765};
}

std::string Bytes(const LHVMFile& file)
{
	std::ostringstream stream(std::ios::binary);
	file.Write(stream);
	return stream.str();
}

LHVMFile Reopened(const std::string& bytes)
{
	LHVMFile file;
	file.Open(std::vector<uint8_t>(bytes.begin(), bytes.end()));
	return file;
}

void ExpectSameStack(const VMStack& a, const VMStack& b)
{
	ASSERT_EQ(a.count, b.count);
	EXPECT_EQ(a.pushCount, b.pushCount);
	EXPECT_EQ(a.popCount, b.popCount);
	for (uint32_t i = 0; i < a.count; ++i)
	{
		EXPECT_EQ(a.values.at(i).uintVal, b.values.at(i).uintVal);
		EXPECT_EQ(a.types.at(i), b.types.at(i));
	}
}

void ExpectSameVariables(const std::vector<VMVar>& a, const std::vector<VMVar>& b)
{
	ASSERT_EQ(a.size(), b.size());
	for (size_t i = 0; i < a.size(); ++i)
	{
		EXPECT_EQ(a[i].type, b[i].type);
		EXPECT_EQ(a[i].value.uintVal, b[i].value.uintVal);
		EXPECT_EQ(a[i].name, b[i].name);
	}
}

} // namespace

TEST(LHVMSaveState, RunningStateIsReadBackAsWritten)
{
	const auto original = RunningProgram();
	const auto reopened = Reopened(Bytes(original));

	ASSERT_TRUE(reopened.IsLoaded());
	ASSERT_TRUE(reopened.HasStatus());
	ExpectSameStack(original.GetStack(), reopened.GetStack());
	ExpectSameVariables(original.GetVariablesValues(), reopened.GetVariablesValues());
	EXPECT_EQ(reopened.GetTicks(), 1234u);
	EXPECT_EQ(reopened.GetCurrentLineNumber(), 56u);
	EXPECT_EQ(reopened.GetHighestTaskId(), 4u);
	EXPECT_EQ(reopened.GetHighestScriptId(), 2u);
	EXPECT_EQ(reopened.GetExecutedInstructions(), 98765u);

	ASSERT_EQ(reopened.GetTasks().size(), 2u);
	for (size_t i = 0; i < 2; ++i)
	{
		const auto& a = original.GetTasks()[i];
		const auto& b = reopened.GetTasks()[i];
		ExpectSameVariables(a.localVars, b.localVars);
		EXPECT_EQ(a.id, b.id);
		EXPECT_EQ(a.scriptId, b.scriptId);
		EXPECT_EQ(a.instructionAddress, b.instructionAddress);
		EXPECT_EQ(a.pevInstructionAddress, b.pevInstructionAddress);
		EXPECT_EQ(a.waitingTaskId, b.waitingTaskId);
		EXPECT_EQ(a.variablesOffset, b.variablesOffset);
		EXPECT_EQ(a.currentExceptionHandlerIndex, b.currentExceptionHandlerIndex);
		EXPECT_EQ(a.exceptionHandlerIps, b.exceptionHandlerIps);
		EXPECT_EQ(a.ticks, b.ticks);
		EXPECT_EQ(a.type, b.type);
		EXPECT_EQ(a.inExceptionHandler, b.inExceptionHandler);
		EXPECT_EQ(a.stop, b.stop);
		EXPECT_EQ(a.iield, b.iield);
		EXPECT_EQ(a.sleeping, b.sleeping);
		ExpectSameStack(a.stack, b.stack);
		EXPECT_EQ(b.name, a.name);
	}
}

TEST(LHVMSaveState, RewritingASaveGivesTheSameBytes)
{
	const auto bytes = Bytes(RunningProgram());
	EXPECT_EQ(Bytes(Reopened(bytes)), bytes);
}

TEST(LHVMSaveState, AProgramWithoutRunningStateWritesNone)
{
	const auto running = RunningProgram();
	const LHVMFile program(running.GetVersion(), running.GetVariablesNames(), running.GetInstructions(), running.GetAutostart(),
	                       running.GetScripts(), running.GetData());
	const auto reopened = Reopened(Bytes(program));
	ASSERT_TRUE(reopened.IsLoaded());
	EXPECT_FALSE(reopened.HasStatus());
}
