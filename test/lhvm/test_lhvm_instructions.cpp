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
#include <cstdlib>

#include <limits>
#include <string>
#include <vector>

#include <LHVM.h>
#include <gtest/gtest.h>

using namespace openblack::lhvm;

namespace
{

/// The virtual machine with its instructions open to the tests. Each instruction works on the main stack.
class TestVm: public LHVM
{
public:
	using LHVM::Opcode04Add;
	using LHVM::Opcode08Mul;
	using LHVM::Opcode09Div;
	using LHVM::Opcode10Mod;
	using LHVM::Opcode14Eq;
	using LHVM::Opcode15Neq;
	using LHVM::Opcode16Geq;
	using LHVM::Opcode18Gt;
	using LHVM::Opcode19Lt;
	using LHVM::Opcode21Sleep;
	using LHVM::Opcode23Cast;
	using LHVM::Opcode29Swap;

	[[nodiscard]] uint32_t Count() const { return _mainStack.count; }
};

VMInstruction Make(Opcode code, VMValue data = VMValue(uint32_t {0}), DataType type = DataType::Int,
                   VMMode mode = VMMode::Immediate)
{
	return {code, mode, type, data, 0};
}

VMInstruction Typed(Opcode code, DataType type, VMMode mode = VMMode::Immediate)
{
	return {code, mode, type, VMValue(uint32_t {0}), 0};
}

class LhvmInstructions: public ::testing::Test
{
protected:
	void SetUp() override { _vm.Initialise(&_natives, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr); }

	std::vector<NativeFunction> _natives {NativeFunction(nullptr, 0, 0, "NONE")};
	TestVm _vm;
	VMTask _task;
};

} // namespace

TEST_F(LhvmInstructions, AStackHoldsThirtyOneValues)
{
	for (int32_t i = 0; i < 40; ++i)
	{
		_vm.Pushi(i);
	}
	ASSERT_EQ(_vm.Count(), 31u);
	EXPECT_EQ(_vm.Pop().intVal, 30);
}

TEST_F(LhvmInstructions, ADivisionByZeroKeepsTheDividendUnderAZero)
{
	_vm.Pushf(5.0f);
	_vm.Pushf(0.0f);
	_vm.Opcode09Div(_task, Typed(Opcode::Div, DataType::Float));
	ASSERT_EQ(_vm.Count(), 2u);
	EXPECT_EQ(_vm.Popf(), 0.0f);
	EXPECT_EQ(_vm.Popf(), 5.0f);
}

TEST_F(LhvmInstructions, ANotANumberDivisorCountsAsZero)
{
	_vm.Pushf(5.0f);
	_vm.Pushf(std::numeric_limits<float>::quiet_NaN());
	_vm.Opcode09Div(_task, Typed(Opcode::Div, DataType::Float));
	ASSERT_EQ(_vm.Count(), 2u);
	EXPECT_EQ(_vm.Popf(), 0.0f);
}

TEST_F(LhvmInstructions, IntegersDivideAndCompareAsUnsignedValues)
{
	_vm.Pushi(-8);
	_vm.Pushi(2);
	_vm.Opcode09Div(_task, Typed(Opcode::Div, DataType::Int));
	EXPECT_EQ(_vm.Pop().uintVal, 0x7FFFFFFCu);

	_vm.Pushi(-1);
	_vm.Pushi(1);
	_vm.Opcode18Gt(_task, Typed(Opcode::Gt, DataType::Int));
	EXPECT_EQ(_vm.Pop().intVal, 1);

	_vm.Pushi(-1);
	_vm.Pushi(1);
	_vm.Opcode19Lt(_task, Typed(Opcode::Lt, DataType::Int));
	EXPECT_EQ(_vm.Pop().intVal, 0);
}

