/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <array>

#include <gtest/gtest.h>

#include "Debug/MemoryUseRules.h"

using namespace openblack::debug::memory_use;

TEST(MemoryUse, AStorageHoldsTheEntityAndTheComponentForEachPlaceItHasRoomFor)
{
	EXPECT_EQ(BytesOf({.name = "Transform", .count = 3, .capacity = 8, .componentSize = 64}, 4), 8u * (4u + 64u));
	// A component with no data counts only its entity
	EXPECT_EQ(BytesOf({.name = "Tag", .count = 3, .capacity = 4, .componentSize = 0}, 4), 16u);
}

TEST(MemoryUse, TheLargestStorageComesFirstAndTheSameSizesByName)
{
	const std::array storages {
	    Storage {.name = "Small", .count = 1, .capacity = 1, .componentSize = 4},
	    Storage {.name = "Large", .count = 10, .capacity = 16, .componentSize = 100},
	    Storage {.name = "BTie", .count = 2, .capacity = 2, .componentSize = 4},
	    Storage {.name = "ATie", .count = 2, .capacity = 2, .componentSize = 4},
	};
	const auto rows = Rows(storages, 4);
	ASSERT_EQ(rows.size(), 4u);
	EXPECT_EQ(rows[0].name, "Large");
	EXPECT_EQ(rows[0].count, 10u);
	EXPECT_EQ(rows[1].name, "ATie");
	EXPECT_EQ(rows[2].name, "BTie");
	EXPECT_EQ(rows[3].name, "Small");
	EXPECT_EQ(TotalBytes(rows), (16u * 104u) + (2u * 8u) + (2u * 8u) + 8u);
}
