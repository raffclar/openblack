/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <array>
#include <optional>
#include <vector>

#include <glm/gtc/matrix_transform.hpp>
#include <gtest/gtest.h>

#include "3D/ScreenPick.h"
#include "ECS/UndrawnPicks.h"
#include "Gui/WorldToolTipRules.h"

namespace undrawn_picks = openblack::ecs::undrawn_picks;
namespace screen_pick = openblack::screen_pick;

namespace
{
constexpr auto k_Temple = static_cast<entt::entity>(10);
constexpr auto k_Tree = static_cast<entt::entity>(11);
constexpr auto k_Entrance = static_cast<entt::entity>(20);
constexpr auto k_OtherEntrance = static_cast<entt::entity>(21);

/// A camera at the origin looking down -z, the cursor in the middle of an 800 by 600 screen
screen_pick::View MiddleView()
{
	constexpr float k_Near = 1.0f;
	const auto projection = glm::perspectiveRH_NO(glm::radians(60.0f), 800.0f / 600.0f, k_Near, 1000.0f);
	const auto view = glm::lookAtRH(glm::vec3(0.0f), glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, 1.0f, 0.0f));
	return {.worldToClip = projection * view,
	        .resolution = {800.0f, 600.0f},
	        .near = k_Near,
	        .xScale = projection[0][0],
	        .camera = glm::vec3(0.0f),
	        .cursor = {400.0f, 300.0f}};
}
} // namespace

TEST(UndrawnPicks, AnUndrawnModelIsTestedJustBeforeTheThingItGoesWith)
{
	const std::array drawn {k_Tree, k_Temple};
	const std::array shapes {undrawn_picks::Shape {.entity = k_Entrance, .drawnWith = k_Temple}};
	std::vector<undrawn_picks::Turn> order;
	undrawn_picks::TestOrder(drawn, shapes, order);
	ASSERT_EQ(order.size(), 3u);
	EXPECT_EQ(order[0].drawnIndex, 0u);
	EXPECT_FALSE(order[0].undrawn.has_value());
	EXPECT_EQ(order[1].undrawn, std::optional(k_Entrance));
	EXPECT_EQ(order[2].drawnIndex, 1u);
	EXPECT_FALSE(order[2].undrawn.has_value());
}

TEST(UndrawnPicks, AnUndrawnModelIsNotTestedWhenItsThingIsNotDrawn)
{
	const std::array drawn {k_Tree};
	const std::array shapes {undrawn_picks::Shape {.entity = k_Entrance, .drawnWith = k_Temple},
	                         undrawn_picks::Shape {.entity = k_OtherEntrance, .drawnWith = k_Tree}};
	std::vector<undrawn_picks::Turn> order {{.drawnIndex = 7}};
	undrawn_picks::TestOrder(drawn, shapes, order);
	ASSERT_EQ(order.size(), 2u);
	EXPECT_EQ(order[0].undrawn, std::optional(k_OtherEntrance));
	EXPECT_FALSE(order[1].undrawn.has_value());
}

// The entrance stands in the temple's doorway: tested first, it keeps the pick on a tie and wherever its triangles are
// nearer, and the temple's walls win wherever they are nearer
TEST(UndrawnPicks, TheEntranceKeepsATieWithItsTempleAndLosesToANearerWall)
{
	const auto view = MiddleView();
	const std::array drawn {k_Temple};
	const std::array shapes {undrawn_picks::Shape {.entity = k_Entrance, .drawnWith = k_Temple}};
	std::vector<undrawn_picks::Turn> order;
	undrawn_picks::TestOrder(drawn, shapes, order);
	ASSERT_EQ(order.size(), 2u);
	const std::array<screen_pick::Candidate, 2> candidates = {{
	    {.centre = {0.0f, 0.0f, -20.0f}, .radius = 5.0f, .origin = {0.0f, 0.0f, -20.0f}},
	    {.centre = {0.0f, 0.0f, -20.0f}, .radius = 30.0f, .origin = {0.0f, 0.0f, -20.0f}},
	}};
	const auto entranceAt = [&order](float entrance, float temple) {
		return [&order, entrance, temple](size_t i) -> std::optional<float> {
			return order[i].undrawn.has_value() ? entrance : temple;
		};
	};
	const auto tie = screen_pick::PickAmong(view, candidates, entranceAt(18.0f, 18.0f));
	ASSERT_TRUE(tie.has_value());
	EXPECT_TRUE(order[tie->index].undrawn.has_value());
	const auto wall = screen_pick::PickAmong(view, candidates, entranceAt(18.0f, 15.0f));
	ASSERT_TRUE(wall.has_value());
	EXPECT_FALSE(order[wall->index].undrawn.has_value());
}

TEST(UndrawnPicks, EntrancesTakeTheCursorFromTheFirstTurnATempleStandsBuilt)
{
	EXPECT_FALSE(undrawn_picks::EntranceTakesCursor(false, 0.0f));
	EXPECT_FALSE(undrawn_picks::EntranceTakesCursor(false, 0.99f));
	EXPECT_TRUE(undrawn_picks::EntranceTakesCursor(false, 1.0f));
	// Once taking it, they keep it whatever the temple is after
	EXPECT_TRUE(undrawn_picks::EntranceTakesCursor(true, 0.0f));
}

TEST(UndrawnPicks, EnterTempleIsOfferedOverTheOwnBuiltTempleWithinInfluence)
{
	using openblack::gui::world_tooltip::OffersEnterTemple;
	EXPECT_TRUE(OffersEnterTemple(true, true, true));
	EXPECT_FALSE(OffersEnterTemple(false, true, true));
	EXPECT_FALSE(OffersEnterTemple(true, false, true));
	EXPECT_FALSE(OffersEnterTemple(true, true, false));
}
