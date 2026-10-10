/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>
#include <cstdlib>

#include <limits>
#include <string>
#include <vector>

#include <LHVM.h>
#include <gtest/gtest.h>

#include "ScriptHeaders/ScriptChallengeSnapshots.h"

using namespace openblack;
using namespace openblack::script::challenge_snapshots;
using openblack::lhvm::DataType;
using openblack::lhvm::VMValue;

namespace
{

/// A script's stack as it is before the call, the first value pushed first
class FakeStack
{
public:
	void Push(float value) { _values.push_back({.value = VMValue(value), .type = DataType::Float}); }
	void Push(int32_t value) { _values.push_back({.value = VMValue(value), .type = DataType::Int}); }
	void Push(glm::vec3 vector)
	{
		Push(vector.x);
		Push(vector.y);
		Push(vector.z);
	}

	[[nodiscard]] Stack Reader()
	{
		return {
		    .pop =
		        [this]() {
			        if (_values.empty())
			        {
				        ++_underflows;
				        return Value {};
			        }
			        const auto value = _values.back();
			        _values.pop_back();
			        return value;
		        },
		    .text = [](uint32_t offset) { return "script at " + std::to_string(offset); },
		};
	}

	[[nodiscard]] size_t Size() const { return _values.size(); }
	[[nodiscard]] int Underflows() const { return _underflows; }

private:
	std::vector<Value> _values;
	int _underflows {0};
};

/// What the language pushes for a challenge's start: whether it is a quest, the camera's place and focus, how far it
/// has got, its alignment, its title, the reminder script, its arguments, how many there are and the challenge
void PushStart(FakeStack& stack, const std::vector<float>& arguments)
{
	stack.Push(int32_t {1});
	stack.Push(glm::vec3(1.0f, 2.0f, 3.0f));
	stack.Push(glm::vec3(4.0f, 5.0f, 6.0f));
	stack.Push(0.25f);
	stack.Push(-0.5f);
	stack.Push(int32_t {2455});
	stack.Push(int32_t {7});
	for (const auto argument : arguments)
	{
		stack.Push(argument);
	}
	stack.Push(static_cast<int32_t>(arguments.size()));
	stack.Push(int32_t {64});
}

} // namespace

TEST(ScriptChallengeSnapshots, AStartTakesEveryValueItWasGiven)
{
	FakeStack stack;
	stack.Push(99.0f); // a value of the script's own, under the call's
	PushStart(stack, {3856.0f, 12.0f});
	const auto snapshot = Read(Call::Start, stack.Reader());
	EXPECT_EQ(stack.Size(), 1u);
	EXPECT_EQ(stack.Underflows(), 0);
	EXPECT_EQ(snapshot.challenge, 64u);
	ASSERT_EQ(snapshot.arguments.size(), 2u);
	// The last given first
	EXPECT_FLOAT_EQ(snapshot.arguments[0].value.floatVal, 12.0f);
	EXPECT_FLOAT_EQ(snapshot.arguments[1].value.floatVal, 3856.0f);
	EXPECT_EQ(snapshot.arguments[1].type, DataType::Float);
	EXPECT_FALSE(snapshot.tooManyArguments);
	EXPECT_EQ(snapshot.reminderScript, "script at 7");
	EXPECT_EQ(snapshot.title, 2455u);
	EXPECT_FLOAT_EQ(snapshot.alignment, -0.5f);
	EXPECT_FLOAT_EQ(snapshot.success, 0.25f);
	ASSERT_TRUE(snapshot.focus.has_value());
	ASSERT_TRUE(snapshot.position.has_value());
	EXPECT_EQ(*snapshot.focus, glm::vec3(4.0f, 5.0f, 6.0f));
	EXPECT_EQ(*snapshot.position, glm::vec3(1.0f, 2.0f, 3.0f));
	EXPECT_TRUE(snapshot.quest);
}

TEST(ScriptChallengeSnapshots, AnUpdateTakesNoCameraOrQuest)
{
	FakeStack stack;
	stack.Push(0.5f);
	stack.Push(0.0f);
	stack.Push(int32_t {2455});
	stack.Push(int32_t {7});
	stack.Push(4257.0f);
	stack.Push(int32_t {1});
	stack.Push(int32_t {64});
	const auto snapshot = Read(Call::Update, stack.Reader());
	EXPECT_EQ(stack.Size(), 0u);
	EXPECT_EQ(stack.Underflows(), 0);
	EXPECT_EQ(snapshot.arguments.size(), 1u);
	EXPECT_FLOAT_EQ(snapshot.success, 0.5f);
	EXPECT_FALSE(snapshot.focus.has_value());
	EXPECT_FALSE(snapshot.quest);
}

TEST(ScriptChallengeSnapshots, APictureTakesTheCameraButNoReminder)
{
	FakeStack stack;
	stack.Push(glm::vec3(1.0f, 2.0f, 3.0f));
	stack.Push(glm::vec3(4.0f, 5.0f, 6.0f));
	stack.Push(1.0f);
	stack.Push(1.0f);
	stack.Push(int32_t {2455});
	stack.Push(int32_t {1});
	stack.Push(int32_t {64});
	const auto snapshot = Read(Call::Picture, stack.Reader());
	EXPECT_EQ(stack.Size(), 0u);
	EXPECT_EQ(stack.Underflows(), 0);
	EXPECT_TRUE(snapshot.takingPicture);
	EXPECT_TRUE(snapshot.arguments.empty());
	EXPECT_EQ(*snapshot.position, glm::vec3(1.0f, 2.0f, 3.0f));
}

