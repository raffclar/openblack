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

#include <functional>
#include <string>
#include <utility>
#include <vector>

#include <LHVM.h>
#include <LHVMFile.h>
#include <gtest/gtest.h>

using namespace openblack::lhvm;

namespace
{

VMInstruction Make(Opcode code, VMValue data = VMValue(uint32_t {0}), DataType type = DataType::Int,
                   VMMode mode = VMMode::Immediate)
{
	return {code, mode, type, data, 0};
}

constexpr uint32_t k_Done = 1;
constexpr uint32_t k_Native = 1;
/// The place of the call in the script that gives the arguments
constexpr uint32_t k_CallAddress = 2;

/// A script that gives two arguments to a native, then sets a global and ends. With no pushes, the native finds the
/// stack empty.
LHVMFile CallWithTwoArguments(bool pushes)
{
	std::vector<VMInstruction> code;
	if (pushes)
	{
		code.push_back(Make(Opcode::Push, VMValue(int32_t {7}))); // PUSHI 7
		code.push_back(Make(Opcode::Push, VMValue(int32_t {9}))); // PUSHI 9
	}
	code.push_back(Make(Opcode::Sys, VMValue(int32_t {k_Native})));                       // SYS the native
	code.push_back(Make(Opcode::Push, VMValue(int32_t {1})));                             // PUSHI 1
	code.push_back(Make(Opcode::Pop, VMValue(k_Done), DataType::Int, VMMode::Reference)); // POPI done
	code.push_back(Make(Opcode::End));                                                    // END
	const std::vector<VMScript> scripts {VMScript("CallNative", "test.txt", ScriptType::Script, 1, {}, 0, 0, 1)};
	return {LHVMVersion::BlackAndWhite, {"done"}, code, {1}, scripts, {}};
}

/// A machine whose native takes two arguments and gives them to the test
struct Machine
{
	explicit Machine(std::function<void()> afterPops = {})
	    : afterPops(std::move(afterPops))
	{
		natives.emplace_back(nullptr, 0, 0, "NONE");
		natives.emplace_back(
		    [this]() {
			    const auto second = vm.Pop().intVal;
			    const auto first = vm.Pop().intVal;
			    popped.push_back(first);
			    popped.push_back(second);
			    if (this->afterPops)
			    {
				    this->afterPops();
			    }
		    },
		    2, 0, "TAKE_TWO");
		vm.Initialise(
		    &natives, nullptr, nullptr, nullptr,
		    [this](ErrorCode code, const std::string&, uint32_t) { errors.push_back(code); }, nullptr, nullptr);
	}

	[[nodiscard]] int32_t Done() const { return vm.GetVariables().at(k_Done).value.intVal; }

	std::function<void()> afterPops;
	std::vector<NativeFunction> natives;
	LHVM vm;
	std::vector<int32_t> popped;
	std::vector<ErrorCode> errors;
};

/// The bytes of a save that the native makes after it took its arguments, as the game saves when a script finishes a
/// temple
std::vector<uint8_t> SaveInsideTheNative()
{
	std::vector<uint8_t> saved;
	Machine first;
	first.afterPops = [&first, &saved]() { saved = first.vm.SaveState(); };
	EXPECT_EQ(first.vm.LoadBinary(CallWithTwoArguments(true)), EXIT_SUCCESS);
	first.vm.LookIn(ScriptType::All);
	EXPECT_EQ(first.popped, (std::vector<int32_t> {7, 9}));
	EXPECT_EQ(first.Done(), 1);
	return saved;
}

} // namespace

TEST(LhvmSaveInNative, ATaskSavedInsideANativeIsListedAfterTheLoad)
{
	const auto saved = SaveInsideTheNative();
	ASSERT_FALSE(saved.empty());
	Machine loaded;
	ASSERT_EQ(loaded.vm.RestoreState(saved), EXIT_SUCCESS);
	const auto cut = loaded.vm.NativesCutBySave();
	ASSERT_EQ(cut.size(), 1u);
	EXPECT_EQ(cut[0].native, k_Native);
	ASSERT_EQ(loaded.vm.GetTasks().size(), 1u);
	EXPECT_EQ(cut[0].task, loaded.vm.GetTasks().begin()->first);
	EXPECT_EQ(loaded.vm.GetTasks().begin()->second.instructionAddress, k_CallAddress);
}

TEST(LhvmSaveInNative, TheLoadedTaskCallsTheNativeAgainWithZerosAndNoError)
{
	const auto saved = SaveInsideTheNative();
	Machine loaded;
	ASSERT_EQ(loaded.vm.RestoreState(saved), EXIT_SUCCESS);
	loaded.vm.LookIn(ScriptType::All);
	// The game calls the native again, and its pops of the missing arguments give zeros
	EXPECT_EQ(loaded.popped, (std::vector<int32_t> {0, 0}));
	EXPECT_TRUE(loaded.errors.empty());
	EXPECT_EQ(loaded.Done(), 1);
	EXPECT_TRUE(loaded.vm.NativesCutBySave().empty());
}

TEST(LhvmSaveInNative, AnEmptyStackInAnotherCallIsStillAnError)
{
	Machine machine;
	ASSERT_EQ(machine.vm.LoadBinary(CallWithTwoArguments(false)), EXIT_SUCCESS);
	machine.vm.LookIn(ScriptType::All);
	EXPECT_EQ(machine.popped, (std::vector<int32_t> {0, 0}));
	EXPECT_EQ(machine.errors, (std::vector<ErrorCode> {ErrorCode::ErrStackEmpty, ErrorCode::ErrStackEmpty}));
}

TEST(LhvmSaveInNative, OnlyTheFirstCallAfterTheLoadIsExpectedToMissItsArguments)
{
	const auto saved = SaveInsideTheNative();
	Machine loaded;
	ASSERT_EQ(loaded.vm.RestoreState(saved), EXIT_SUCCESS);
	loaded.vm.LookIn(ScriptType::All);
	ASSERT_TRUE(loaded.errors.empty());
	// A new program has no task that a save stopped
	ASSERT_EQ(loaded.vm.LoadBinary(CallWithTwoArguments(false)), EXIT_SUCCESS);
	EXPECT_TRUE(loaded.vm.NativesCutBySave().empty());
	loaded.vm.LookIn(ScriptType::All);
	EXPECT_EQ(loaded.errors.size(), 2u);
}