TEST_F(LhvmInstructions, AFloatRemainderUsesTheTruncatedValues)
{
	// 7 divided by 2 gives 3, so the remainder is 7.5 - 6
	_vm.Pushf(7.5f);
	_vm.Pushf(2.5f);
	_vm.Opcode10Mod(_task, Typed(Opcode::Mod, DataType::Float));
	EXPECT_EQ(_vm.Popf(), 1.5f);

	// A divisor less than one truncates to zero
	_vm.Pushf(3.0f);
	_vm.Pushf(0.5f);
	_vm.Opcode10Mod(_task, Typed(Opcode::Mod, DataType::Float));
	EXPECT_EQ(_vm.Popf(), 0.0f);
	EXPECT_EQ(_vm.Count(), 0u);

	_vm.Pushf(-7.5f);
	_vm.Pushf(2.0f);
	_vm.Opcode10Mod(_task, Typed(Opcode::Mod, DataType::Float));
	EXPECT_EQ(_vm.Popf(), -1.5f);
}

TEST_F(LhvmInstructions, AVectorRemainderTakesNothingAndGivesZeros)
{
	_vm.Pushf(9.0f);
	_vm.Opcode10Mod(_task, Typed(Opcode::Mod, DataType::Vector));
	ASSERT_EQ(_vm.Count(), 4u);
	DataType type;
	EXPECT_EQ(_vm.Pop(type).floatVal, 0.0f);
	EXPECT_EQ(type, DataType::Vector);
}

TEST_F(LhvmInstructions, AnInstructionOfAnUnknownTypeTakesNothingAndGivesZero)
{
	_vm.Pushi(4);
	_vm.Pushi(5);
	_vm.Opcode04Add(_task, Typed(Opcode::Add, DataType::Object));
	ASSERT_EQ(_vm.Count(), 3u);
	DataType type;
	EXPECT_EQ(_vm.Pop(type).floatVal, 0.0f);
	EXPECT_EQ(type, DataType::Float);
}

TEST_F(LhvmInstructions, ComparisonsOfObjectsAndVectorsTakeNothing)
{
	_vm.Pusho(7);
	_vm.Pusho(7);
	_vm.Opcode14Eq(_task, Typed(Opcode::Eq, DataType::Object));
	ASSERT_EQ(_vm.Count(), 3u);
	EXPECT_EQ(_vm.Pop().intVal, 0);

	_vm.Opcode15Neq(_task, Typed(Opcode::Ne, DataType::Vector));
	ASSERT_EQ(_vm.Count(), 3u);
	EXPECT_EQ(_vm.Pop().intVal, 1);

	_vm.Opcode16Geq(_task, Typed(Opcode::Ge, DataType::Vector));
	ASSERT_EQ(_vm.Count(), 3u);
	EXPECT_EQ(_vm.Pop().intVal, 0);
}

TEST_F(LhvmInstructions, AVectorTimesANumberScalesEachPart)
{
	_vm.Pushf(2.0f);
	_vm.Pushv(1.0f);
	_vm.Pushv(3.0f);
	_vm.Pushv(5.0f);
	_vm.Opcode08Mul(_task, Typed(Opcode::Mul, DataType::Vector));
	EXPECT_EQ(_vm.Popf(), 10.0f);
	EXPECT_EQ(_vm.Popf(), 6.0f);
	EXPECT_EQ(_vm.Popf(), 2.0f);
}

TEST_F(LhvmInstructions, ASleepWithANegativeTimeDoesNotEnd)
{
	// The negative count of tenths becomes a very large unsigned count
	_task.ticks = 1000;
	_vm.Pushf(-1.0f);
	_vm.Opcode21Sleep(_task, Typed(Opcode::Sleep, DataType::Float));
	EXPECT_EQ(_vm.Pop().intVal, 0);
	EXPECT_TRUE(_task.sleeping);

	_task.ticks = 11;
	_vm.Pushf(1.0f);
	_vm.Opcode21Sleep(_task, Typed(Opcode::Sleep, DataType::Float));
	EXPECT_EQ(_vm.Pop().intVal, 1);
	EXPECT_FALSE(_task.sleeping);

	_task.ticks = 10;
	_vm.Pushf(1.0f);
	_vm.Opcode21Sleep(_task, Typed(Opcode::Sleep, DataType::Float));
	EXPECT_EQ(_vm.Pop().intVal, 0);
}

