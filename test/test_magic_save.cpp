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
#include <utility>

#include <entt/meta/context.hpp>
#include <gtest/gtest.h>

#include "ECS/Reflection/BinaryArchive.h"
#include "ECS/Systems/Implementations/MagicSystem.h"

using openblack::ecs::systems::MagicSystem;

namespace
{
/// The number of the saved value that holds the press and the button of this computer's hand
constexpr size_t k_HandInputValue = 12;
} // namespace

// The frames change the press and the button of the hand of this computer. The synced state does not include them.
TEST(MagicSave, TheInputOfTheHandOfThisComputerIsAPresentationValue)
{
	const entt::meta_ctx context;
	openblack::ecs::reflection::BinaryWriter out(context);
	out.KeepValueEnds();
	const MagicSystem magic;
	magic.SaveState(out);
	const auto& ends = out.ValueEnds();
	ASSERT_GT(ends.size(), k_HandInputValue);
	const std::pair<size_t, size_t> input {ends.at(k_HandInputValue - 1), ends.at(k_HandInputValue)};
	EXPECT_LT(input.first, input.second);
	EXPECT_NE(std::ranges::find(out.PresentationRanges(), input), out.PresentationRanges().end());
	// The miracles and the turn of the system stay synced
	EXPECT_EQ(std::ranges::find_if(out.PresentationRanges(), [](const auto& range) { return range.first == 0; }),
	          out.PresentationRanges().end());
}
