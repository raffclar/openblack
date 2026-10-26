/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>
#include <cstring>

#include <array>

#include <gtest/gtest.h>

#include "ECS/Components/Scaffold.h"
#include "ECS/Components/Workshop.h"
#include "ECS/Registry.h"
#include "ECS/WorkshopRules.h"

using namespace openblack::ecs;
using namespace openblack::ecs::components;
using Place = Workshop::Place;

namespace
{
uint32_t BitsOf(float value)
{
	uint32_t bits = 0;
	std::memcpy(&bits, &value, sizeof(bits));
	return bits;
}

workshop::TurnFacts Facts(bool functional, int32_t room, uint32_t wood)
{
	return {.functional = functional, .room = room, .wood = wood, .scaffoldWood = 100};
}
} // namespace

// The scale of a scaffold of each worth, as the game's floats give it
TEST(WorkshopRules, ScaleOfValueMatchesTheGameFloats)
{
	constexpr std::array<uint32_t, 7> k_Bits {0x3F000000, 0x3F333333, 0x3F666666, 0x3F8CCCCD,
	                                          0x3FA66666, 0x3FC00000, 0x3FD9999A};
	for (uint32_t value = 0; value < k_Bits.size(); ++value)
	{
		EXPECT_EQ(BitsOf(workshop::ScaleOfValue(value)), k_Bits.at(value)) << value;
	}
}

// The worth of a scaffold comes back from its scale, also from the scale that a script gives
TEST(WorkshopRules, ValueOfScaleRoundsToTheNearestWorth)
{
	for (uint32_t value = 0; value < 7; ++value)
	{
		EXPECT_EQ(workshop::ValueOfScale(workshop::ScaleOfValue(value)), value) << value;
	}
	EXPECT_EQ(workshop::ValueOfScale(1.0f), 3u);
	EXPECT_EQ(workshop::ValueOfScale(0.79f), 1u);
	EXPECT_EQ(workshop::ValueOfScale(0.81f), 2u);
}

// The game cuts the fraction off the time to make a scaffold
TEST(WorkshopRules, MakingTurnsCutsTheFractionOff)
{
	EXPECT_EQ(workshop::MakingTurns(40.9f), 40);
	EXPECT_EQ(workshop::MakingTurns(40.0f), 40);
	EXPECT_EQ(workshop::MakingTurns(0.5f), 0);
}

// A workshop starts a scaffold only when it works, has room and has the wood for it
TEST(WorkshopRules, StartsWithRoomAndWood)
{
	int32_t turns = 0;
	EXPECT_EQ(workshop::NextEvent(turns, Facts(true, 1, 100)), workshop::TurnEvent::Starts);
	EXPECT_EQ(workshop::NextEvent(turns, Facts(true, 1, 99)), workshop::TurnEvent::None);
	EXPECT_EQ(workshop::NextEvent(turns, Facts(true, 0, 500)), workshop::TurnEvent::None);
	EXPECT_EQ(workshop::NextEvent(turns, Facts(false, 3, 500)), workshop::TurnEvent::None);
	// The room can be less than 0, which counts as room
	EXPECT_EQ(workshop::NextEvent(turns, Facts(true, -1, 100)), workshop::TurnEvent::Starts);
	EXPECT_EQ(turns, 0);
}

// A workshop counts its turns down and finishes the scaffold at 0; it does not count while it does not work
TEST(WorkshopRules, CountsDownAndFinishes)
{
	int32_t turns = 3;
	EXPECT_EQ(workshop::NextEvent(turns, Facts(true, 0, 0)), workshop::TurnEvent::None);
	EXPECT_EQ(turns, 2);
	EXPECT_EQ(workshop::NextEvent(turns, Facts(false, 0, 0)), workshop::TurnEvent::None);
	EXPECT_EQ(turns, 2);
	EXPECT_EQ(workshop::NextEvent(turns, Facts(true, 0, 0)), workshop::TurnEvent::None);
	EXPECT_EQ(workshop::NextEvent(turns, Facts(true, 0, 0)), workshop::TurnEvent::Finishes);
	EXPECT_EQ(turns, 0);
}

