/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstddef>
#include <cstdint>

#include <array>
#include <optional>
#include <stdexcept>
#include <vector>

#include <LNDFile.h>
#include <gtest/gtest.h>

#include "3D/LandIslandInterface.h"
#include "ECS/PlacedMemo.h"

using namespace openblack;
using namespace openblack::ecs;

namespace
{
constexpr int32_t k_Fixed = 0x10000;

/// One block of cells at the corner of the map
class OneBlockIsland final: public LandIslandInterface
{
public:
	[[nodiscard]] float GetHeightAt(glm::vec2) const override { return 0.0f; }
	[[nodiscard]] glm::vec3 GetNormalAt(glm::vec2) const override { return {0.0f, 1.0f, 0.0f}; }
	[[nodiscard]] const lnd::LNDCell& GetCell(const glm::u16vec2& coordinates) const override { return *FindCell(coordinates); }
	[[nodiscard]] const lnd::LNDCell* FindCell(const glm::u16vec2& coordinates) const override
	{
		if (coordinates.x >= 16 || coordinates.y >= 16)
		{
			return nullptr;
		}
		return &_cells.at((coordinates.x * 17u) + coordinates.y);
	}
	void DumpTextures() const override {}
	void DumpMaps() const override {}
	[[nodiscard]] std::vector<LandBlock>& GetBlocks() override { throw std::logic_error("no blocks"); }
	[[nodiscard]] const std::vector<LandBlock>& GetBlocks() const override { throw std::logic_error("no blocks"); }
	[[nodiscard]] const std::vector<lnd::LNDCountry>& GetCountries() const override { throw std::logic_error("no countries"); }
	[[nodiscard]] const graphics::Texture2D& GetHeightMap() const override { throw std::logic_error("no maps"); }
	[[nodiscard]] const graphics::Texture2D& GetLuminosityMap() const override { throw std::logic_error("no maps"); }
	[[nodiscard]] const graphics::Texture2D& GetCellColourMap() const override { throw std::logic_error("no maps"); }
	[[nodiscard]] const graphics::Texture2D& GetBlockTextures() const override { throw std::logic_error("no maps"); }
	[[nodiscard]] U16Extent2 GetIndexExtent() const override { return {}; }
	[[nodiscard]] glm::mat4 GetOrthoView() const override { return glm::mat4(1.0f); }
	[[nodiscard]] glm::mat4 GetOrthoProj() const override { return glm::mat4(1.0f); }
	[[nodiscard]] Extent2 GetExtent() const override { return {}; }
	uint8_t GetNoise(glm::u8vec2) override { return 0; }

	/// The corner x and z of the block's 17 x 17 cell array
	lnd::LNDCell& At(int x, int z) { return _cells.at((static_cast<size_t>(x) * 17u) + static_cast<size_t>(z)); }

private:
	std::array<lnd::LNDCell, 17 * 17> _cells {};
};

std::vector<std::byte> Key(float value)
{
	std::vector<std::byte> key;
	placed_memo::Append(key, value);
	return key;
}

/// A calculation that counts its calls and gives one corner at the height it is told
struct FakePlace
{
	int calls {0};
	float height {1.0f};
	std::optional<MapReach> reach;

	std::optional<MapReach> operator()(std::vector<glm::vec3>& corners)
	{
		++calls;
		corners.assign({glm::vec3(0.0f, height, 0.0f)});
		return reach;
	}
};

/// A land that gives one byte, or cannot give its bytes
struct FakeLand
{
	std::byte value {0};
	bool canTell {true};

	bool operator()(const MapReach&, std::vector<std::byte>& bytes) const
	{
		bytes.push_back(value);
		return canTell;
	}
};

const placed_memo::Slot k_Slot {.entity = entt::entity {7}, .geometry = &k_Fixed};
} // namespace

TEST(PlacedMemo, TheSameKeyGivesTheKeptCorners)
{
	placed_memo::Memo memo;
	FakePlace place;
	const FakeLand land;
	EXPECT_EQ(memo.Get(k_Slot, Key(1.0f), place, land)[0].y, 1.0f);
	place.height = 2.0f;
	EXPECT_EQ(memo.Get(k_Slot, Key(1.0f), place, land)[0].y, 1.0f);
	EXPECT_EQ(place.calls, 1);
	EXPECT_EQ(memo.Hits(), 1u);
}

