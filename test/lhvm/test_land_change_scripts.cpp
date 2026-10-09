/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

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

/// A challenge whose one script starts by itself and counts a global up by one each turn, forever
LHVMFile Challenge()
{
	const std::vector<VMInstruction> code {
	    Make(Opcode::Push, VMValue(uint32_t {1}), DataType::Int, VMMode::Reference), // 0: PUSHI counter
	    Make(Opcode::Push, VMValue(int32_t {1})),                                    // 1: PUSHI 1
	    Make(Opcode::Add, VMValue(uint32_t {0})),                                    // 2: ADDI
	    Make(Opcode::Pop, VMValue(uint32_t {1}), DataType::Int, VMMode::Reference),  // 3: POPI counter
	    Make(Opcode::Jmp, VMValue(uint32_t {0})),                                    // 4: JMP 0, which ends the turn
	    Make(Opcode::End),                                                           // 5
	};
	const std::vector<VMScript> scripts {VMScript("LandControl", "test.txt", ScriptType::Script, 1, {}, 0, 0, 1)};
	return {LHVMVersion::BlackAndWhite, {"counter"}, code, {1}, scripts, {}};
}

class LandChangeScripts: public ::testing::Test
{
protected:
	void SetUp() override
	{
		_vm.Initialise(&_natives, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
		ASSERT_EQ(_vm.LoadBinary(Challenge()), EXIT_SUCCESS);
	}

	[[nodiscard]] int32_t Counter() const { return _vm.GetVariables().at(1).value.intVal; }

	std::vector<NativeFunction> _natives {NativeFunction(nullptr, 0, 0, "NONE")};
	LHVM _vm;
};

} // namespace

TEST_F(LandChangeScripts, TheChallengesOwnScriptsStartWithIt)
{
	ASSERT_EQ(_vm.GetTasks().size(), 1u);
	_vm.LookIn(ScriptType::All);
	_vm.LookIn(ScriptType::All);
	EXPECT_EQ(Counter(), 2);
}

TEST_F(LandChangeScripts, LoadingTheChallengeAgainStartsItsScriptsAfresh)
{
	// A land loaded from the menu reloads the challenge: the last land's task stops and its global goes back to zero,
	// and the script that starts by itself starts once more, from its beginning
	_vm.LookIn(ScriptType::All);
	_vm.LookIn(ScriptType::All);
	_vm.LookIn(ScriptType::All);
	ASSERT_EQ(Counter(), 3);

	ASSERT_EQ(_vm.LoadBinary(Challenge()), EXIT_SUCCESS);
	EXPECT_EQ(Counter(), 0);
	ASSERT_EQ(_vm.GetTasks().size(), 1u);
	EXPECT_EQ(_vm.GetTasks().begin()->second.instructionAddress, 0u);

	_vm.LookIn(ScriptType::All);
	EXPECT_EQ(Counter(), 1);
}
