/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <array>
#include <limits>
#include <vector>

#include <gtest/gtest.h>

#include "ECS/DiscipleGlowDraw.h"
#include "ECS/DiscipleSymbol.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;
namespace disciple_symbol = openblack::ecs::disciple_symbol;

namespace
{

struct Land
{
	Registry land;
	// The hands of the player at this computer, and a hand of another god
	std::array<entt::entity, 2> own {land.Create(), land.Create()};
	entt::entity otherGod {land.Create()};

	Land()
	{
		for (const auto hand : {own[0], own[1], otherGod})
		{
			land.Assign<HandGrab>(hand);
		}
	}

	entt::entity Disciple(VillagerDisciple type = VillagerDisciple::Farmer)
	{
		const auto villager = land.Create();
		land.Assign<Villager>(villager);
		land.Assign<components::Disciple>(villager, components::Disciple {.type = type});
		return villager;
	}

	void Hold(entt::entity hand, entt::entity thing, HandGrab::State state = HandGrab::State::Holding)
	{
		auto& grab = land.Get<HandGrab>(hand);
		grab.state = state;
		grab.object = thing;
	}

	[[nodiscard]] std::optional<entt::entity> Shown(std::optional<entt::entity> underPointer = std::nullopt) const
	{
		return disciple_symbol::Shown(land, own, underPointer);
	}
};

} // namespace

TEST(DiscipleSymbol, TheDiscipleInTheHandOfThePlayerAtThisComputerShowsItsSymbol)
{
	Land world;
	const auto disciple = world.Disciple();
	world.Hold(world.own[0], disciple);
	EXPECT_EQ(world.Shown(), disciple);
}

TEST(DiscipleSymbol, TheSecondHandOfThePlayerAlsoShowsTheSymbol)
{
	Land world;
	const auto disciple = world.Disciple();
	world.Hold(world.own[1], disciple);
	EXPECT_EQ(world.Shown(), disciple);
}

TEST(DiscipleSymbol, ADiscipleInTheHandOfAnotherGodShowsNoSymbol)
{
	Land world;
	world.Hold(world.otherGod, world.Disciple());
	EXPECT_FALSE(world.Shown().has_value());
	// The disciple under the pointer of this computer shows its symbol all the same
	const auto underPointer = world.Disciple(VillagerDisciple::Forester);
	EXPECT_EQ(world.Shown(underPointer), underPointer);
}

TEST(DiscipleSymbol, AHandThatHoldsNoDiscipleLeavesTheSymbolToTheThingUnderThePointer)
{
	Land world;
	const auto person = world.land.Create();
	world.land.Assign<Villager>(person);
	world.Hold(world.own[0], person);
	const auto underPointer = world.Disciple();
	EXPECT_EQ(world.Shown(underPointer), underPointer);
	// An ordinary villager under the pointer shows no symbol
	EXPECT_FALSE(world.Shown(person).has_value());
}

TEST(DiscipleSymbol, AHandThatOnlyGrabsADiscipleDoesNotHoldIt)
{
	Land world;
	const auto grabbed = world.Disciple();
	world.Hold(world.own[0], grabbed, HandGrab::State::Grabbing);
	EXPECT_FALSE(world.Shown().has_value());
	world.Hold(world.own[0], grabbed, HandGrab::State::Taking);
	EXPECT_FALSE(world.Shown().has_value());
	for (const auto state : {HandGrab::State::Holding, HandGrab::State::ReadyToThrow, HandGrab::State::LettingGo})
	{
		world.Hold(world.own[0], grabbed, state);
		EXPECT_EQ(world.Shown(), grabbed);
	}
}

TEST(DiscipleSymbol, ADiscipleThatTheFramesDoNotDrawShowsNoSymbol)
{
	Land world;
	const auto held = world.Disciple();
	world.land.Assign<HiddenByState>(held);
	world.Hold(world.own[0], held);
	// The held disciple takes the symbol, thus the disciple under the pointer does not show it
	EXPECT_FALSE(world.Shown(world.Disciple()).has_value());
}

TEST(DiscipleSymbol, AThingThatIsGoneShowsNoSymbol)
{
	Land world;
	const auto gone = world.Disciple();
	world.Hold(world.own[0], gone);
	world.land.Destroy(gone);
	EXPECT_FALSE(world.Shown(gone).has_value());
}