TEST(PlacedMemo, AnyChangedBitCalculatesAgain)
{
	placed_memo::Memo memo;
	FakePlace place;
	const FakeLand land;
	(void)memo.Get(k_Slot, Key(0.0f), place, land);
	place.height = 2.0f;
	// Minus zero is equal to zero, but its bits are not the same
	EXPECT_EQ(memo.Get(k_Slot, Key(-0.0f), place, land)[0].y, 2.0f);
	EXPECT_EQ(place.calls, 2);
}

TEST(PlacedMemo, EachSlotKeepsItsOwnCorners)
{
	placed_memo::Memo memo;
	FakePlace place;
	const FakeLand land;
	(void)memo.Get(k_Slot, Key(1.0f), place, land);
	auto other = k_Slot;
	other.entity = entt::entity {8};
	(void)memo.Get(other, Key(1.0f), place, land);
	EXPECT_EQ(place.calls, 2);
	EXPECT_EQ(memo.Size(), 2u);
}

TEST(PlacedMemo, ChangedLandCalculatesAgain)
{
	placed_memo::Memo memo;
	FakePlace place {.reach = MapReach {.minX = 0, .minZ = 0, .maxX = 1, .maxZ = 1}};
	FakeLand land;
	(void)memo.Get(k_Slot, Key(1.0f), place, land);
	(void)memo.Get(k_Slot, Key(1.0f), place, land);
	EXPECT_EQ(place.calls, 1);
	land.value = std::byte {1};
	place.height = 3.0f;
	EXPECT_EQ(memo.Get(k_Slot, Key(1.0f), place, land)[0].y, 3.0f);
	EXPECT_EQ(place.calls, 2);
}

TEST(PlacedMemo, ALandThatCannotTellIsNeverKept)
{
	placed_memo::Memo memo;
	FakePlace place {.reach = MapReach {.minX = 0, .minZ = 0, .maxX = 1, .maxZ = 1}};
	const FakeLand land {.canTell = false};
	(void)memo.Get(k_Slot, Key(1.0f), place, land);
	(void)memo.Get(k_Slot, Key(1.0f), place, land);
	EXPECT_EQ(place.calls, 2);
	EXPECT_EQ(memo.Hits(), 0u);
}

TEST(PlacedMemo, ASlotNotUsedForTwoPassesIsForgotten)
{
	placed_memo::Memo memo;
	FakePlace place;
	const FakeLand land;
	memo.EndPass();
	(void)memo.Get(k_Slot, Key(1.0f), place, land);
	memo.EndPass();
	memo.EndPass();
	EXPECT_EQ(memo.Size(), 1u);
	memo.EndPass();
	EXPECT_EQ(memo.Size(), 0u);
}

TEST(PlacedMemo, AltitudeInputsFollowOnlyTheCellsOfTheReach)
{
	OneBlockIsland island;
	// The points of the reach are in cells (1, 1) to (2, 1), so corners x 1 to 3 and z 1 to 2 count
	const MapReach reach {.minX = k_Fixed + 5, .minZ = k_Fixed + 5, .maxX = (2 * k_Fixed) + 5, .maxZ = k_Fixed + 9};
	const auto bytesOf = [&] {
		std::vector<std::byte> bytes;
		EXPECT_TRUE(island.AltitudeInputs(reach, bytes));
		return bytes;
	};
	const auto before = bytesOf();
	island.At(5, 5).altitude = 40;
	EXPECT_EQ(bytesOf(), before);
	island.At(3, 2).altitude = 40;
	const auto corner = bytesOf();
	EXPECT_NE(corner, before);
	island.At(2, 1).properties.split = 1;
	EXPECT_NE(bytesOf(), corner);
}

TEST(PlacedMemo, AltitudeInputsTellOffTheMapFromNoBlock)
{
	OneBlockIsland island;
	std::vector<std::byte> offMap;
	EXPECT_TRUE(island.AltitudeInputs({.minX = -5, .minZ = 0, .maxX = -5, .maxZ = 0}, offMap));
	std::vector<std::byte> noBlock;
	EXPECT_TRUE(island.AltitudeInputs({.minX = 20 * k_Fixed, .minZ = 0, .maxX = 20 * k_Fixed, .maxZ = 0}, noBlock));
	EXPECT_EQ(offMap.size(), 1u);
	EXPECT_NE(offMap, noBlock);
}

TEST(PlacedMemo, AVeryLargeReachIsNotCompared)
{
	const OneBlockIsland island;
	std::vector<std::byte> bytes;
	EXPECT_FALSE(island.AltitudeInputs({.minX = 0, .minZ = 0, .maxX = 100 * k_Fixed, .maxZ = 100 * k_Fixed}, bytes));
}
