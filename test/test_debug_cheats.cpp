/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <gtest/gtest.h>

#include "Common/DebugCheats.h"
#include "ECS/Components/DebugCheats.h"
#include "ECS/Components/Player.h"
#include "ECS/Components/PrayerPower.h"
#include "ECS/Components/WorshipChants.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/Registry.h"

// Enable this define because the test makes the system with its own registry
#define LOCATOR_IMPLEMENTATIONS
#include "ECS/Systems/Implementations/DebugCheatSystem.h"

using namespace openblack;
using namespace openblack::debug_cheats;
using openblack::ecs::components::DebugCheats;
using openblack::ecs::components::DebugCheatSite;
using openblack::ecs::components::Player;
using openblack::ecs::components::PrayerPower;
using openblack::ecs::components::WorshipChants;
using openblack::ecs::components::WorshipSite;

namespace
{

/// A world with two players, a worship site of each, and the prayer power of the first player
struct World
{
	World()
	{
		one = registry.Create();
		registry.Assign<Player>(one).name = PlayerNames::PLAYER_ONE;
		registry.Assign<PrayerPower>(one);
		two = registry.Create();
		registry.Assign<Player>(two).name = PlayerNames::PLAYER_TWO;
		siteOne = registry.Create();
		registry.Assign<WorshipSite>(siteOne).player = PlayerNames::PLAYER_ONE;
		registry.Assign<WorshipChants>(siteOne);
		siteTwo = registry.Create();
		registry.Assign<WorshipSite>(siteTwo).player = PlayerNames::PLAYER_TWO;
		registry.Assign<WorshipChants>(siteTwo);
	}

	ecs::Registry registry;
	ecs::systems::DebugCheatSystem cheats {[this]() -> ecs::Registry* { return &registry; }};
	entt::entity one {entt::null};
	entt::entity two {entt::null};
	entt::entity siteOne {entt::null};
	entt::entity siteTwo {entt::null};
};

} // namespace

TEST(DebugCheats, EachCheatHasItsOwnBitAndName)
{
	EXPECT_EQ(BitOf(Cheat::InfiniteInfluence), 1u);
	EXPECT_EQ(BitOf(Cheat::InfiniteMiracleCharge), 2u);
	EXPECT_EQ(NameOf(Cheat::InfiniteInfluence), "infinite influence");
	EXPECT_EQ(NameOf(Cheat::InfiniteMiracleCharge), "infinite miracle charge");
}

TEST(DebugCheats, CheatsGoOnAndOffAlone)
{
	auto cheats = With(0, Cheat::InfiniteInfluence, true);
	EXPECT_TRUE(Has(cheats, Cheat::InfiniteInfluence));
	EXPECT_FALSE(Has(cheats, Cheat::InfiniteMiracleCharge));
	cheats = With(cheats, Cheat::InfiniteMiracleCharge, true);
	cheats = With(cheats, Cheat::InfiniteInfluence, false);
	EXPECT_FALSE(Has(cheats, Cheat::InfiniteInfluence));
	EXPECT_TRUE(Has(cheats, Cheat::InfiniteMiracleCharge));
}

TEST(DebugCheats, OnlyTheKnownNumbersAreCheats)
{
	EXPECT_EQ(CheatFrom(0), Cheat::InfiniteInfluence);
	EXPECT_EQ(CheatFrom(1), Cheat::InfiniteMiracleCharge);
	EXPECT_FALSE(CheatFrom(static_cast<uint8_t>(k_CheatCount)).has_value());
	EXPECT_FALSE(CheatFrom(255).has_value());
}

TEST(DebugCheats, TheInfiniteMiracleChargeForcesBothFlagsOfASite)
{
	const SiteFlags own {.infinite = false, .freeMaintenance = true};
	EXPECT_EQ(SiteFlagsWith(BitOf(Cheat::InfiniteMiracleCharge), own), (SiteFlags {.infinite = true, .freeMaintenance = true}));
	EXPECT_EQ(SiteFlagsWith(BitOf(Cheat::InfiniteInfluence), own), own);
	EXPECT_EQ(SiteFlagsWith(0, own), own);
}

TEST(DebugCheatSystem, ACheatIsOnTheEntityOfItsPlayerOnly)
{
	World world;
	EXPECT_TRUE(world.cheats.Set(PlayerNames::PLAYER_ONE, Cheat::InfiniteInfluence, true));
	EXPECT_TRUE(world.cheats.Has(PlayerNames::PLAYER_ONE, Cheat::InfiniteInfluence));
	EXPECT_FALSE(world.cheats.Has(PlayerNames::PLAYER_TWO, Cheat::InfiniteInfluence));
	EXPECT_TRUE(world.registry.AllOf<DebugCheats>(world.one));
	EXPECT_FALSE(world.registry.AllOf<DebugCheats>(world.two));
	// The same cheat again changes nothing
	EXPECT_FALSE(world.cheats.Set(PlayerNames::PLAYER_ONE, Cheat::InfiniteInfluence, true));
}