TEST(DiscipleSymbol, TheSizeOfTheSymbolComesFromTheDistanceToTheGlow)
{
	// The camera is 3 across, 4 up and 12 back from the glow. The symbol stands higher, but its height does not count.
	const glm::vec3 glow {10.0f, 2.0f, 5.0f};
	const glm::vec3 eye {13.0f, 6.0f, -7.0f};
	EXPECT_FLOAT_EQ(disciple_symbol::SymbolDistance(eye, glow), 13.0f);
	EXPECT_FLOAT_EQ(disciple_symbol::SymbolDistance(glow, glow), 0.0f);
}

namespace disciple_glow_draw = openblack::ecs::disciple_glow_draw;

TEST(DiscipleGlowDraw, TheFrameDrawsTheGlowOnlyWithTheUsualBody)
{
	EXPECT_TRUE(disciple_glow_draw::Shown({.highDetail = false, .drawn = true}));
	// The land's draw list leaves the body out
	EXPECT_FALSE(disciple_glow_draw::Shown({.highDetail = false, .drawn = false}));
	// A script draws the villager in high detail
	EXPECT_FALSE(disciple_glow_draw::Shown({.highDetail = true, .drawn = true}));
}

TEST(DiscipleGlowDraw, AllGlowsUpToTheLimitShow)
{
	Registry land;
	std::vector<disciple_glow_draw::Glow> glows;
	for (uint32_t i = 0; i < 3; ++i)
	{
		glows.push_back({.villager = land.Create(), .place = 2 - i});
	}
	EXPECT_EQ(disciple_glow_draw::Kept(glows).size(), 3u);
}

TEST(DiscipleGlowDraw, TheGlowsPastTheLimitInTheFrameOrderDoNotShow)
{
	Registry land;
	const auto listedLate = land.Create();
	const auto listedEarly = land.Create();
	const auto flyingLate = land.Create();
	const auto flyingEarly = land.Create();
	const auto outOfMap = land.Create();
	const auto heldNear = land.Create();
	const auto heldFar = land.Create();
	const auto offScreen = land.Create();
	using Draw = disciple_glow_draw::Draw;
	const std::vector<disciple_glow_draw::Glow> glows {
	    {.villager = heldNear, .draw = Draw::Hand, .key = 10.0f},
	    {.villager = outOfMap, .draw = Draw::Physics, .place = std::numeric_limits<uint32_t>::max()},
	    {.villager = flyingLate, .draw = Draw::Physics, .place = 4},
	    {.villager = listedLate, .place = 7},
	    {.villager = offScreen, .place = 0, .onScreen = false},
	    {.villager = heldFar, .draw = Draw::Hand, .key = 90.0f},
	    {.villager = flyingEarly, .draw = Draw::Physics, .place = 1},
	    {.villager = listedEarly, .place = 3},
	};
	// The land's draw list in its order, the moving bodies in the order of their list, the other things out of the map,
	// then the hands from the farthest. A glow off the screen does not count.
	const std::vector<entt::entity> all {listedEarly, listedLate, flyingEarly, flyingLate, outOfMap, heldFar, heldNear};
	EXPECT_EQ(disciple_glow_draw::Kept(glows, 7), all);
	EXPECT_EQ(disciple_glow_draw::Kept(glows, 3), (std::vector<entt::entity> {listedEarly, listedLate, flyingEarly}));
	EXPECT_EQ(disciple_glow_draw::Kept(glows, 6),
	          (std::vector<entt::entity> {listedEarly, listedLate, flyingEarly, flyingLate, outOfMap, heldFar}));
}

TEST(DiscipleGlowDraw, TheLimitIs1024GlowsAFrame)
{
	Registry land;
	std::vector<disciple_glow_draw::Glow> glows;
	for (uint32_t i = 0; i < 1100; ++i)
	{
		glows.push_back({.villager = land.Create(), .place = 1099 - i});
	}
	const auto kept = disciple_glow_draw::Kept(glows);
	ASSERT_EQ(kept.size(), 1024u);
	// The nearest blocks of the list come first, so the last places of the list lose their glows
	EXPECT_EQ(kept.front(), glows.back().villager);
	EXPECT_EQ(kept.back(), glows.at(1100 - 1024).villager);
}