TEST_F(LhvmInstructions, ACastToAnIntegerTruncatesThroughSixtyFourBits)
{
	_vm.Pushf(-2.75f);
	_vm.Opcode23Cast(_task, Typed(Opcode::Cast, DataType::Int));
	EXPECT_EQ(_vm.Pop().intVal, -2);

	_vm.Pushf(5.0e9f);
	_vm.Opcode23Cast(_task, Typed(Opcode::Cast, DataType::Int));
	EXPECT_EQ(_vm.Pop().uintVal, static_cast<uint32_t>(static_cast<int64_t>(5.0e9f)));

	_vm.Pushf(1.0e30f);
	_vm.Opcode23Cast(_task, Typed(Opcode::Cast, DataType::Int));
	EXPECT_EQ(_vm.Pop().intVal, 0);
}

TEST_F(LhvmInstructions, ASwapThatCopiesFromBelowMovesTheValueToTheTop)
{
	_vm.Pushi(1);
	_vm.Pushi(2);
	_vm.Pushi(3);
	_vm.Opcode29Swap(_task, Make(Opcode::Swap, VMValue(uint32_t {3}), DataType::Float, VMMode::CopyFrom));
	ASSERT_EQ(_vm.Count(), 3u);
	EXPECT_EQ(_vm.Pop().intVal, 1);
	EXPECT_EQ(_vm.Pop().intVal, 3);
	EXPECT_EQ(_vm.Pop().intVal, 2);
}

TEST_F(LhvmInstructions, ASwapThatCopiesToBelowPutsACopyOfTheTopUnderTheValues)
{
	_vm.Pushi(1);
	_vm.Pushi(2);
	_vm.Pushi(3);
	_vm.Opcode29Swap(_task, Make(Opcode::Swap, VMValue(uint32_t {2}), DataType::Float, VMMode::CopyTo));
	ASSERT_EQ(_vm.Count(), 4u);
	EXPECT_EQ(_vm.Pop().intVal, 3);
	EXPECT_EQ(_vm.Pop().intVal, 2);
	EXPECT_EQ(_vm.Pop().intVal, 3);
	EXPECT_EQ(_vm.Pop().intVal, 1);
}

namespace
{

constexpr uint32_t k_Hits = 1;

/// One script with a frame handler around a loop. The loop's "until" condition is true at once.
LHVMFile LoopInsideAFrame()
{
	std::vector<VMInstruction> code(19, Make(Opcode::Line));
	code[0] = Make(Opcode::Except, VMValue(uint32_t {9}));                                 // frame handler at 9
	code[1] = Make(Opcode::Except, VMValue(uint32_t {14}));                                // loop handler at 14
	code[2] = Make(Opcode::Jmp, VMValue(uint32_t {2}));                                    // the loop waits
	code[9] = Make(Opcode::Push, VMValue(k_Hits), DataType::Int, VMMode::Reference);       // frame: hits + 1
	code[10] = Make(Opcode::Push, VMValue(int32_t {1}));                                   //
	code[11] = Make(Opcode::Add, VMValue(uint32_t {0}));                                   //
	code[12] = Make(Opcode::Pop, VMValue(k_Hits), DataType::Int, VMMode::Reference);       //
	code[13] = Make(Opcode::FailExcept);                                                   //
	code[14] = Make(Opcode::Push, VMValue(int32_t {1}), DataType::Boolean);                // loop: until true
	code[15] = Make(Opcode::Wait, VMValue(uint32_t {18}), DataType::Int, VMMode::Forward); //
	code[16] = Make(Opcode::BrkExcept);                                                    //
	code[17] = Make(Opcode::Jmp, VMValue(uint32_t {17}));                                  // after the loop: wait
	code[18] = Make(Opcode::FailExcept);                                                   //
	const std::vector<VMScript> scripts {VMScript("Frame", "test.txt", ScriptType::Script, 1, {}, 0, 0, 1)};
	return {LHVMVersion::BlackAndWhite, {"hits"}, code, {1}, scripts, {}};
}

} // namespace

TEST_F(LhvmInstructions, ABreakOutOfALoopKeepsTheHandlerOfTheBlockAroundIt)
{
	ASSERT_EQ(_vm.LoadBinary(LoopInsideAFrame()), EXIT_SUCCESS);
	for (int turn = 0; turn < 4; ++turn)
	{
		_vm.LookIn(ScriptType::All);
	}
	// The handlers start in the first turn and the loop ends in the second turn. The frame's handler runs in each turn
	// after it.
	EXPECT_EQ(_vm.GetVariables().at(k_Hits).value.intVal, 2);
	ASSERT_EQ(_vm.GetTasks().size(), 1u);
	EXPECT_EQ(_vm.GetTasks().begin()->second.exceptionHandlerIps.size(), 1u);
}