TEST(ScriptChallengeSnapshots, TooManyArgumentsAreReportedButAllTaken)
{
	FakeStack stack;
	PushStart(stack, std::vector<float>(13, 1.0f));
	const auto start = Read(Call::Start, stack.Reader());
	EXPECT_EQ(stack.Size(), 0u);
	EXPECT_TRUE(start.tooManyArguments);
	EXPECT_EQ(start.arguments.size(), 13u);
	EXPECT_EQ(MaxArguments(Call::Start), 12u);
	EXPECT_EQ(MaxArguments(Call::Update), 11u);
}

TEST(ScriptChallengeSnapshots, AlignmentAndSuccessAreKeptInRange)
{
	EXPECT_FLOAT_EQ(ClampAlignment(-3.0f), -1.0f);
	EXPECT_FLOAT_EQ(ClampAlignment(3.0f), 1.0f);
	EXPECT_FLOAT_EQ(ClampAlignment(0.2f), 0.2f);
	EXPECT_FLOAT_EQ(ClampAlignment(std::numeric_limits<float>::quiet_NaN()), -1.0f);
	EXPECT_FLOAT_EQ(ClampSuccess(-0.1f), 0.0f);
	EXPECT_FLOAT_EQ(ClampSuccess(1.5f), 1.0f);
	EXPECT_FLOAT_EQ(ClampSuccess(std::numeric_limits<float>::quiet_NaN()), 0.0f);
}

namespace
{
lhvm::VMInstruction Instruction(lhvm::Opcode code, VMValue data, DataType type = DataType::Int,
                                lhvm::VMMode mode = lhvm::VMMode::Immediate)
{
	return {code, mode, type, data, 0};
}
} // namespace

TEST(ScriptChallengeSnapshots, ANativeTakingAnyNumberOfValuesLeavesTheTasksStackEmpty)
{
	using lhvm::Opcode;
	lhvm::LHVM vm;
	// A native taking any number of values, as a challenge's update does
	std::vector<lhvm::NativeFunction> natives {
	    lhvm::NativeFunction(nullptr, 0, 0, "NONE"),
	    lhvm::NativeFunction(
	        [&vm]() {
		        [[maybe_unused]] const auto snapshot =
		            Read(Call::Update, {
		                                   .pop =
		                                       [&vm]() {
			                                       Value value;
			                                       value.value = vm.Pop(value.type);
			                                       return value;
		                                       },
		                                   .text = [&vm](uint32_t offset) { return vm.GetString(offset); },
		                               });
	        },
	        -1, 0, "UPDATE_SNAPSHOT"),
	};
	vm.Initialise(&natives, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
	constexpr uint32_t k_Counter = 1;
	const std::vector<lhvm::VMInstruction> code {
	    Instruction(Opcode::Push, VMValue(0.5f), DataType::Float),                            // 0: success
	    Instruction(Opcode::Push, VMValue(0.0f), DataType::Float),                            // 1: alignment
	    Instruction(Opcode::Push, VMValue(int32_t {2455})),                                   // 2: title
	    Instruction(Opcode::Push, VMValue(int32_t {0})),                                      // 3: reminder script
	    Instruction(Opcode::Push, VMValue(4257.0f), DataType::Float),                         // 4: argument
	    Instruction(Opcode::Push, VMValue(12.0f), DataType::Float),                           // 5: argument
	    Instruction(Opcode::Push, VMValue(int32_t {2})),                                      // 6: how many
	    Instruction(Opcode::Push, VMValue(int32_t {64})),                                     // 7: challenge
	    Instruction(Opcode::Sys, VMValue(uint32_t {1})),                                      // 8: SYS
	    Instruction(Opcode::Push, VMValue(int32_t {1})),                                      // 9: PUSHI 1
	    Instruction(Opcode::Pop, VMValue(k_Counter), DataType::Int, lhvm::VMMode::Reference), // 10: POPI counter
	    Instruction(Opcode::Jmp, VMValue(uint32_t {9})),                                      // 11: JMP 9, ending the turn
	};
	const std::vector<lhvm::VMScript> scripts {
	    lhvm::VMScript("UpdateChallenge", "test.txt", lhvm::ScriptType::Script, 1, {}, 0, 0, 1)};
	const std::vector<char> data {'R', 'e', 'm', 'i', 'n', 'd', '\0'};
	ASSERT_EQ(vm.LoadBinary(lhvm::LHVMFile(lhvm::LHVMVersion::BlackAndWhite, {"counter"}, code, {1}, scripts, data)),
	          EXIT_SUCCESS);
	for (int turn = 0; turn < 3; ++turn)
	{
		vm.LookIn(lhvm::ScriptType::All);
	}
	ASSERT_EQ(vm.GetTasks().size(), 1u);
	EXPECT_EQ(vm.GetTasks().begin()->second.stack.count, 0u);
	EXPECT_EQ(vm.GetVariables().at(k_Counter).value.intVal, 1);
}
