/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <array>

#include <gtest/gtest.h>

#include "Creature/CreatureDrink.h"
#include "Creature/CreatureLayers.h"

using namespace openblack;
using creature_drink::Place;
using creature_mind::Movement;
using creature_mind::Step;

TEST(CreatureDrink, TheNearestWaterPointMustBeNearerThanAThousand)
{
	const std::array points {glm::vec2(0.0f, 500.0f), glm::vec2(0.0f, 300.0f), glm::vec2(0.0f, 1200.0f)};
	EXPECT_EQ(creature_drink::NearestWaterPoint({0.0f, 0.0f}, points), glm::vec2(0.0f, 300.0f));
	// A point a thousand away or further does not count
	const std::array far {glm::vec2(0.0f, 1000.0f)};
	EXPECT_FALSE(creature_drink::NearestWaterPoint({0.0f, 0.0f}, far).has_value());
	EXPECT_FALSE(creature_drink::NearestWaterPoint({0.0f, 0.0f}, {}).has_value());
}

TEST(CreatureDrink, OfWaterPointsAtEqualDistanceTheLastMadeCounts)
{
	const std::array points {glm::vec2(0.0f, 100.0f), glm::vec2(100.0f, 0.0f)};
	EXPECT_EQ(creature_drink::NearestWaterPoint({0.0f, 0.0f}, points), glm::vec2(100.0f, 0.0f));
}

TEST(CreatureDrink, ItStandsAtTheWaterPointOrAtAPlaceNearerThanThirty)
{
	// It can stand at the point
	EXPECT_EQ(creature_drink::StandingPoint({10.0f, 4.0f, 20.0f}, true, std::nullopt), glm::vec2(10.0f, 20.0f));
	// The search found a place near enough
	EXPECT_EQ(creature_drink::StandingPoint({10.0f, 0.0f, 20.0f}, false, glm::vec2(10.0f, 45.0f)), glm::vec2(10.0f, 45.0f));
	// Too far: 30 or more
	EXPECT_FALSE(creature_drink::StandingPoint({10.0f, 0.0f, 20.0f}, false, glm::vec2(10.0f, 50.0f)).has_value());
	// The height of the ground at the point counts in the distance, as in the game
	EXPECT_FALSE(creature_drink::StandingPoint({10.0f, 20.0f, 20.0f}, false, glm::vec2(10.0f, 45.0f)).has_value());
	// The search found nothing
	EXPECT_FALSE(creature_drink::StandingPoint({10.0f, 0.0f, 20.0f}, false, std::nullopt).has_value());
}

TEST(CreatureDrink, AtAWaterPointItWalksWithinItsHeightAndDrinks)
{
	const auto agenda = creature_drink::Drink(Place {.walkTo = glm::vec2(40.0f, 50.0f)}, 18.0f);
	ASSERT_EQ(agenda.size(), 2);
	EXPECT_EQ(agenda[0].kind, Step::Kind::Move);
	EXPECT_EQ(agenda[0].movement.kind, Movement::Kind::ToPoint);
	EXPECT_EQ(agenda[0].movement.point, glm::vec2(40.0f, 50.0f));
	EXPECT_FLOAT_EQ(agenda[0].movement.maxDistance, 18.0f);
	EXPECT_TRUE(agenda[0].movement.giveUpIfUnreachable);
	EXPECT_EQ(agenda[1].kind, Step::Kind::Action);
	EXPECT_EQ(agenda[1].animation, creature_layers::animations::k_Drink);
	EXPECT_EQ(agenda[1].effect, creature_mind::Effect::Drink);
}

TEST(CreatureDrink, AtACoastItWalksThereTurnsToTheSeaAndDrinks)
{
	const auto agenda = creature_drink::Drink(Place {.walkTo = glm::vec2(40.0f, 50.0f), .sea = glm::vec2(40.0f, 90.0f)}, 18.0f);
	ASSERT_EQ(agenda.size(), 3);
	EXPECT_EQ(agenda[1].movement.kind, Movement::Kind::TurnToFace);
	EXPECT_EQ(agenda[1].movement.point, glm::vec2(40.0f, 90.0f));
	// In the water already, it only turns to the sea
	const auto inWater = creature_drink::Drink(Place {.sea = glm::vec2(40.0f, 90.0f)}, 18.0f);
	ASSERT_EQ(inWater.size(), 2);
	EXPECT_EQ(inWater[0].movement.kind, Movement::Kind::TurnToFace);
}

TEST(CreatureDrink, NoPlaceToDrinkStopsTheWantOfWaterAndToGetColder)
{
	creature_desires::Desires desires;
	creature_drink::NoPlaceFound(desires, 10.0f);
	using creature_desires::Desire;
	EXPECT_EQ(desires[Desire::Water].suppressedTurns, 300u);
	EXPECT_EQ(desires[Desire::GetColder].suppressedTurns, 300u);
	EXPECT_EQ(desires[Desire::Hunger].suppressedTurns, 0u);
}
