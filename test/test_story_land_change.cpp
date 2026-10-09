/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdlib>

#include <algorithm>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include <Game.h>
#include <LHVM.h>
#include <gtest/gtest.h>

#include "CHLApi.h"
#include "ECS/Components/Player.h"
#include "ECS/Registry.h"
#include "ECS/Systems/AlignmentSystemInterface.h"
#include "FileSystem/FileSystemInterface.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::lhvm;

namespace
{

constexpr std::string_view k_NextLand = "Scripts/Land1.txt";

VMInstruction Make(Opcode code, VMValue data = VMValue(uint32_t {0}), DataType type = DataType::Int,
                   VMMode mode = VMMode::Immediate)
{
	return {code, mode, type, data, 0};
}

/// The story's control script: it notes it has begun, loads the next land, then counts the turns it goes on running
LHVMFile StoryControl(uint32_t loadMap)
{
	const std::vector<VMInstruction> code {
	    Make(Opcode::Push, VMValue(int32_t {7})),                                    // 0: PUSHI 7
	    Make(Opcode::Pop, VMValue(uint32_t {1}), DataType::Int, VMMode::Reference),  // 1: POPI begun
	    Make(Opcode::Push, VMValue(int32_t {0})),                                    // 2: PUSHI the land's name
	    Make(Opcode::Sys, VMValue(loadMap)),                                         // 3: SYS LOAD_MAP
	    Make(Opcode::Push, VMValue(uint32_t {2}), DataType::Int, VMMode::Reference), // 4: PUSHI turns
	    Make(Opcode::Push, VMValue(int32_t {1})),                                    // 5: PUSHI 1
	    Make(Opcode::Add, VMValue(uint32_t {0})),                                    // 6: ADDI
	    Make(Opcode::Pop, VMValue(uint32_t {2}), DataType::Int, VMMode::Reference),  // 7: POPI turns
	    Make(Opcode::Jmp, VMValue(uint32_t {4})),                                    // 8: JMP 4, which ends the turn
	    Make(Opcode::End),                                                           // 9
	};
	std::vector<char> data(k_NextLand.begin(), k_NextLand.end());
	data.push_back('\0');
	const std::vector<VMScript> scripts {VMScript("LandControl", "test.txt", ScriptType::Script, 2, {}, 0, 0, 1)};
	return {LHVMVersion::BlackAndWhite, {"begun", "turns"}, code, {1}, scripts, data};
}

class StoryLandChange: public ::testing::Test
{
protected:
	void SetUp() override
	{
		static const auto mockGamePath = std::filesystem::path(TEST_BINARY_DIR) / "mock";
		auto args = Arguments {
		    .graphicsBackend = GraphicsBackend::Noop,
		    .gamePath = mockGamePath.string(),
		    .numFramesToSimulate = 0,
		    .logFile = "stdout",
		};
		std::fill_n(args.logLevels.begin(), args.logLevels.size(), spdlog::level::warn);
		_game = std::make_unique<Game>(std::move(args));
		ASSERT_TRUE(_game->Initialize());
		ASSERT_TRUE(_game->LoadMap(Locator::filesystem::value().FindPath(std::string(k_NextLand))));

		auto& vm = Locator::vm::value();
		const auto& natives = Locator::chlapi::value().GetFunctionsTable();
		const auto loadMap = std::ranges::find(natives, std::string("LOAD_MAP"), &NativeFunction::name);
		ASSERT_NE(loadMap, natives.end());
		vm.Initialise(&natives, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
		ASSERT_EQ(vm.LoadBinary(StoryControl(static_cast<uint32_t>(std::distance(natives.begin(), loadMap)))), EXIT_SUCCESS);
	}
	void TearDown() override { _game.reset(); }

	[[nodiscard]] static int32_t Global(size_t index) { return Locator::vm::value().GetVariables().at(index).value.intVal; }

	std::unique_ptr<Game> _game;
};

} // namespace

TEST_F(StoryLandChange, TheNextLandIsLoadedAtOnceAndTheScriptGoesOn)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto lastLands = registry.Create();
	Locator::vm::value().LookIn(ScriptType::All);

	// The last land is gone and the new one laid out, in the turn the script asked
	EXPECT_FALSE(registry.Valid(lastLands));
	EXPECT_TRUE(Locator::terrainSystem::has_value());
	// The script that asked went on in the same turn, on the new land
	EXPECT_EQ(Global(1), 7);
	EXPECT_EQ(Global(2), 1);
	ASSERT_EQ(Locator::vm::value().GetTasks().size(), 1u);

	Locator::vm::value().LookIn(ScriptType::All);
	EXPECT_EQ(Global(2), 2);
}

TEST_F(StoryLandChange, ThePlayersAlignmentGoesOnToTheNextLand)
{
	auto& alignment = Locator::alignmentSystem::value();
	alignment.SetPlayerAlignment(PlayerNames::PLAYER_ONE, -0.4f);
	Locator::vm::value().LookIn(ScriptType::All);
	EXPECT_FLOAT_EQ(alignment.GetPlayerAlignment(PlayerNames::PLAYER_ONE), -0.4f);
	// The new land's player is the only one of that name
	int players = 0;
	Locator::entitiesRegistry::value().Each<const ecs::components::Player>(
	    [&players](entt::entity, const ecs::components::Player& player) {
		    players += player.name == PlayerNames::PLAYER_ONE ? 1 : 0;
	    });
	EXPECT_EQ(players, 1);
}
