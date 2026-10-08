/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The town's script number and FindTownWithID: the scripts' number
// (Town::scriptId, else Town::id) found in the players' town lists in order, the first match winning, so the towns a
// scaffold founds can share 0xABA52; openblack's unique key Town::id (TownByKey) is not that number.

#define LOCATOR_IMPLEMENTATIONS

#include <optional>

#include <gtest/gtest.h>

#include "ECS/Components/Town.h"
#include "ECS/Registry.h"
#include "ECS/Town/TownQueries.h"
#include "Enums.h"
#include "Locator.h"
#include "support/WorldSystems.h"

using namespace openblack;
using namespace openblack::ecs::components;
namespace town_queries = openblack::ecs::town_queries;

namespace
{
class TownScriptIdTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		Locator::entitiesRegistry::emplace<ecs::Registry>();
		openblack::test::EmplaceWorldSystems();
	}
	void TearDown() override
	{
		Locator::entitiesRegistry::reset();
		openblack::test::ResetWorldSystems();
	}
	static ecs::Registry& Reg() { return Locator::entitiesRegistry::value(); }

	/// A town of key `id`, owned by `owner`, joined its owner's list `stamp`-th
	static entt::entity MakeTown(uint32_t id, PlayerNames owner, uint32_t stamp, std::optional<uint32_t> scriptId = {})
	{
		const auto e = Reg().Create();
		auto& t = Reg().Assign<Town>(e, id);
		t.owner = owner;
		t.ownerListStamp = stamp;
		t.scriptId = scriptId;
		Reg().Context().towns[id] = e;
		return e;
	}
};
} // namespace

TEST_F(TownScriptIdTest, AScriptTownIsFoundByItsId)
{
	const auto t = MakeTown(3, PlayerNames::PLAYER_ONE, 1);
	EXPECT_EQ(town_queries::ScriptIdOf(t), 3u);
	EXPECT_TRUE(town_queries::FindTownWithID(3) == t);
	EXPECT_TRUE(town_queries::TownByKey(3) == t);
	EXPECT_TRUE(town_queries::FindTownWithID(4) == entt::null);
}

TEST_F(TownScriptIdTest, FoundedTownsShareTheirNumberAndTheFirstInTheListWins)
{
	// a scaffold's building: every founded town's script number is 0xABA52, each its own key
	const auto first = MakeTown(10, PlayerNames::PLAYER_ONE, 1, 0xABA52u);
	const auto second = MakeTown(11, PlayerNames::PLAYER_ONE, 2, 0xABA52u);
	EXPECT_TRUE(town_queries::FindTownWithID(0xABA52) == first); // the head of the owner's list (the older join)
	EXPECT_TRUE(town_queries::TownByKey(11) == second);
	// the key is not the script number
	EXPECT_TRUE(town_queries::FindTownWithID(11) == entt::null);
}

TEST_F(TownScriptIdTest, ThePlayersAreWalkedInOrder)
{
	// the player slots in order, the neutral one last
	const auto neutral = MakeTown(20, PlayerNames::NEUTRAL, 1, 7u);
	const auto two = MakeTown(21, PlayerNames::PLAYER_TWO, 1, 7u);
	const auto one = MakeTown(22, PlayerNames::PLAYER_ONE, 1, 7u);
	EXPECT_TRUE(town_queries::FindTownWithID(7) == one);
	static_cast<void>(two);
	static_cast<void>(neutral);
}

TEST_F(TownScriptIdTest, TheListOrderNotTheCreationOrderDecides)
{
	// a town taken over joins its new owner's list at the tail: made first, found second
	const auto madeFirst = MakeTown(30, PlayerNames::PLAYER_ONE, 2, 9u);
	const auto madeSecond = MakeTown(31, PlayerNames::PLAYER_ONE, 1, 9u);
	EXPECT_TRUE(town_queries::FindTownWithID(9) == madeSecond);
	static_cast<void>(madeFirst);
}
