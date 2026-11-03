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

#include <map>
#include <sstream>
#include <string>
#include <vector>

#include <LHVM.h>
#include <gtest/gtest.h>

using namespace openblack::lhvm;

namespace
{

constexpr uint32_t k_Woke = 1;
constexpr uint32_t k_Runs = 2;
constexpr uint32_t k_Held = 3;
constexpr uint32_t k_HeldObject = 42;
constexpr int32_t k_TurnsBeforeTheSave = 5;
constexpr int32_t k_TurnsAfterTheSave = 40;

VMInstruction Make(Opcode code, VMValue data = VMValue(uint32_t {0}), DataType type = DataType::Int,
                   VMMode mode = VMMode::Immediate)
{
	return {code, mode, type, data, 0};
}

/// Two scripts. The first holds an object in a local, waits two seconds and then sets a flag. The second counts the turns.
LHVMFile WaitsAndCounts()
{
	const std::vector<VMInstruction> code {
	    Make(Opcode::Push, VMValue(k_HeldObject), DataType::Object),                // 0: held = the object
	    Make(Opcode::Pop, VMValue(k_Held), DataType::Object, VMMode::Reference),    // 1
	    Make(Opcode::Push, VMValue(2.0f), DataType::Float),                         // 2: wait 2 seconds
	    Make(Opcode::Sleep, VMValue(uint32_t {0}), DataType::Float),                // 3
	    Make(Opcode::Wait, VMValue(uint32_t {2}), DataType::Int, VMMode::Backward), // 4: back to 2 until the time is past
	    Make(Opcode::Push, VMValue(int32_t {1})),                                   // 5: woke = 1
	    Make(Opcode::Pop, VMValue(k_Woke), DataType::Int, VMMode::Reference),       // 6
	    Make(Opcode::End),                                                          // 7
	    Make(Opcode::Push, VMValue(k_Runs), DataType::Int, VMMode::Reference),      // 8: runs + 1
	    Make(Opcode::Push, VMValue(int32_t {1})),                                   // 9
	    Make(Opcode::Add, VMValue(uint32_t {0})),                                   // 10
	    Make(Opcode::Pop, VMValue(k_Runs), DataType::Int, VMMode::Reference),       // 11
	    Make(Opcode::Jmp, VMValue(uint32_t {8}), DataType::Int, VMMode::Backward),  // 12: back to 8
	};
	const std::vector<VMScript> scripts {
	    VMScript("Waits", "test.txt", ScriptType::Script, 2, {"held"}, 0, 0, 1),
	    VMScript("Counts", "test.txt", ScriptType::Script, 2, {}, 8, 0, 2),
	};
	return {LHVMVersion::BlackAndWhite, {"woke", "runs"}, code, {1, 2}, scripts, {}};
}

std::vector<uint8_t> Bytes(const LHVMFile& file)
{
	std::ostringstream stream(std::ios::binary);
	file.Write(stream);
	const auto text = stream.str();
	return {text.begin(), text.end()};
}

/// A machine that counts the references that its scripts give to each object, as the table of the scripts' objects does
struct CountedVm
{
	CountedVm()
	{
		vm.Initialise(
		    &natives, nullptr, nullptr, nullptr, nullptr, [this](uint32_t object) { ++references[object]; },
		    [this](uint32_t object) { --references[object]; });
	}

	[[nodiscard]] int32_t Global(uint32_t index) const { return vm.GetVariables().at(index).value.intVal; }

	std::vector<NativeFunction> natives {NativeFunction(nullptr, 0, 0, "NONE")};
	std::map<uint32_t, int32_t> references;
	LHVM vm;
};

} // namespace

// A saved game loads into a new machine. The scripts must continue from where they were, with their waits and objects.
TEST(LhvmSaveResume, ARestoredMachineContinuesAsTheSavedMachine)
{
	CountedVm saved;
	ASSERT_EQ(saved.vm.LoadBinary(WaitsAndCounts()), EXIT_SUCCESS);
	for (int32_t turn = 0; turn < k_TurnsBeforeTheSave; ++turn)
	{
		saved.vm.LookIn(ScriptType::All);
	}
	ASSERT_EQ(saved.Global(k_Woke), 0) << "the wait must not be past at the save";
	ASSERT_EQ(saved.references[k_HeldObject], 1);
	const auto save = Bytes(LHVM::StateFile(*saved.vm.ProgramFile(), saved.vm.CopyRunningState()));

	CountedVm loaded;
	ASSERT_EQ(loaded.vm.RestoreState(save), EXIT_SUCCESS);
	ASSERT_EQ(Bytes(loaded.vm.StateFile()), save);
	ASSERT_EQ(loaded.vm.GetTasks().size(), 2u);
	EXPECT_EQ(loaded.Global(k_Runs), saved.Global(k_Runs));
	// The table of the scripts' objects keeps the references, so the restore gives none again. The table loads next.
	EXPECT_EQ(loaded.references[k_HeldObject], 0);
	loaded.references = saved.references;

	int32_t wokeTurn = -1;
	for (int32_t turn = 0; turn < k_TurnsAfterTheSave; ++turn)
	{
		saved.vm.LookIn(ScriptType::All);
		loaded.vm.LookIn(ScriptType::All);
		ASSERT_EQ(loaded.Global(k_Runs), saved.Global(k_Runs)) << "turn " << turn;
		ASSERT_EQ(loaded.Global(k_Woke), saved.Global(k_Woke)) << "turn " << turn;
		ASSERT_EQ(loaded.vm.GetTasks().size(), saved.vm.GetTasks().size()) << "turn " << turn;
		if (wokeTurn < 0 && saved.Global(k_Woke) == 1)
		{
			wokeTurn = turn;
		}
	}
	// The wait ended after the save, on the same turn in the two machines, and the task that waited then ended
	EXPECT_GT(wokeTurn, 0);
	EXPECT_EQ(loaded.vm.GetTasks().size(), 1u);
	EXPECT_EQ(Bytes(loaded.vm.StateFile()), Bytes(saved.vm.StateFile()));
	// The end of the task lets go of the object of its local in each machine
	EXPECT_EQ(saved.references[k_HeldObject], 0);
	EXPECT_EQ(loaded.references[k_HeldObject], 0);
}

// A saved game of an older build has no running state of the scripts. The machine keeps the scripts that the land started.
TEST(LhvmSaveResume, AFileWithNoRunningStateKeepsTheScriptsOfTheLand)
{
	CountedVm machine;
	ASSERT_EQ(machine.vm.LoadBinary(WaitsAndCounts()), EXIT_SUCCESS);
	machine.vm.LookIn(ScriptType::All);
	const auto before = Bytes(machine.vm.StateFile());
	EXPECT_EQ(machine.vm.RestoreState(Bytes(WaitsAndCounts())), EXIT_FAILURE);
	EXPECT_EQ(Bytes(machine.vm.StateFile()), before);
	machine.vm.LookIn(ScriptType::All);
	EXPECT_EQ(machine.Global(k_Runs), 2);
}
