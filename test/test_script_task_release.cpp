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

#include <algorithm>
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

constexpr uint32_t k_Held = 2;       // The one local variable of each script
constexpr uint32_t k_Object = 7;     // The object that a task holds
constexpr uint32_t k_Holder = 0;     // Keeps its parameter in its local and loops
constexpr uint32_t k_Ender = 2;      // Keeps its parameter in its local and ends
constexpr uint32_t k_Stacker = 4;    // Keeps an object on its stack only and loops
constexpr uint32_t k_NullKeeper = 6; // Keeps no object in its local and loops

/// A machine with four scripts. Each records what it tells the game about its references and its end
class ScriptTaskRelease: public ::testing::Test
{
protected:
	void SetUp() override
	{
		const std::vector<VMInstruction> code {
		    Make(Opcode::Pop, VMValue(k_Held), DataType::Object, VMMode::Reference), // 0: POPO held
		    Make(Opcode::Jmp, VMValue(uint32_t {1})),                                // 1: JMP 1, which ends the turn
		    Make(Opcode::Pop, VMValue(k_Held), DataType::Object, VMMode::Reference), // 2: POPO held
		    Make(Opcode::End),                                                       // 3: END
		    Make(Opcode::Push, VMValue(k_Object), DataType::Object),                 // 4: PUSHO 7
		    Make(Opcode::Jmp, VMValue(uint32_t {5})),                                // 5: JMP 5
		    Make(Opcode::Push, VMValue(uint32_t {0}), DataType::Object),             // 6: PUSHO 0
		    Make(Opcode::Pop, VMValue(k_Held), DataType::Object, VMMode::Reference), // 7: POPO held
		    Make(Opcode::Jmp, VMValue(uint32_t {8})),                                // 8: JMP 8
		};
		const std::vector<VMScript> scripts {
		    VMScript("Holder", "test.txt", lhvm::ScriptType::Script, 1, {"held"}, k_Holder, 1, 1),
		    VMScript("Ender", "test.txt", lhvm::ScriptType::Script, 1, {"held"}, k_Ender, 1, 2),
		    VMScript("Stacker", "test.txt", lhvm::ScriptType::Script, 1, {"held"}, k_Stacker, 0, 3),
		    VMScript("NullKeeper", "test.txt", lhvm::ScriptType::Script, 1, {"held"}, k_NullKeeper, 0, 4),
		};
		_vm.Initialise(
		    &_natives, nullptr, nullptr, [this](uint32_t task) { _log.push_back("stop " + std::to_string(task)); }, nullptr,
		    [this](uint32_t object) { _log.push_back("add " + std::to_string(object)); },
		    [this](uint32_t object) { _log.push_back("remove " + std::to_string(object)); });
		ASSERT_EQ(_vm.LoadBinary(lhvm::LHVMFile(lhvm::LHVMVersion::BlackAndWhite, {"unused"}, code, {}, scripts, {})),
		          EXIT_SUCCESS);
	}

	uint32_t StartWithObject(const std::string& name)
	{
		_vm.Push(VMValue(k_Object), DataType::Object);
		return _vm.StartScript(name, lhvm::ScriptType::All);
	}

	std::vector<lhvm::NativeFunction> _natives {lhvm::NativeFunction(nullptr, 0, 0, "NONE")};
	lhvm::LHVM _vm;
	std::vector<std::string> _log;
};
} // namespace

TEST_F(ScriptTaskRelease, AStoppedTaskGivesBackTheObjectInItsLocalAfterItsEndIsTold)
{
	const auto task = StartWithObject("Holder");
	_vm.LookIn(lhvm::ScriptType::All);
	EXPECT_EQ(_log, (std::vector<std::string> {"add 7"}));
	_vm.StopTask(task);
	EXPECT_EQ(_log, (std::vector<std::string> {"add 7", "stop " + std::to_string(task), "remove 7"}));
	EXPECT_FALSE(_vm.GetTasks().contains(task));
}

TEST_F(ScriptTaskRelease, ATaskThatEndsGivesBackTheObjectInItsLocal)
{
	const auto task = StartWithObject("Ender");
	_vm.LookIn(lhvm::ScriptType::All);
	EXPECT_EQ(_log, (std::vector<std::string> {"add 7", "stop " + std::to_string(task), "remove 7"}));
	EXPECT_FALSE(_vm.GetTasks().contains(task));
}

TEST_F(ScriptTaskRelease, StoppingAllTasksGivesBackEveryLocalObject)
{
	StartWithObject("Holder");
	StartWithObject("Holder");
	_vm.LookIn(lhvm::ScriptType::All);
	_vm.StopAllTasks();
	EXPECT_EQ(std::ranges::count(_log, std::string("remove 7")), 2);
	EXPECT_TRUE(_vm.GetTasks().empty());
}

TEST_F(ScriptTaskRelease, AnObjectOnlyOnTheStackHasNoReference)
{
	const auto task = _vm.StartScript("Stacker", lhvm::ScriptType::All);
	_vm.LookIn(lhvm::ScriptType::All);
	_vm.StopTask(task);
	EXPECT_EQ(_log, (std::vector<std::string> {"stop " + std::to_string(task)}));
}

TEST_F(ScriptTaskRelease, ALocalThatHoldsNoObjectGivesAndGivesBackNoReference)
{
	const auto task = _vm.StartScript("NullKeeper", lhvm::ScriptType::All);
	_vm.LookIn(lhvm::ScriptType::All);
	EXPECT_EQ(_vm.GetTasks().at(task).localVars.at(0).type, DataType::Object);
	_vm.StopTask(task);
	EXPECT_EQ(_log, (std::vector<std::string> {"stop " + std::to_string(task)}));
}