namespace
{

/// One script with a frame handler around two loops, one inside the other. The inner loop's handler only moves to
/// the next handler. The outer loop's "until" condition is true at once.
LHVMFile TwoLoopsInsideAFrame()
{
	std::vector<VMInstruction> code(21, Make(Opcode::Line));
	code[0] = Make(Opcode::Except, VMValue(uint32_t {9}));                                 // frame handler at 9
	code[1] = Make(Opcode::Except, VMValue(uint32_t {14}));                                // outer loop handler at 14
	code[2] = Make(Opcode::Except, VMValue(uint32_t {20}));                                // inner loop handler at 20
	code[3] = Make(Opcode::Jmp, VMValue(uint32_t {3}));                                    // the inner loop waits
	code[9] = Make(Opcode::Push, VMValue(k_Hits), DataType::Int, VMMode::Reference);       // frame: hits + 1
	code[10] = Make(Opcode::Push, VMValue(int32_t {1}));                                   //
	code[11] = Make(Opcode::Add, VMValue(uint32_t {0}));                                   //
	code[12] = Make(Opcode::Pop, VMValue(k_Hits), DataType::Int, VMMode::Reference);       //
	code[13] = Make(Opcode::FailExcept);                                                   //
	code[14] = Make(Opcode::Push, VMValue(int32_t {1}), DataType::Boolean);                // outer loop: until true
	code[15] = Make(Opcode::Wait, VMValue(uint32_t {19}), DataType::Int, VMMode::Forward); //
	code[16] = Make(Opcode::BrkExcept);                                                    //
	code[17] = Make(Opcode::Jmp, VMValue(uint32_t {17}));                                  // after the loops: wait
	code[19] = Make(Opcode::FailExcept);                                                   //
	code[20] = Make(Opcode::FailExcept);                                                   // inner loop: next handler
	const std::vector<VMScript> scripts {VMScript("Frame", "test.txt", ScriptType::Script, 1, {}, 0, 0, 1)};
	return {LHVMVersion::BlackAndWhite, {"hits"}, code, {1}, scripts, {}};
}

} // namespace

TEST_F(LhvmInstructions, ABreakFromAnOuterHandlerRemovesTheHandlersInsideItOnly)
{
	ASSERT_EQ(_vm.LoadBinary(TwoLoopsInsideAFrame()), EXIT_SUCCESS);
	for (int turn = 0; turn < 4; ++turn)
	{
		_vm.LookIn(ScriptType::All);
	}
	// In the second turn the inner handler moves to the outer handler, which ends both loops. The frame's handler
	// stays and runs in each turn after it.
	EXPECT_EQ(_vm.GetVariables().at(k_Hits).value.intVal, 2);
	ASSERT_EQ(_vm.GetTasks().size(), 1u);
	EXPECT_EQ(_vm.GetTasks().begin()->second.exceptionHandlerIps.size(), 1u);
}

namespace
{

/// Two scripts: the first ends at once, the second waits for ever
LHVMFile OneEndsOneWaits()
{
	const std::vector<VMInstruction> code {
	    Make(Opcode::End),                        // 0: Ends
	    Make(Opcode::Jmp, VMValue(uint32_t {1})), // 1: Waits
	};
	const std::vector<VMScript> scripts {
	    VMScript("Waits", "test.txt", ScriptType::Script, 0, {}, 1, 0, 1),
	    VMScript("Ends", "test.txt", ScriptType::Script, 0, {}, 0, 0, 2),
	};
	return {LHVMVersion::BlackAndWhite, {}, code, {1, 2}, scripts, {}};
}

} // namespace

