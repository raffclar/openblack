/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <algorithm>
#include <array>
#include <optional>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "Creature/CreatureIdleMind.h"
#include "Creature/CreaturePlanActions.h"
#include "Creature/CreatureThrow.h"
#include "Creature/CreatureTownTheft.h"

using namespace openblack;
using namespace openblack::creature_mind;
namespace theft = openblack::creature_town_theft;
namespace plan_actions = openblack::creature_plan_actions;

namespace
{
constexpr uint32_t k_Mine = 0;
constexpr uint32_t k_Other = 1;

std::optional<std::vector<Step>> AgendaOf(std::string_view action, std::optional<uint32_t> object,
                                          const plan_actions::Situation& situation)
{
	const auto* executor = plan_actions::For(action);
	if (executor == nullptr)
	{
		return std::nullopt;
	}
	return plan_actions::Agenda(*executor, object, {}, situation, [](uint32_t) { return 0u; }, std::nullopt);
}
} // namespace

TEST(CreatureTownTheft, OnlyABuiltPitOfItsOwnPlayerIsItsOwn)
{
	EXPECT_TRUE(theft::IsOwnStoragePit(true, k_Mine, k_Mine));
	EXPECT_FALSE(theft::IsOwnStoragePit(false, k_Mine, k_Mine));
	EXPECT_FALSE(theft::IsOwnStoragePit(true, k_Other, k_Mine));
	EXPECT_FALSE(theft::IsOwnStoragePit(true, std::nullopt, k_Mine));
	// A creature of no player and a pit of no player have the same player
	EXPECT_TRUE(theft::IsOwnStoragePit(true, std::nullopt, std::nullopt));
}

TEST(CreatureTownTheft, ABuiltPitOfAnyOtherPlayerOrOfNoPlayerCanBeRobbed)
{
	EXPECT_TRUE(theft::IsOthersStoragePit(true, k_Other, k_Mine));
	EXPECT_TRUE(theft::IsOthersStoragePit(true, std::nullopt, k_Mine));
	EXPECT_FALSE(theft::IsOthersStoragePit(false, k_Other, k_Mine));
	EXPECT_FALSE(theft::IsOthersStoragePit(true, k_Mine, k_Mine));
}

TEST(CreatureTownTheft, AFieldOfAnotherPlayerCanBeRobbedWithNoTestOfItsFood)
{
	EXPECT_TRUE(theft::IsOthersField(k_Other, k_Mine));
	EXPECT_TRUE(theft::IsOthersField(std::nullopt, k_Mine));
	EXPECT_FALSE(theft::IsOthersField(k_Mine, k_Mine));
}

TEST(CreatureTownTheft, TheNearestTownIsTheFirstOfTheNearest)
{
	EXPECT_EQ(theft::NearestTown(std::array {30.0f, 10.0f, 10.0f, 20.0f}), std::optional<size_t>(1));
	EXPECT_EQ(theft::NearestTown(std::array<float, 0> {}), std::nullopt);
}

TEST(CreatureTownTheft, WithAnEmptyHandItPicksUpTheThingAndPutsItDownByThePit)
{
	const auto agenda = theft::StealAndPutInTown(7u, 9u, 2.0f);
	ASSERT_EQ(agenda.size(), 3u);
	EXPECT_EQ(agenda.at(0).order.kind, ObjectOrder::Kind::PickUp);
	EXPECT_EQ(agenda.at(0).order.object, std::optional(7u));
	EXPECT_EQ(agenda.at(1).movement.kind, Movement::Kind::GoNearObject);
	EXPECT_EQ(agenda.at(1).movement.object, std::optional(9u));
	EXPECT_FLOAT_EQ(agenda.at(1).movement.maxDistance, 6.0f);
	EXPECT_EQ(agenda.at(2).order.kind, ObjectOrder::Kind::Discard);
	EXPECT_EQ(agenda.at(2).order.animation, creature_throw::k_PutDown);
	EXPECT_TRUE(std::ranges::none_of(agenda, &Step::main));
}

TEST(CreatureTownTheft, WithAFullHandItTakesWhatItHoldsToThePit)
{
	const auto agenda = theft::StealAndPutInTown(std::nullopt, 9u, 1.0f);
	ASSERT_EQ(agenda.size(), 2u);
	EXPECT_FLOAT_EQ(agenda.at(0).movement.maxDistance, 3.0f);
}

