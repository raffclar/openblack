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

#include "ScriptHeaders/ScriptStart.h"

using namespace openblack;
using lhvm::DataType;
using lhvm::Opcode;
using lhvm::ScriptType;
using lhvm::VMInstruction;
using lhvm::VMMode;
using lhvm::VMScript;
using lhvm::VMValue;

namespace
{
VMInstruction Make(Opcode code, VMValue data = VMValue(uint32_t {0}))
{
	return {code, VMMode::Immediate, DataType::Int, data, 0};
}

/// A machine with one script of each kind that the game starts in a network game or alone. Each script waits forever.
/// The first script is also in the list of scripts that start when the program loads.
class ScriptStartRule: public ::testing::Test
{
protected:
	void SetUp() override
	{
		const std::vector<VMInstruction> code {
		    Make(Opcode::Jmp, VMValue(uint32_t {0})), // 0: JMP 0, which ends the turn
		};
		const std::vector<VMScript> scripts {
		    VMScript("World", "test.txt", ScriptType::Script, 0, {}, 0, 0, 1),
		    VMScript("Help", "test.txt", ScriptType::Help, 0, {}, 0, 0, 2),
		    VMScript("ChallengeHelp", "test.txt", ScriptType::ChallengeHelp, 0, {}, 0, 0, 3),
		    VMScript("TempleHelp", "test.txt", ScriptType::TempleHelp, 0, {}, 0, 0, 4),
		    VMScript("TempleSpecial", "test.txt", ScriptType::TempleSpecial, 0, {}, 0, 0, 5),
		    VMScript("Multiplayer", "test.txt", ScriptType::Multiplayer, 0, {}, 0, 0, 6),
		    VMScript("MultiplayerHelp", "test.txt", ScriptType::MultiplayerHelp, 0, {}, 0, 0, 7),
		};
		_vm.Initialise(&_natives, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
		ASSERT_EQ(_vm.LoadBinary(lhvm::LHVMFile(lhvm::LHVMVersion::BlackAndWhite, {}, code, {1}, scripts, {})), EXIT_SUCCESS);
	}

	[[nodiscard]] bool Starts(const std::string& name, bool networkGame)
	{
		return _vm.StartScript(name, script::start::StartableScripts(networkGame)) != 0;
	}

	std::vector<lhvm::NativeFunction> _natives {lhvm::NativeFunction(nullptr, 0, 0, "NONE")};
	lhvm::LHVM _vm;
};
} // namespace

TEST(ScriptStart, TheKindsAreTheGames)
{
	// Alone the game starts all seven kinds; in a network game only the two kinds of multiplayer script
	EXPECT_EQ(static_cast<uint32_t>(script::start::StartableScripts(false)), 0x7Fu);
	EXPECT_EQ(static_cast<uint32_t>(script::start::StartableScripts(true)), 0x60u);
}

TEST_F(ScriptStartRule, AloneEveryKindStarts)
{
	for (const auto* name : {"World", "Help", "ChallengeHelp", "TempleHelp", "TempleSpecial", "Multiplayer", "MultiplayerHelp"})
	{
		EXPECT_TRUE(Starts(name, false)) << name;
	}
}

TEST_F(ScriptStartRule, ANetworkGameStartsOnlyTheMultiplayerScripts)
{
	for (const auto* name : {"World", "Help", "ChallengeHelp", "TempleHelp", "TempleSpecial"})
	{
		EXPECT_FALSE(Starts(name, true)) << name;
	}
	EXPECT_TRUE(Starts("Multiplayer", true));
	EXPECT_TRUE(Starts("MultiplayerHelp", true));
}

TEST_F(ScriptStartRule, AScriptThatDoesNotStartMakesNoTask)
{
	// Only the script that started when the program loaded is there
	const auto before = _vm.GetTasks().size();
	EXPECT_FALSE(Starts("World", true));
	EXPECT_EQ(_vm.GetTasks().size(), before);
	EXPECT_TRUE(Starts("MultiplayerHelp", true));
	EXPECT_EQ(_vm.GetTasks().size(), before + 1);
}

TEST_F(ScriptStartRule, TheScriptsOfTheLoadStartOfAnyKind)
{
	// The program's list of scripts that start when it loads does not look at the kind, also in a network game
	ASSERT_EQ(_vm.GetTasks().size(), 1u);
	EXPECT_EQ(_vm.GetTasks().begin()->second.name, "World");
}