TEST_F(LhvmInstructions, ATaskBeforeAStoppedTaskCountsTheTurnTwoTimes)
{
	ASSERT_EQ(_vm.LoadBinary(OneEndsOneWaits()), EXIT_SUCCESS);
	ASSERT_EQ(_vm.GetTasks().size(), 2u);
	_vm.LookIn(ScriptType::All);
	ASSERT_EQ(_vm.GetTasks().size(), 1u);
	// A task starts with a count of one
	EXPECT_EQ(_vm.GetTasks().begin()->second.ticks, 3u);
	_vm.LookIn(ScriptType::All);
	EXPECT_EQ(_vm.GetTasks().begin()->second.ticks, 4u);
}

TEST_F(LhvmInstructions, TheAutomaticScriptsStartByTheirPlaceInTheList)
{
	auto program = OneEndsOneWaits();
	const LHVMFile secondOnly(program.GetVersion(), program.GetVariablesNames(), program.GetInstructions(), {2},
	                          program.GetScripts(), program.GetData());
	ASSERT_EQ(_vm.LoadBinary(secondOnly), EXIT_SUCCESS);
	ASSERT_EQ(_vm.GetTasks().size(), 1u);
	EXPECT_EQ(_vm.GetTasks().begin()->second.name, "Ends");
}

namespace
{

constexpr uint32_t k_Runs = 1;

/// The first script stops itself through a native function. The second counts its runs.
LHVMFile StopsItself()
{
	const std::vector<VMInstruction> code {
	    Make(Opcode::Sys, VMValue(uint32_t {1})),                              // 0: stop me
	    Make(Opcode::Jmp, VMValue(uint32_t {0})),                              // 1
	    Make(Opcode::Push, VMValue(k_Runs), DataType::Int, VMMode::Reference), // 2: runs + 1
	    Make(Opcode::Push, VMValue(int32_t {1})),                              // 3
	    Make(Opcode::Add, VMValue(uint32_t {0})),                              // 4
	    Make(Opcode::Pop, VMValue(k_Runs), DataType::Int, VMMode::Reference),  // 5
	    Make(Opcode::Jmp, VMValue(uint32_t {2})),                              // 6
	};
	const std::vector<VMScript> scripts {
	    VMScript("StopsItself", "test.txt", ScriptType::Script, 1, {"held"}, 0, 0, 1),
	    VMScript("Counts", "test.txt", ScriptType::Script, 1, {}, 2, 0, 2),
	};
	return {LHVMVersion::BlackAndWhite, {"runs"}, code, {1, 2}, scripts, {}};
}

} // namespace

TEST_F(LhvmInstructions, ATaskThatStopsItselfEndsTheStepOfTheOtherTasks)
{
	std::vector<uint32_t> stopped;
	std::vector<NativeFunction> natives {
	    NativeFunction(nullptr, 0, 0, "NONE"),
	    NativeFunction([this]() { _vm.StopTask(_vm.GetCurrentTaskNumber()); }, 0, 0, "STOP_ME"),
	};
	_vm.Initialise(
	    &natives, nullptr, nullptr, [&stopped](uint32_t task) { stopped.push_back(task); }, nullptr, nullptr, nullptr);
	ASSERT_EQ(_vm.LoadBinary(StopsItself()), EXIT_SUCCESS);

	_vm.LookIn(ScriptType::All);
	ASSERT_EQ(_vm.GetTasks().size(), 1u);
	EXPECT_EQ(stopped, std::vector<uint32_t> {1});
	// The game finds no task after the one that stopped itself, so the other task waits for the next turn
	EXPECT_EQ(_vm.GetVariables().at(k_Runs).value.intVal, 0);

	_vm.LookIn(ScriptType::All);
	EXPECT_EQ(_vm.GetVariables().at(k_Runs).value.intVal, 1);
}

TEST_F(LhvmInstructions, ATaskCanStopEveryTaskDuringItsRun)
{
	std::vector<NativeFunction> natives {
	    NativeFunction(nullptr, 0, 0, "NONE"),
	    NativeFunction([this]() { _vm.StopAllTasks(); }, 0, 0, "STOP_ALL"),
	};
	_vm.Initialise(&natives, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
	ASSERT_EQ(_vm.LoadBinary(StopsItself()), EXIT_SUCCESS);

	_vm.LookIn(ScriptType::All);
	EXPECT_TRUE(_vm.GetTasks().empty());
}