TEST(CreatureTownTheft, ItTakesAHandfulFromThePitOfTheOtherPlayerAndThrowsItIntoItsOwn)
{
	const auto agenda = theft::StealFromStore(4u, 5u, ObjectOrder::Handful::Logs, 2.0f);
	ASSERT_EQ(agenda.size(), 6u);
	EXPECT_EQ(agenda.at(0).movement.kind, Movement::Kind::GoNearObject);
	EXPECT_EQ(agenda.at(0).movement.object, std::optional(4u));
	EXPECT_FLOAT_EQ(agenda.at(0).movement.maxDistance, 0.0f);
	EXPECT_EQ(agenda.at(1).movement.kind, Movement::Kind::TurnToFaceObject);
	EXPECT_FLOAT_EQ(agenda.at(1).seconds, 0.1f);
	EXPECT_EQ(agenda.at(2).order.kind, ObjectOrder::Kind::TakeFromPile);
	EXPECT_EQ(agenda.at(2).order.handful, ObjectOrder::Handful::Logs);
	EXPECT_EQ(agenda.at(2).order.object, std::optional(4u));
	EXPECT_EQ(agenda.at(3).movement.kind, Movement::Kind::ToThrowPosition);
	EXPECT_EQ(agenda.at(3).movement.object, std::optional(5u));
	EXPECT_FLOAT_EQ(agenda.at(3).movement.maxDistance, 2.0f);
	EXPECT_EQ(agenda.at(3).face, creature_face::Cue::Compassion);
	EXPECT_EQ(agenda.at(4).effect, Effect::Completed);
	EXPECT_EQ(agenda.at(4).face, creature_face::Cue::None);
	EXPECT_EQ(agenda.at(5).order.kind, ObjectOrder::Kind::ThrowInStore);
	EXPECT_EQ(agenda.at(5).order.object, std::optional(5u));
	EXPECT_TRUE(agenda.at(5).main);
}

TEST(CreatureTownTheft, TheActionsUseTheirOwnStoresAndWhatTheySteal)
{
	const auto* town = plan_actions::For("StealObjectAndTakeToTown");
	ASSERT_NE(town, nullptr);
	EXPECT_EQ(town->target, plan_actions::Target::Stealable);
	const auto* farm = plan_actions::For("StealFoodFromFarm");
	ASSERT_NE(farm, nullptr);
	EXPECT_EQ(farm->target, plan_actions::Target::OwnStoragePit);
	EXPECT_EQ(farm->use, plan_actions::Target::OthersField);
	for (const auto* name : {"StealFoodFromStoragePit", "StealWoodFromStoragePit"})
	{
		const auto* store = plan_actions::For(name);
		ASSERT_NE(store, nullptr);
		EXPECT_EQ(store->target, plan_actions::Target::OwnStoragePit);
		EXPECT_EQ(store->use, plan_actions::Target::OthersStoragePit);
	}
}

TEST(CreatureTownTheft, WithNoPitInATownOfItsPlayerTheTheftToATownHasNoAgenda)
{
	plan_actions::Situation situation;
	EXPECT_FALSE(AgendaOf("StealObjectAndTakeToTown", 7u, situation).has_value());
	situation.ownTownPit = 9u;
	const auto agenda = AgendaOf("StealObjectAndTakeToTown", 7u, situation);
	ASSERT_TRUE(agenda.has_value());
	EXPECT_EQ(agenda->back().order.kind, ObjectOrder::Kind::Discard);
}

TEST(CreatureTownTheft, TheTheftFromAStoreTakesWheatForFoodAndLogsForWood)
{
	plan_actions::Situation situation;
	EXPECT_FALSE(AgendaOf("StealFoodFromStoragePit", 5u, situation).has_value());
	situation.instrument = 4u;
	const auto food = AgendaOf("StealFoodFromStoragePit", 5u, situation);
	const auto wood = AgendaOf("StealWoodFromStoragePit", 5u, situation);
	ASSERT_TRUE(food.has_value());
	ASSERT_TRUE(wood.has_value());
	EXPECT_EQ(food->at(2).order.handful, ObjectOrder::Handful::Wheat);
	EXPECT_EQ(wood->at(2).order.handful, ObjectOrder::Handful::Logs);
}
