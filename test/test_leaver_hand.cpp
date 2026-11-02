/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <optional>
#include <set>
#include <string>
#include <vector>

#include <entt/entity/entity.hpp>
#include <gtest/gtest.h>

#include "ECS/LeaverHand.h"

namespace
{

using openblack::PlayerNames;
using namespace openblack::ecs::leaver_hand;

/// A world that keeps the calls in their order
class FakeLeaverWorld final: public LeaverHandWorldInterface
{
public:
	bool TakeOffLeash(PlayerNames player) override
	{
		calls.emplace_back("leash");
		players.insert(player);
		return holdsLeash;
	}
	void CancelCharges(PlayerNames player) override
	{
		calls.emplace_back("charges");
		players.insert(player);
	}
	[[nodiscard]] std::optional<entt::entity> HeldSeed(PlayerNames /*player*/) const override { return seed; }
	[[nodiscard]] std::optional<entt::entity> HeldThing(PlayerNames /*player*/) const override { return thing; }
	[[nodiscard]] bool IsAvailable(entt::entity thing) const override { return !unavailable.contains(thing); }
	void DeleteSeed(entt::entity deleted) override { calls.emplace_back("delete seed " + Number(deleted)); }
	void DeleteThing(entt::entity deleted) override { calls.emplace_back("delete thing " + Number(deleted)); }

	static std::string Number(entt::entity entity) { return std::to_string(entt::to_integral(entity)); }

	bool holdsLeash {false};
	std::optional<entt::entity> seed;
	std::optional<entt::entity> thing;
	std::set<entt::entity> unavailable;
	std::vector<std::string> calls;
	std::set<PlayerNames> players;
};

} // namespace

TEST(LeaverHand, TheLeashComesOffThenTheChargesStopThenTheHeldThingGoes)
{
	FakeLeaverWorld world;
	world.holdsLeash = true;
	world.thing = entt::entity {7};
	const auto release = ReleaseHand(world, PlayerNames::PLAYER_TWO);
	EXPECT_TRUE(release.leashOff);
	EXPECT_EQ(release.deleted, entt::entity {7});
	EXPECT_EQ(world.calls, (std::vector<std::string> {"leash", "charges", "delete thing 7"}));
	EXPECT_EQ(world.players, (std::set<PlayerNames> {PlayerNames::PLAYER_TWO}));
}

TEST(LeaverHand, AHeldMiracleSeedGoesAndDoesNotFall)
{
	FakeLeaverWorld world;
	world.seed = entt::entity {3};
	const auto release = ReleaseHand(world, PlayerNames::PLAYER_ONE);
	EXPECT_FALSE(release.leashOff);
	EXPECT_EQ(release.deleted, entt::entity {3});
	EXPECT_EQ(world.calls, (std::vector<std::string> {"leash", "charges", "delete seed 3"}));
}

TEST(LeaverHand, AnEmptyHandOnlyStopsTheLeashAndTheCharges)
{
	FakeLeaverWorld world;
	const auto release = ReleaseHand(world, PlayerNames::PLAYER_THREE);
	EXPECT_FALSE(release.deleted.has_value());
	EXPECT_EQ(world.calls, (std::vector<std::string> {"leash", "charges"}));
}

TEST(LeaverHand, AThingThatTheWorldCannotDealWithStays)
{
	FakeLeaverWorld world;
	world.thing = entt::entity {9};
	world.unavailable.insert(entt::entity {9});
	const auto release = ReleaseHand(world, PlayerNames::PLAYER_TWO);
	EXPECT_FALSE(release.deleted.has_value());
	EXPECT_EQ(world.calls, (std::vector<std::string> {"leash", "charges"}));
}
