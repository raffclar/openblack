/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <array>

#include <gtest/gtest.h>

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

	std::optional<entt::entity> Shown(std::optional<entt::entity> underPointer = std::nullopt) const
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
