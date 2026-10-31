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

#include <algorithm>

#include <entt/meta/context.hpp>
#include <gtest/gtest.h>

#include "ECS/Reflection/BinaryArchive.h"
#include "ECS/Systems/Implementations/MagicSystem.h"

using openblack::ecs::reflection::BinaryWriter;
using openblack::ecs::systems::MagicSystem;

namespace
{
/// The numbers of the saved values of the magic system that the tests look at
constexpr size_t k_HandInputValue = 12;
constexpr size_t k_HoldLoopValue = 13;
constexpr size_t k_TurnValue = 15;

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
