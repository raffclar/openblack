/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <array>
#include <string>

#include <glm/glm.hpp>
#include <gtest/gtest.h>

#include "Common/FixedFormat.h"
#include "ECS/Components/FloatingNumber.h"
#include "ECS/FloatingNumber.h"
#include "ECS/Registry.h"
#include "Gui/ToolTips.h"

using namespace openblack;
namespace fn = openblack::ecs::floating_number;

TEST(FixedFormat, WholeNumbersThreeWideRoundHalfAwayFromNothing)
{
	EXPECT_EQ(fixed_format::Fixed(30.0, 3, 0), " 30");
	EXPECT_EQ(fixed_format::Fixed(100.0, 3, 0), "100");
	EXPECT_EQ(fixed_format::Fixed(5.0, 3, 0), "  5");
	// The game's C library rounds a half up
	EXPECT_EQ(fixed_format::Fixed(12.5, 3, 0), " 13");
	EXPECT_EQ(fixed_format::Fixed(0.5, 3, 0), "  1");
	EXPECT_EQ(fixed_format::Fixed(12.49, 3, 0), " 12");
	EXPECT_EQ(fixed_format::Fixed(1234.0, 3, 0), "1234");
	EXPECT_EQ(fixed_format::Fixed(-2.5, 3, 0), " -3");
	EXPECT_EQ(fixed_format::Fixed(3.14159, 0, 2), "3.14");
	EXPECT_EQ(fixed_format::Fixed(0.05, 0, 1), "0.1");
}

TEST(FixedFormat, ANumberIsWrittenIntoTheWords)
{
	EXPECT_EQ(fixed_format::WithNumber(u"People Worshipping: %3.0f%%", 40.0), u"People Worshipping:  40%");
	EXPECT_EQ(fixed_format::WithNumber(u"Amount: %3.0f", 7.5), u"Amount:   8");
	EXPECT_EQ(fixed_format::WithNumber(u"No number", 7.0), u"No number");
	EXPECT_EQ(fixed_format::WithNumber(u"%d stays", 7.0), u"%d stays");
}

TEST(FloatingNumber, ItRisesTenUnitsASecondForFiveSeconds)
{
	ecs::components::FloatingNumber number {.text = " 30", .position = glm::vec3(0.0f, 2.0f, 0.0f)};
	EXPECT_TRUE(fn::Step(number, 0.5f));
	EXPECT_FLOAT_EQ(number.position.y, 7.0f);
	EXPECT_FLOAT_EQ(number.life, 4.5f);
	EXPECT_TRUE(fn::Step(number, 4.5f));
	EXPECT_FALSE(fn::Step(number, 0.01f));
}

TEST(FloatingNumber, ItFadesInItsLastSecond)
{
	EXPECT_EQ(fn::Alpha(5.0f), 0xFF);
	EXPECT_EQ(fn::Alpha(1.0f), 0xFF);
	EXPECT_EQ(fn::Alpha(0.5f), 127);
	EXPECT_FALSE(fn::Alpha(0.003f).has_value());
	EXPECT_FALSE(fn::Alpha(-0.1f).has_value());
}

TEST(FloatingNumber, ItsTextEndsAtItsPoint)
{
	EXPECT_EQ(fn::TextPlace(glm::vec2(100.0f, 50.0f), 24.0f), glm::vec2(76.0f, 50.0f));
}

TEST(FloatingNumber, ThoseWhoseTimeIsUpGo)
{
	ecs::Registry registry;
	const auto early = registry.Create();
	registry.Assign<ecs::components::FloatingNumber>(early, ecs::components::FloatingNumber {.life = 0.5f});
	const auto late = registry.Create();
	registry.Assign<ecs::components::FloatingNumber>(late, ecs::components::FloatingNumber {.life = 5.0f});
	fn::StepAll(registry, 1.0f);
	EXPECT_FALSE(registry.Valid(early));
	EXPECT_TRUE(registry.Valid(late));
}

namespace
{
std::array<gui::ToolTipInfo, gui::ToolTips::k_Count> ToolTipTable()
{
	std::array<gui::ToolTipInfo, gui::ToolTips::k_Count> info {};
	info.fill({.priority = 0.5f, .displayTime = 1.0f, .displayTimeAfterFocus = 1.0f});
	return info;
}
} // namespace

TEST(ToolTipsForce, AForcedTooltipShowsAtOnceWithItsNumber)
{
	const auto table = ToolTipTable();
	gui::ToolTips toolTips(table);
	toolTips.SetLevel(gui::ToolTipLevel::All);
	toolTips.Submit(3, gui::ToolTipAction::Select, gui::ToolTipArrows::k_None);
	toolTips.ProcessTurn();
	toolTips.Force(120, 40.0f);
	toolTips.ProcessTurn();
	toolTips.Update(1.0f);
	const auto shown = toolTips.GetShown();
	ASSERT_TRUE(shown.has_value());
	EXPECT_EQ(shown->index, 120u);
	EXPECT_EQ(shown->action, gui::ToolTipAction::None);
	ASSERT_TRUE(shown->number.has_value());
	EXPECT_FLOAT_EQ(*shown->number, 40.0f);
}
