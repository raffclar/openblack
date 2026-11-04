/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include <cstddef>
#include <cstdint>

#include <algorithm>
#include <array>
#include <vector>

#include <entt/meta/context.hpp>
#include <gtest/gtest.h>

#include "ECS/Reflection/BinaryArchive.h"
#include "ECS/Systems/Implementations/MagicSystem.h"

using openblack::ecs::reflection::BinaryReader;
using openblack::ecs::reflection::BinaryWriter;
using openblack::ecs::reflection::EntityRemap;
using openblack::ecs::systems::MagicSystem;

namespace
{
/// The numbers of the saved values of the magic system that the tests look at
constexpr size_t k_HandInputValue = 11;
constexpr size_t k_HoldLoopValue = 12;
constexpr size_t k_TurnValue = 14;
/// The number of the last saved value before the place where an older save kept the grid of recent miracles
constexpr size_t k_BeforeOldGridValue = 3;
/// The squares on each side of the grid of recent miracles in an older save, one byte for each square
constexpr size_t k_OldGridSide = 64;

/// Whether a presentation value holds all the bytes from first to the byte before last
bool IsPresentation(const BinaryWriter& out, size_t first, size_t last)
{
	return std::ranges::any_of(out.PresentationRanges(),
	                           [first, last](const auto& range) { return range.first <= first && last <= range.second; });
}
} // namespace

// The frames change the press and the button of the hand of this computer. The synced state does not include them.
TEST(MagicSave, TheInputOfTheHandOfThisComputerIsAPresentationValue)
{
	const entt::meta_ctx context;
	const MagicSystem magic;
	BinaryWriter out(context);
	out.KeepValueEnds();
	magic.SaveState(out);
	const auto& ends = out.ValueEnds();
	ASSERT_GT(ends.size(), k_TurnValue);
	EXPECT_TRUE(IsPresentation(out, ends.at(k_HandInputValue - 1), ends.at(k_HandInputValue)));
	// The miracles of the system stay synced
	EXPECT_FALSE(IsPresentation(out, 0, 1));
}

// The press of the hand on a fireball and the circle that the hand drew are of this computer only. The turn of the
// system after the circle stays synced.
TEST(MagicSave, TheFireBallPressAndTheDrawnCircleArePresentationValues)
{
	const entt::meta_ctx context;
	const MagicSystem magic;
	BinaryWriter out(context);
	out.KeepValueEnds();
	magic.SaveState(out);
	const auto& ends = out.ValueEnds();
	ASSERT_GT(ends.size(), k_TurnValue);
	EXPECT_TRUE(IsPresentation(out, ends.at(k_HoldLoopValue - 1), ends.at(k_HoldLoopValue - 1) + 1));
	EXPECT_TRUE(IsPresentation(out, ends.at(k_TurnValue - 1), ends.at(k_TurnValue - 1) + 1));
	EXPECT_FALSE(IsPresentation(out, ends.at(k_TurnValue) - 1, ends.at(k_TurnValue)));
}

// A save does not keep the grid of recent miracles. A section of an older save keeps it, and the load of that section
// reads past it.
TEST(MagicSave, TheLoadOfAnOlderSectionReadsPastTheGridOfRecentMiracles)
{
	const entt::meta_ctx context;
	MagicSystem magic;
	BinaryWriter out(context);
	out.KeepValueEnds();
	magic.SaveState(out);
	const std::vector<uint8_t> bytes = out.Bytes();
	const auto gridAt = static_cast<std::ptrdiff_t>(out.ValueEnds().at(k_BeforeOldGridValue));
	// The grid as an older save wrote it
	std::array<std::array<uint8_t, k_OldGridSide>, k_OldGridSide> grid {};
	grid.at(3).at(5) = 0xFF;
	BinaryWriter gridOut(context);
	openblack::ecs::reflection::Save(gridOut, grid);
	std::vector<uint8_t> older(bytes.begin(), bytes.begin() + gridAt);
	older.insert(older.end(), gridOut.Bytes().begin(), gridOut.Bytes().end());
	older.insert(older.end(), bytes.begin() + gridAt, bytes.end());

	const EntityRemap identity;
	BinaryReader current(context, bytes, identity);
	EXPECT_TRUE(magic.LoadState(current));
	EXPECT_EQ(current.Remaining(), 0);

	ASSERT_NE(magic.OldSaveState(), nullptr);
	BinaryReader old(context, older, identity);
	EXPECT_TRUE(magic.OldSaveState()->LoadState(old));
	EXPECT_EQ(old.Remaining(), 0);
}