TEST(DebugCheatSystem, APlayerThatTheLandDoesNotHaveGetsNoCheat)
{
	World world;
	EXPECT_FALSE(world.cheats.Set(PlayerNames::PLAYER_FOUR, Cheat::InfiniteInfluence, true));
	EXPECT_EQ(world.cheats.CheatsOf(PlayerNames::PLAYER_FOUR), 0u);
}

TEST(DebugCheatSystem, TheMiracleChargeMakesTheSitesAndThePrayerOfThePlayerInfinite)
{
	World world;
	world.registry.Get<WorshipChants>(world.siteOne).freeMaintenance = true;
	world.cheats.Set(PlayerNames::PLAYER_ONE, Cheat::InfiniteMiracleCharge, true);
	const auto& chants = world.registry.Get<WorshipChants>(world.siteOne);
	EXPECT_TRUE(chants.infinite);
	EXPECT_TRUE(chants.freeMaintenance);
	EXPECT_TRUE(world.registry.Get<PrayerPower>(world.one).infinite);
	// The site of the other player stays as it was
	EXPECT_FALSE(world.registry.Get<WorshipChants>(world.siteTwo).infinite);
	EXPECT_FALSE(world.registry.AllOf<DebugCheatSite>(world.siteTwo));
}

TEST(DebugCheatSystem, OffGivesTheSitesAndThePrayerTheirOwnFlagsBack)
{
	World world;
	world.registry.Get<WorshipChants>(world.siteOne).freeMaintenance = true;
	world.cheats.Set(PlayerNames::PLAYER_ONE, Cheat::InfiniteMiracleCharge, true);
	world.cheats.Set(PlayerNames::PLAYER_ONE, Cheat::InfiniteMiracleCharge, false);
	const auto& chants = world.registry.Get<WorshipChants>(world.siteOne);
	EXPECT_FALSE(chants.infinite);
	EXPECT_TRUE(chants.freeMaintenance);
	EXPECT_FALSE(world.registry.Get<PrayerPower>(world.one).infinite);
	EXPECT_FALSE(world.registry.AllOf<DebugCheatSite>(world.siteOne));
	// A player with no cheat left keeps no record
	EXPECT_FALSE(world.registry.AllOf<DebugCheats>(world.one));
}

TEST(DebugCheatSystem, APlayerWithInfinitePrayerBeforeTheCheatKeepsIt)
{
	World world;
	world.registry.Get<PrayerPower>(world.one).infinite = true;
	world.cheats.Set(PlayerNames::PLAYER_ONE, Cheat::InfiniteMiracleCharge, true);
	world.cheats.Set(PlayerNames::PLAYER_ONE, Cheat::InfiniteMiracleCharge, false);
	EXPECT_TRUE(world.registry.Get<PrayerPower>(world.one).infinite);
}

TEST(DebugCheatSystem, ASiteThatGoesToAnotherPlayerLosesTheCheat)
{
	World world;
	world.cheats.Set(PlayerNames::PLAYER_ONE, Cheat::InfiniteMiracleCharge, true);
	world.registry.Get<WorshipSite>(world.siteOne).player = PlayerNames::PLAYER_TWO;
	world.cheats.ProcessTurn();
	EXPECT_FALSE(world.registry.Get<WorshipChants>(world.siteOne).infinite);
	EXPECT_FALSE(world.registry.AllOf<DebugCheatSite>(world.siteOne));
}

TEST(DebugCheatSystem, ANewSiteOfThePlayerGetsTheCheatAtTheNextTurn)
{
	World world;
	world.cheats.Set(PlayerNames::PLAYER_ONE, Cheat::InfiniteMiracleCharge, true);
	const auto site = world.registry.Create();
	world.registry.Assign<WorshipSite>(site).player = PlayerNames::PLAYER_ONE;
	world.registry.Assign<WorshipChants>(site);
	world.cheats.ProcessTurn();
	EXPECT_TRUE(world.registry.Get<WorshipChants>(site).infinite);
}

TEST(DebugCheatSystem, TheInfluenceCheatChangesNoSite)
{
	World world;
	world.cheats.Set(PlayerNames::PLAYER_ONE, Cheat::InfiniteInfluence, true);
	EXPECT_FALSE(world.registry.Get<WorshipChants>(world.siteOne).infinite);
	EXPECT_FALSE(world.registry.Get<PrayerPower>(world.one).infinite);
}
