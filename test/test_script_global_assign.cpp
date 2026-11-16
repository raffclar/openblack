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

#include <string>
#include <vector>

#include <LHVM.h>
#include <gtest/gtest.h>

using namespace openblack;
using lhvm::DataType;
using lhvm::Opcode;
using lhvm::VMInstruction;
using lhvm::VMMode;
using lhvm::VMValue;

namespace
{
constexpr uint32_t k_Global = 1;    // The place of the one global of the program
constexpr uint32_t k_Object = 1913; // The object that the global holds

/// A machine with one global and no scripts. It records what it tells the game about its references.
class ScriptGlobalAssign: public ::testing::Test
{
protected:
	void SetUp() override { Load(_vm, _log); }

	void Load(lhvm::LHVM& vm, std::vector<std::string>& log)
	{
		vm.Initialise(
		    &_natives, nullptr, nullptr, nullptr, nullptr,
		    [&log](uint32_t object) { log.push_back("add " + std::to_string(object)); },
		    [&log](uint32_t object) { log.push_back("remove " + std::to_string(object)); });
		const std::vector<VMInstruction> code {{Opcode::End, VMMode::Immediate, DataType::Int, VMValue(uint32_t {0}), 0}};
		ASSERT_EQ(vm.LoadBinary(lhvm::LHVMFile(lhvm::LHVMVersion::BlackAndWhite, {"MyCreature"}, code, {}, {}, {})),
		          EXIT_SUCCESS);
	}

	std::vector<lhvm::NativeFunction> _natives {lhvm::NativeFunction(nullptr, 0, 0, "NONE")};
	lhvm::LHVM _vm;
	std::vector<std::string> _log;
};
} // namespace

TEST_F(ScriptGlobalAssign, AGlobalStartsAsANumber)
{
	EXPECT_EQ(_vm.GetVariables().at(k_Global).type, DataType::Float);
}

TEST_F(ScriptGlobalAssign, AnAssignedObjectGivesTheGlobalTheObjectTypeAndAReference)
{
	_vm.AssignVariable(k_Global, VMValue(k_Object), DataType::Object);
	EXPECT_EQ(_vm.GetVariables().at(k_Global).type, DataType::Object);
	EXPECT_EQ(_vm.GetVariables().at(k_Global).value.uintVal, k_Object);
	EXPECT_EQ(_log, (std::vector<std::string> {"add 1913"}));
}

TEST_F(ScriptGlobalAssign, ANumberInPlaceOfAnObjectGivesBackTheReference)
{
	_vm.AssignVariable(k_Global, VMValue(k_Object), DataType::Object);
	_vm.AssignVariable(k_Global, VMValue(2.0f), DataType::Float);
	EXPECT_EQ(_vm.GetVariables().at(k_Global).type, DataType::Float);
	EXPECT_EQ(_log, (std::vector<std::string> {"add 1913", "remove 1913"}));
}

TEST_F(ScriptGlobalAssign, SetVariableKeepsTheTypeOfTheGlobal)
{
	_vm.SetVariable(k_Global, VMValue(k_Object));
	EXPECT_EQ(_vm.GetVariables().at(k_Global).type, DataType::Float);
	EXPECT_TRUE(_log.empty());
}

TEST_F(ScriptGlobalAssign, AnObjectGlobalKeepsItsTypeAfterASaveAndALoad)
{
	_vm.AssignVariable(k_Global, VMValue(k_Object), DataType::Object);
	const auto saved = _vm.SaveState();

	lhvm::LHVM loaded;
	std::vector<std::string> log;
	Load(loaded, log);
	ASSERT_EQ(loaded.RestoreState(saved), EXIT_SUCCESS);
	ASSERT_GT(loaded.GetVariables().size(), k_Global);
	EXPECT_EQ(loaded.GetVariables().at(k_Global).name, "MyCreature");
	EXPECT_EQ(loaded.GetVariables().at(k_Global).type, DataType::Object);
	EXPECT_EQ(loaded.GetVariables().at(k_Global).value.uintVal, k_Object);
}