TEST(WorkshopRules, FirstFreePlaceIsTheFirstPlaceWhenNoneIsFree)
{
	EXPECT_EQ(workshop::FirstFreePlace({Place::Free, Place::Free, Place::Free}), 0);
	EXPECT_EQ(workshop::FirstFreePlace({Place::Holds, Place::Away, Place::Free}), 2);
	EXPECT_EQ(workshop::FirstFreePlace({Place::Holds, Place::Free, Place::Free}), 1);
	EXPECT_EQ(workshop::FirstFreePlace({Place::Holds, Place::Away, Place::Holds}), 0);
}

// A workshop lets a scaffold go when it lay for the turns after a hand put it down, unless a script gave its building
TEST(WorkshopRules, KeepsScaffoldUntilItLayForTheTurns)
{
	// A scaffold that no hand put down lay for no turns
	EXPECT_TRUE(workshop::KeepsScaffold(0, 5000, 150, false));
	EXPECT_FALSE(workshop::KeepsScaffold(0, 5000, 0, false));
	EXPECT_TRUE(workshop::KeepsScaffold(1000, 1149, 150, false));
	EXPECT_FALSE(workshop::KeepsScaffold(1000, 1150, 150, false));
	EXPECT_TRUE(workshop::KeepsScaffold(1000, 1150, 150, true));
}

TEST(WorkshopRules, AddAndRemoveKeepTheCount)
{
	Registry registry;
	const auto a = registry.Create();
	const auto b = registry.Create();
	const auto c = registry.Create();
	Workshop data;
	workshop::AddScaffold(data, a);
	workshop::AddScaffold(data, b);
	workshop::AddScaffold(data, c);
	ASSERT_EQ(data.scaffolds.size(), 3u);
	EXPECT_EQ(data.scaffolds.front(), c);
	EXPECT_EQ(data.counted, 3u);
	data.places = {Place::Holds, Place::Holds, Place::Holds};
	// The yard is not full now: the town wants again what it needs
	EXPECT_TRUE(workshop::RemoveScaffold(data, b, 1));
	EXPECT_EQ(data.counted, 2u);
	EXPECT_EQ(data.places.at(1), Place::Free);
	// A scaffold that the workshop does not have changes nothing
	EXPECT_FALSE(workshop::RemoveScaffold(data, b, 0));
	EXPECT_EQ(data.places.at(0), Place::Holds);
	EXPECT_FALSE(workshop::RemoveScaffold(data, a, 0));
	EXPECT_EQ(data.counted, 1u);
	// None counts, but its count goes up
	workshop::AddScaffold(data, entt::null);
	EXPECT_EQ(data.scaffolds.size(), 1u);
	EXPECT_EQ(data.counted, 2u);
}

TEST(WorkshopRules, StandsInYardOnlyInAPlaceThatHoldsIt)
{
	Registry registry;
	const auto owner = registry.Create();
	auto& data = registry.Assign<Workshop>(owner);
	data.places = {Place::Free, Place::Holds, Place::Away};
	const auto held = registry.Create();
	registry.Assign<Scaffold>(held, Scaffold {.workshop = owner, .place = 1});
	const auto away = registry.Create();
	registry.Assign<Scaffold>(away, Scaffold {.workshop = owner, .place = 2});
	const auto loose = registry.Create();
	registry.Assign<Scaffold>(loose, Scaffold {});
	const auto other = registry.Create();
	EXPECT_TRUE(workshop::StandsInYard(registry, held));
	EXPECT_FALSE(workshop::StandsInYard(registry, away));
	EXPECT_FALSE(workshop::StandsInYard(registry, loose));
	EXPECT_FALSE(workshop::StandsInYard(registry, other));
}
