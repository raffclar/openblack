/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/
// The players of the magic objects (teleport stones, fire balls) and of the effects: none unless one was given

#include <optional>

#include <gtest/gtest.h>

#include "ECS/Components/MagicFireBall.h"
#include "ECS/Components/MagicTeleport.h"
#include "ECS/Effects/EffectValues.h"
#include "ECS/Registry.h"
#include "Enums.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/Objects/MagicFireBall.h"
#include "Magic/Objects/MagicTeleport.h"

using namespace openblack;

namespace
{
class MagicObjectPlayerTest: public ::testing::Test
{
protected:
	void SetUp() override { Locator::entitiesRegistry::emplace<ecs::Registry>(); }
	void TearDown() override { Locator::entitiesRegistry::reset(); }

	static ecs::Registry& Reg() { return Locator::entitiesRegistry::value(); }
};
} // namespace

TEST_F(MagicObjectPlayerTest, ATeleportStoneHasItsSpellsPlayerOrNone)
{
	const auto none = Reg().Create();
	Reg().Assign<ecs::components::MagicTeleport>(none);
	const auto owned = Reg().Create();
	Reg().Assign<ecs::components::MagicTeleport>(owned).player = PlayerNames::PLAYER_TWO;
	const auto notAStone = Reg().Create();
	EXPECT_EQ(magic::teleport::PlayerOf(none), std::nullopt);
	EXPECT_EQ(magic::teleport::PlayerOf(owned), PlayerNames::PLAYER_TWO);
	EXPECT_EQ(magic::teleport::PlayerOf(notAStone), std::nullopt);
}

TEST_F(MagicObjectPlayerTest, AFireBallCanBeCaughtUnlessItIsTheCatchersOwn)
{
	const auto none = Reg().Create();
	Reg().Assign<ecs::components::MagicFireBall>(none);
	const auto owned = Reg().Create();
	Reg().Assign<ecs::components::MagicFireBall>(owned).player = PlayerNames::PLAYER_ONE;
	const auto notABall = Reg().Create();
	EXPECT_TRUE(magic::fireball::ValidForPlaceInHand(none, PlayerNames::PLAYER_ONE));
	EXPECT_FALSE(magic::fireball::ValidForPlaceInHand(owned, PlayerNames::PLAYER_ONE));
	EXPECT_TRUE(magic::fireball::ValidForPlaceInHand(owned, PlayerNames::PLAYER_TWO));
	EXPECT_TRUE(magic::fireball::ValidForPlaceInHand(notABall, PlayerNames::PLAYER_ONE));
}

TEST(EffectValuesPlayer, NoneUntilOneIsGiven)
{
	// an effect from its info alone has no player (a spell without one leaves it so); the copy keeps a given one
	auto values = ecs::effects::EffectValues::FromEffectInfo(GEffectInfo {});
	EXPECT_EQ(values.player, std::nullopt);
	values.player = PlayerNames::PLAYER_TWO;
	const auto copy = values;
	EXPECT_EQ(copy.player, PlayerNames::PLAYER_TWO);
}
