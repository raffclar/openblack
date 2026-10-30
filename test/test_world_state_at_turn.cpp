/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#if defined(OPENBLACK_DEVELOPER_TOOLS)

#include <cstdint>

#include <array>
#include <filesystem>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "Simulation/WorldStateAtTurn.h"

using namespace openblack;

TEST(WorldStateAtTurn, ATurnListGivesItsTurnsInTheirOrder)
{
	EXPECT_EQ(world_state_at_turn::ParseTurns("250"), std::vector<uint32_t>({250}));
	EXPECT_EQ(world_state_at_turn::ParseTurns("400,100,250"), std::vector<uint32_t>({400, 100, 250}));
	EXPECT_EQ(world_state_at_turn::ParseTurns("0"), std::vector<uint32_t>({0}));
}

TEST(WorldStateAtTurn, ATextThatIsNotATurnListGivesNoTurns)
{
	for (const auto* text : {"", ",", "100,", ",100", "100,,200", "a", "100;200", "-1", "1 2", "4294967296"})
	{
		EXPECT_FALSE(world_state_at_turn::ParseTurns(text).has_value()) << text;
	}
}

TEST(WorldStateAtTurn, TheFilesOfATurnAreInTheFolderWithTheTurnInTheirNames)
{
	const std::filesystem::path folder("states");
	EXPECT_EQ(world_state_at_turn::StateFile(folder, 250), folder / "world-250.obsav");
	EXPECT_EQ(world_state_at_turn::MapFile(folder, 250), folder / "world-250.json");
}

TEST(WorldStateAtTurn, TheMapGivesThePresentationPartsAndTheBytesOfEachSection)
{
	constexpr std::array k_Parts {
	    synced_state::PresentationPart {.component = "Animal", .field = "clipPlace", .marker = ""},
	    synced_state::PresentationPart {.component = "*", .field = "", .marker = "Say \"hi\"\\"},
	};
	const world_state_at_turn::Map map {
	    .turn = 7,
	    .parts = k_Parts,
	    .sections = {{.name = "map", .size = 12, .presentation = {}},
	                 {.name = "localParticles", .size = 40, .presentation = {{0, 40}}},
	                 {.name = "magic", .size = 30, .presentation = {{4, 8}, {20, 24}}}},
	    .syncedSeed = 99,
	    .syncedDraws = 1234567890123,
	    .waitingSounds = 2,
	};
	EXPECT_EQ(world_state_at_turn::MapText(map),
	          R"({"turn": 7, "syncedSeed": 99, "syncedDraws": 1234567890123, "waitingSounds": 2, )"
	          R"("presentationParts": [{"component": "Animal", "field": "clipPlace", "marker": ""}, )"
	          R"({"component": "*", "field": "", "marker": "Say \"hi\"\\"}], )"
	          R"("sections": [{"name": "map", "size": 12, "presentation": []}, )"
	          R"({"name": "localParticles", "size": 40, "presentation": [[0, 40]]}, )"
	          R"({"name": "magic", "size": 30, "presentation": [[4, 8], [20, 24]]}]})"
	          "\n");
}

TEST(WorldStateAtTurn, AMapWithNoPartsAndNoSectionsHasEmptyLists)
{
	const world_state_at_turn::Map map {.turn = 1};
	EXPECT_EQ(world_state_at_turn::MapText(map),
	          R"({"turn": 1, "syncedSeed": 0, "syncedDraws": 0, "waitingSounds": 0, "presentationParts": [], "sections": []})"
	          "\n");
}

#endif
