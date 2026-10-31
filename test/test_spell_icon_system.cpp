/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "ECS/Components/Player.h"
#include "ECS/Components/SpellIcon.h"
#include "ECS/Components/SpellSeed.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/PlayerMiracles.h"
#include "ECS/Registry.h"
#include "ECS/Systems/Implementations/SpellIconSystem.h"
#include "ECS/Systems/MagicSystemInterface.h"
#include "ECS/Systems/PlaygroundTripSystemInterface.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/WorshipBattery.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;

namespace
{
/// Magic that only records the seeds it is asked to delete, and deletes them
class SeedDeletingMagic final: public MagicSystemInterface
{
public:
	entt::entity CastAtPoint(MagicType, PlayerNames, glm::vec3, const magic::SpellCastData&,
	                         const particles::ProcessInfo&) override
	{
		return entt::null;
	}
	entt::entity CastOnObject(MagicType, PlayerNames, entt::entity, const magic::SpellCastData&,
	                          const particles::ProcessInfo&) override
	{
		return entt::null;
	}
	void CloseDown(entt::entity) override {}
	[[nodiscard]] bool CanCastAt(MagicType, PlayerNames, glm::vec3) override { return false; }
	bool SpellEvent(entt::entity, const particles::SpellEventInfo&) override { return false; }
	void PayForSpell(entt::entity, float) override {}
	entt::entity CreateDispenser(glm::vec3, MagicType, float) override { return entt::null; }
	void SetDispenserPeriod(entt::entity, float) override {}
	entt::entity CreateOneOffSeed(glm::vec3, SpellSeedType, int, float) override { return entt::null; }
	entt::entity CreateOneOffSeedFor(glm::vec3, MagicType) override { return entt::null; }
	bool Remove(entt::entity) override { return false; }
	entt::entity GiveSeedToHand(PlayerNames, SpellSeedType, int, float) override { return entt::null; }
	entt::entity SummonSeed(PlayerNames, SpellSeedType, int) override { return entt::null; }
	entt::entity SummonSeedAtSite(entt::entity, SpellSeedType, int) override { return entt::null; }
	void DiscardHeldSeed() override {}
	void DiscardHeldSeedOf(PlayerNames) override {}
	void DeleteSeed(entt::entity seed) override
	{
		deleted.push_back(seed);
		Locator::entitiesRegistry::value().Destroy(seed);
	}
	void UpdateHand(const HandFrame&, const input_steps::Steps&) override {}
	bool TapFireBall(entt::entity) override { return false; }
	bool TapGlobe(entt::entity, PlayerNames) override { return false; }
	bool PressFireBall(uint32_t, uint32_t) override { return false; }
	void HoldFireBall(uint32_t, uint32_t) override {}
	void ReleaseFireBall(uint32_t, uint32_t) override {}
	[[nodiscard]] std::optional<entt::entity> OrbUnderHand() const override { return std::nullopt; }
	bool PressAction() override { return false; }
	void ReleaseAction() override {}
	[[nodiscard]] bool IsHandBusy() const override { return false; }
	[[nodiscard]] bool IsHandBusyOf(PlayerNames player) const override { return GetHeldSeedOf(player).has_value(); }
	[[nodiscard]] magic::PourPose GetHandPour(float) const override { return {}; }
	[[nodiscard]] HandCastState GetHandCastState() const override { return {}; }
	[[nodiscard]] std::optional<entt::entity> GetHeldSeed() const override { return held; }
	[[nodiscard]] std::optional<entt::entity> GetHeldSeedOf(PlayerNames player) const override
	{
		return player == heldBy ? held : std::nullopt;
	}
	void SetIconSeedPowerUp(entt::entity seed, int powerUp) override { poweredUp.emplace_back(seed, powerUp); }
	entt::entity TakeIconSeed(entt::entity site, PlayerNames holder, SpellSeedType seed, int /*powerUp*/, float chants) override
	{
		auto& registry = Locator::entitiesRegistry::value();
		const auto made = registry.Create();
		registry.Assign<SpellSeed>(made, SpellSeed {.seedType = seed, .player = holder, .chantStore = chants});
		taken.push_back(site);
		holders.push_back(holder);
		return made;
	}
	[[nodiscard]] HandResult GetLastHandResult() const override { return {}; }
	bool SendSpellEvent(entt::entity, const particles::SpellEventInfo&) override { return false; }
	float ForcePayForSpell(entt::entity, float) override { return 0.0f; }
	[[nodiscard]] float SpellStrength(entt::entity) override { return 0.0f; }
	void ProcessTurn() override {}
	void Update(float) override {}
	void Reset() override {}
	void SetIgnoreInfluence(bool) override {}
	void DriveHand(std::optional<HandFrame>) override {}
	[[nodiscard]] std::optional<HandFrame> GetDrivenHand() const override { return std::nullopt; }
	[[nodiscard]] bool IsIgnoringInfluence() const override { return false; }
	[[nodiscard]] std::vector<SpellInfo> GetSpells() const override { return {}; }
	void RainOnFire(const glm::vec3&) override {}
	[[nodiscard]] std::vector<DispenserInfo> GetDispensers() const override { return {}; }

	std::vector<entt::entity> deleted;
	std::optional<entt::entity> held;
	/// The player whose hand holds the seed
	PlayerNames heldBy {PlayerNames::PLAYER_ONE};
	std::vector<std::pair<entt::entity, int>> poweredUp;
	/// The sites that gave a seed to a hand, in order, and the players whose hands took the seeds
	std::vector<entt::entity> taken;
	std::vector<PlayerNames> holders;
};

/// The trip to the Gods' Playground, with every player's influence on the whole land or not
class FakePlaygroundTrip final: public PlaygroundTripSystemInterface
{
public:
	explicit FakePlaygroundTrip(bool everywhere)
	    : _everywhere(everywhere)
	{
	}
	[[nodiscard]] bool InfluenceEverywhere() const override { return _everywhere; }
	[[nodiscard]] bool IsLessonRunning() const override { return false; }
	void StartLesson() override {}
	bool AskLessonEnd() override { return false; }
	void EndTurn() override {}
	bool SaveBeforeTrip() override { return false; }
	bool Travel() override { return false; }
	bool Leave() override { return false; }

private:
	bool _everywhere;
};

class SpellIconSystemTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		Locator::entitiesRegistry::emplace<Registry>();
		_magic = &static_cast<SeedDeletingMagic&>(Locator::magicSystem::emplace<SeedDeletingMagic>());
	}
	void TearDown() override
	{
		Locator::magicSystem::reset();
		Locator::entitiesRegistry::reset();
	}

	[[nodiscard]] static Registry& Entities() { return Locator::entitiesRegistry::value(); }

	/// An icon of a site, charging its miracle with some prayer power stored, with a seed out from it or none
	entt::entity Icon(entt::entity site, SpellSeedType seed, entt::entity seedOut)
	{
		auto& registry = Entities();
		const auto icon = registry.Create();
		registry.Assign<WorshipIcon>(
		    icon,
		    WorshipIcon {.site = site, .seed = seed, .charging = true, .charger = PlayerNames::PLAYER_ONE, .store = 50.0f});
		if (seedOut != entt::null)
		{
			registry.Get<WorshipIcon>(icon).seedsOut.push_back(seedOut);
		}
		registry.Get<SiteIcons>(site).icons.push_back(icon);
		return icon;
	}

	SeedDeletingMagic* _magic {nullptr};
	SpellIconSystem _system;
};

TEST_F(SpellIconSystemTest, AGoneSiteTakesItsIconsAndTheSeedsOutFromThemButNotTheCentresHands)
{
	auto& registry = Entities();
	const auto site = registry.Create();
	registry.Assign<SiteIcons>(site);
	const auto seed = registry.Create();
	registry.Assign<SpellSeed>(seed, SpellSeed {.seedType = SpellSeedType::Heal});
	const auto heal = Icon(site, SpellSeedType::Heal, seed);
	const auto water = Icon(site, SpellSeedType::Water, entt::null);
	// A hand on the village centre showing the same miracle belongs to the town
	const auto hand = registry.Create();
	registry.Assign<TownCentreIcon>(hand, TownCentreIcon {.seed = SpellSeedType::Heal});
	// Another site's icon is not touched
	const auto other = registry.Create();
	registry.Assign<SiteIcons>(other);
	const auto otherIcon = Icon(other, SpellSeedType::Heal, entt::null);

	_system.SiteRemoved(site);

	EXPECT_FALSE(registry.Valid(heal));
	EXPECT_FALSE(registry.Valid(water));
	EXPECT_TRUE(registry.Get<const SiteIcons>(site).icons.empty());
	EXPECT_EQ(_magic->deleted, std::vector<entt::entity> {seed});
	EXPECT_TRUE(registry.Valid(hand));
	EXPECT_TRUE(registry.Valid(otherIcon));
	EXPECT_EQ(registry.Get<const SiteIcons>(other).icons, std::vector<entt::entity> {otherIcon});
}

TEST_F(SpellIconSystemTest, ASeedAlreadyGoneFromAnIconIsNotDeletedAgain)
{
	auto& registry = Entities();
	const auto site = registry.Create();
	registry.Assign<SiteIcons>(site);
	const auto seed = registry.Create();
	registry.Assign<SpellSeed>(seed);
	const auto icon = Icon(site, SpellSeedType::Heal, seed);
	registry.Destroy(seed);

	_system.SiteRemoved(site);

	EXPECT_FALSE(registry.Valid(icon));
	EXPECT_TRUE(_magic->deleted.empty());
}

TEST_F(SpellIconSystemTest, ASiteWithoutIconsLosesNothing)
{
	auto& registry = Entities();
	const auto site = registry.Create();
	_system.SiteRemoved(site);
	_system.SiteRemoved(entt::null);
	EXPECT_TRUE(_magic->deleted.empty());
}
TEST_F(SpellIconSystemTest, AnIconWhoseSeedIsHeldAndFullStopsChargingAndKeepsItsStore)
{
	auto& registry = Entities();
	const auto site = registry.Create();
	registry.Assign<SiteIcons>(site);
	registry.Assign<WorshipSite>(site, WorshipSite {.player = PlayerNames::PLAYER_ONE});
	const auto seed = registry.Create();
	registry.Assign<SpellSeed>(seed, SpellSeed {.seedType = SpellSeedType::Heal, .chantStore = 10.0f});
	const auto icon = Icon(site, SpellSeedType::Heal, seed);
	registry.Get<WorshipIcon>(icon).powerUp = 1;
	_magic->held = seed;

	// The held seed holds more than the miracle costs, so the icon needs nothing
	auto charges = _system.ChargesAt(site);
	ASSERT_EQ(charges.size(), 1u);
	EXPECT_TRUE(charges[0].seedHeld);
	EXPECT_TRUE(charges[0].seedOut);
	EXPECT_LE(magic::IconNeed(charges[0]), 0.0f);
	_system.ChargedAt(site, charges);

	const auto& after = registry.Get<const WorshipIcon>(icon);
	EXPECT_FALSE(after.charging);
	EXPECT_EQ(after.store, 50.0f);
	EXPECT_EQ(registry.Get<const SpellSeed>(seed).chantStore, 10.0f);
	EXPECT_EQ(_magic->poweredUp, (std::vector<std::pair<entt::entity, int>> {{seed, 1}}));
}

TEST_F(SpellIconSystemTest, ASeedOutOfTheHandIsOutButNotHeld)
{
	auto& registry = Entities();
	const auto site = registry.Create();
	registry.Assign<SiteIcons>(site);
	registry.Assign<WorshipSite>(site, WorshipSite {.player = PlayerNames::PLAYER_ONE});
	const auto seed = registry.Create();
	registry.Assign<SpellSeed>(seed, SpellSeed {.seedType = SpellSeedType::Heal});
	Icon(site, SpellSeedType::Heal, seed);

	const auto charges = _system.ChargesAt(site);
	ASSERT_EQ(charges.size(), 1u);
	EXPECT_TRUE(charges[0].seedOut);
	EXPECT_FALSE(charges[0].seedHeld);
}

TEST_F(SpellIconSystemTest, ASeedHeldByTheHandOfAnotherPlayerDoesNotCount)
{
	auto& registry = Entities();
	const auto site = registry.Create();
	registry.Assign<SiteIcons>(site);
	registry.Assign<WorshipSite>(site, WorshipSite {.player = PlayerNames::PLAYER_ONE});
	const auto seed = registry.Create();
	registry.Assign<SpellSeed>(seed, SpellSeed {.seedType = SpellSeedType::Heal, .player = PlayerNames::PLAYER_TWO});
	Icon(site, SpellSeedType::Heal, seed);
	_magic->held = seed;
	_magic->heldBy = PlayerNames::PLAYER_TWO;

	EXPECT_FALSE(_system.ChargesAt(site).at(0).seedHeld);
}

// In a network game the hand of each player holds its own seed: the seed in the hand of the second player counts at the
// site of the second player
TEST_F(SpellIconSystemTest, ASeedInTheHandOfTheSecondPlayerCountsAtTheSiteOfThatPlayer)
{
	auto& registry = Entities();
	const auto site = registry.Create();
	registry.Assign<SiteIcons>(site);
	registry.Assign<WorshipSite>(site, WorshipSite {.player = PlayerNames::PLAYER_TWO});
	const auto seed = registry.Create();
	registry.Assign<SpellSeed>(seed, SpellSeed {.seedType = SpellSeedType::Heal, .player = PlayerNames::PLAYER_TWO});
	Icon(site, SpellSeedType::Heal, seed);
	_magic->held = seed;
	_magic->heldBy = PlayerNames::PLAYER_TWO;

	EXPECT_TRUE(_system.ChargesAt(site).at(0).seedHeld);
}
} // namespace

TEST_F(SpellIconSystemTest, APlayerMadeAfterItsTownsMayCastWhatTheTownsOffer)
{
	auto& registry = Entities();
	const auto town = registry.Create();
	registry.Assign<Town>(town, 3u, PlayerNames::PLAYER_TWO);
	// The town comes to offer a miracle while its player has no entity yet, as a god that joins a network game
	ASSERT_TRUE(_system.TownHolds(town, MagicType::Heal));
	const auto playerEntity = registry.Create();
	registry.Assign<Player>(playerEntity, PlayerNames::PLAYER_TWO);
	EXPECT_FALSE(player_miracles::IsMagicTypeEnabled(registry.Get<Player>(playerEntity).miracles, MagicType::Heal));
	_system.PlayerMade(PlayerNames::PLAYER_TWO);
	EXPECT_TRUE(player_miracles::IsMagicTypeEnabled(registry.Get<Player>(playerEntity).miracles, MagicType::Heal));
	EXPECT_FALSE(player_miracles::IsMagicTypeEnabled(registry.Get<Player>(playerEntity).miracles, MagicType::Teleport));
}

/// A computer god starts the charge of an icon at a site of the player. The icon has this much prayer power stored, and
/// the miracle costs 100.
class GodChargeTest: public SpellIconSystemTest
{
protected:
	void SetUp() override
	{
		SpellIconSystemTest::SetUp();
		// The tables are large: made on the heap
		_info = std::make_unique<InfoConstants>();
		_info->spellSeed.at(static_cast<size_t>(SpellSeedType::Heal)).magicTypes.at(0) = MagicType::Heal;
		_info->magicEffect.at(static_cast<size_t>(MagicType::Heal)).costToCreate = 100.0f;
		Locator::infoConstants::emplace(*_info);
	}
	void TearDown() override
	{
		Locator::infoConstants::reset();
		SpellIconSystemTest::TearDown();
	}

	/// An icon of the heal miracle that does not charge, at a site of the player, who may cast the miracle
	entt::entity IdleIcon(PlayerNames player, float store)
	{
		auto& registry = Entities();
		const auto playerEntity = registry.Create();
		registry.Assign<Player>(playerEntity, player);
		player_miracles::SetMagicTypeEnabled(registry.Get<Player>(playerEntity).miracles, MagicType::Heal, true);
		const auto site = registry.Create();
		registry.Assign<SiteIcons>(site);
		registry.Assign<WorshipSite>(site, WorshipSite {.player = player});
		const auto icon = Icon(site, SpellSeedType::Heal, entt::null);
		auto& component = registry.Get<WorshipIcon>(icon);
		component.charging = false;
		component.charger = PlayerNames::NEUTRAL;
		component.store = store;
		return icon;
	}

	std::unique_ptr<InfoConstants> _info;
};

// With no hands in the game, only the first player is a human. A full icon gives the miracle to the empty hand of a
// human god at the start of the charge, as a tap does.
TEST_F(GodChargeTest, AFullIconGivesItsMiracleToTheEmptyHandOfAHumanGod)
{
	const auto icon = IdleIcon(PlayerNames::PLAYER_ONE, 100.0f);
	ASSERT_TRUE(_system.StartChargeForGod(icon, PlayerNames::PLAYER_ONE));
	const auto& after = Entities().Get<const WorshipIcon>(icon);
	EXPECT_TRUE(after.charging);
	EXPECT_EQ(after.charger, PlayerNames::PLAYER_ONE);
	ASSERT_EQ(_magic->taken.size(), 1u);
	EXPECT_EQ(_magic->taken[0], after.site);
	EXPECT_EQ(after.store, 0.0f);
	EXPECT_EQ(after.seedsOut.size(), 1u);
}

// The hand of a computer god never takes the miracle: the icon keeps its full store for the cast of the god
TEST_F(GodChargeTest, AFullIconKeepsItsMiracleForAComputerGod)
{
	const auto icon = IdleIcon(PlayerNames::PLAYER_TWO, 100.0f);
	ASSERT_TRUE(_system.StartChargeForGod(icon, PlayerNames::PLAYER_TWO));
	const auto& after = Entities().Get<const WorshipIcon>(icon);
	EXPECT_TRUE(after.charging);
	EXPECT_TRUE(_magic->taken.empty());
	EXPECT_EQ(after.store, 100.0f);
	EXPECT_TRUE(after.seedsOut.empty());
	EXPECT_EQ(_system.ChantNeeded(icon), 0.0f);
}

// An icon that is not full only starts to charge, also for a human god
TEST_F(GodChargeTest, AnIconThatIsNotFullOnlyStartsToCharge)
{
	const auto icon = IdleIcon(PlayerNames::PLAYER_ONE, 40.0f);
	ASSERT_TRUE(_system.StartChargeForGod(icon, PlayerNames::PLAYER_ONE));
	EXPECT_TRUE(_magic->taken.empty());
	EXPECT_EQ(Entities().Get<const WorshipIcon>(icon).store, 40.0f);
	EXPECT_EQ(_system.ChantNeeded(icon), 60.0f);
}

// A seed of the icon in the hand of the god does not stop the charge, but the icon gives no second seed
TEST_F(GodChargeTest, ASeedOfTheIconInTheHandDoesNotStopTheCharge)
{
	auto& registry = Entities();
	const auto icon = IdleIcon(PlayerNames::PLAYER_ONE, 100.0f);
	const auto seed = registry.Create();
	registry.Assign<SpellSeed>(seed, SpellSeed {.seedType = SpellSeedType::Heal, .player = PlayerNames::PLAYER_ONE});
	registry.Get<WorshipIcon>(icon).seedsOut.push_back(seed);
	_magic->held = seed;
	_magic->heldBy = PlayerNames::PLAYER_ONE;
	ASSERT_TRUE(_system.StartChargeForGod(icon, PlayerNames::PLAYER_ONE));
	const auto& after = Entities().Get<const WorshipIcon>(icon);
	EXPECT_TRUE(after.charging);
	EXPECT_TRUE(_magic->taken.empty());
	EXPECT_EQ(after.store, 100.0f);
	EXPECT_EQ(after.seedsOut.size(), 1u);
}

// The charge does not ask if the god may cast the miracle: the wish of the god asked that before
TEST_F(GodChargeTest, TheChargeStartsAlsoForAMiracleThatTheGodMayNotCast)
{
	auto& registry = Entities();
	const auto icon = IdleIcon(PlayerNames::PLAYER_TWO, 40.0f);
	registry.Each<Player>(
	    [](Player& player) { player_miracles::SetMagicTypeEnabled(player.miracles, MagicType::Heal, false); });
	ASSERT_TRUE(_system.StartChargeForGod(icon, PlayerNames::PLAYER_TWO));
	EXPECT_TRUE(Entities().Get<const WorshipIcon>(icon).charging);
	// An icon that charges already refuses the god
	EXPECT_FALSE(_system.StartChargeForGod(icon, PlayerNames::PLAYER_TWO));
}

// The cast of a god takes the cost of the miracle from the store of the icon, and never more than the store holds
TEST_F(GodChargeTest, ACastTakesTheCostButNeverMoreThanTheStore)
{
	const auto icon = IdleIcon(PlayerNames::PLAYER_TWO, 150.0f);
	_system.TakeCastCharge(icon);
	EXPECT_EQ(Entities().Get<const WorshipIcon>(icon).store, 50.0f);
	_system.TakeCastCharge(icon);
	EXPECT_EQ(Entities().Get<const WorshipIcon>(icon).store, 0.0f);
}

// A script finds the worship site of an icon: the site of a site's icon, or the site of the town of a stone hand
TEST_F(SpellIconSystemTest, TheSiteOfAnIconIsItsOwnOrTheSiteOfTheTownOfAStoneHand)
{
	auto& registry = Entities();
	const auto site = registry.Create();
	registry.Assign<SiteIcons>(site);
	const auto icon = Icon(site, SpellSeedType::Heal, entt::null);
	const auto town = registry.Create();
	registry.Assign<Town>(town).worshipSite = site;
	const auto hand = registry.Create();
	registry.Assign<TownCentreIcon>(hand, TownCentreIcon {.town = town, .seed = SpellSeedType::Heal});
	const auto homeless = registry.Create();
	registry.Assign<TownCentreIcon>(homeless, TownCentreIcon {.town = registry.Create(), .seed = SpellSeedType::Heal});

	EXPECT_EQ(_system.SiteOfIcon(icon), site);
	EXPECT_EQ(_system.SiteOfIcon(hand), site);
	EXPECT_TRUE(_system.SiteOfIcon(homeless) == entt::null);
	EXPECT_TRUE(_system.SiteOfIcon(site) == entt::null);
	EXPECT_TRUE(_system.SiteOfIcon(entt::null) == entt::null);
}

// The icons of the site lose their stores, but each icon continues to charge
TEST_F(SpellIconSystemTest, EmptyStoresOfASiteKeepTheChargesAndOtherSites)
{
	auto& registry = Entities();
	const auto site = registry.Create();
	registry.Assign<SiteIcons>(site);
	const auto heal = Icon(site, SpellSeedType::Heal, entt::null);
	const auto water = Icon(site, SpellSeedType::Water, entt::null);
	const auto other = registry.Create();
	registry.Assign<SiteIcons>(other);
	const auto otherIcon = Icon(other, SpellSeedType::Heal, entt::null);

	_system.EmptyIconStores(site);

	EXPECT_EQ(registry.Get<const WorshipIcon>(heal).store, 0.0f);
	EXPECT_EQ(registry.Get<const WorshipIcon>(water).store, 0.0f);
	EXPECT_TRUE(registry.Get<const WorshipIcon>(heal).charging);
	EXPECT_EQ(registry.Get<const WorshipIcon>(otherIcon).store, 50.0f);
}

namespace
{
/// A temple with a worship site in each of two places, the second place empty
struct TempleWithSites
{
	entt::entity temple {entt::null};
	entt::entity first {entt::null};
	entt::entity second {entt::null};
};

TempleWithSites MakeTemple(Registry& registry)
{
	TempleWithSites made;
	made.temple = registry.Create();
	auto& worship = registry.Assign<CitadelWorship>(made.temple);
	made.first = registry.Create();
	made.second = registry.Create();
	for (const auto site : {made.first, made.second})
	{
		registry.Assign<SiteIcons>(site);
		registry.Assign<WorshipSite>(site, WorshipSite {.temple = made.temple, .player = PlayerNames::PLAYER_ONE});
	}
	// The site in the first place comes first, whatever the order in which the sites were made
	worship.sites[0] = made.second;
	worship.sites[2] = made.first;
	return made;
}
} // namespace

// Only the charges for the hand that the script names stop, and only at the sites of the temple
TEST_F(SpellIconSystemTest, CancelChargesStopsOnlyTheChargesForTheHandAtTheTemplesSites)
{
	auto& registry = Entities();
	const auto made = MakeTemple(registry);
	const auto mine = Icon(made.first, SpellSeedType::Heal, entt::null);
	const auto god = Icon(made.second, SpellSeedType::Water, entt::null);
	registry.Get<WorshipIcon>(god).charger = PlayerNames::PLAYER_TWO;
	registry.Get<WorshipIcon>(mine).powerUp = 1;
	const auto elsewhere = registry.Create();
	registry.Assign<SiteIcons>(elsewhere);
	const auto away = Icon(elsewhere, SpellSeedType::Heal, entt::null);

	_system.CancelChargesFor(made.temple, PlayerNames::PLAYER_ONE);

	const auto& stopped = registry.Get<const WorshipIcon>(mine);
	EXPECT_FALSE(stopped.charging);
	EXPECT_EQ(stopped.powerUp, magic::k_BasePowerUpLevel);
	EXPECT_EQ(stopped.charger, PlayerNames::NEUTRAL);
	EXPECT_EQ(stopped.store, 0.0f);
	EXPECT_TRUE(registry.Get<const WorshipIcon>(god).charging);
	EXPECT_EQ(registry.Get<const WorshipIcon>(god).store, 50.0f);
	EXPECT_TRUE(registry.Get<const WorshipIcon>(away).charging);
	// A temple without sites, and no temple, stop nothing
	_system.CancelChargesFor(entt::null, PlayerNames::PLAYER_TWO);
	EXPECT_TRUE(registry.Get<const WorshipIcon>(god).charging);
}

// A scribble of the empty hand stops only the charge for the hand that started last, at the sites of the temple
TEST_F(SpellIconSystemTest, TheScribbleStopsTheChargeForTheHandThatStartedLast)
{
	auto& registry = Entities();
	const auto made = MakeTemple(registry);
	EXPECT_FALSE(_system.ChargesFor(made.temple, PlayerNames::PLAYER_ONE));
	const auto first = Icon(made.first, SpellSeedType::Heal, entt::null);
	const auto later = Icon(made.first, SpellSeedType::Water, entt::null);
	const auto same = Icon(made.second, SpellSeedType::Food, entt::null);
	const auto god = Icon(made.second, SpellSeedType::Wood, entt::null);
	registry.Get<WorshipIcon>(first).chargeStartTurn = 10;
	registry.Get<WorshipIcon>(later).chargeStartTurn = 30;
	// The second site comes first in the order of the places
	registry.Get<WorshipIcon>(same).chargeStartTurn = 30;
	registry.Get<WorshipIcon>(god).chargeStartTurn = 90;
	registry.Get<WorshipIcon>(god).charger = PlayerNames::PLAYER_TWO;

	EXPECT_TRUE(_system.ChargesFor(made.temple, PlayerNames::PLAYER_ONE));
	EXPECT_FALSE(_system.ChargesFor(made.temple, PlayerNames::PLAYER_THREE));
	EXPECT_TRUE(_system.CancelLatestChargeFor(made.temple, PlayerNames::PLAYER_ONE));
	EXPECT_FALSE(registry.Get<const WorshipIcon>(same).charging);
	EXPECT_TRUE(registry.Get<const WorshipIcon>(later).charging);
	EXPECT_TRUE(registry.Get<const WorshipIcon>(first).charging);
	EXPECT_TRUE(registry.Get<const WorshipIcon>(god).charging);

	EXPECT_TRUE(_system.CancelLatestChargeFor(made.temple, PlayerNames::PLAYER_ONE));
	EXPECT_FALSE(registry.Get<const WorshipIcon>(later).charging);
	EXPECT_TRUE(registry.Get<const WorshipIcon>(first).charging);

	// A charge that started on turn 0 counts as a charge, but the scribble does not stop it
	registry.Get<WorshipIcon>(first).chargeStartTurn = 0;
	EXPECT_TRUE(_system.ChargesFor(made.temple, PlayerNames::PLAYER_ONE));
	EXPECT_FALSE(_system.CancelLatestChargeFor(made.temple, PlayerNames::PLAYER_ONE));
	EXPECT_TRUE(registry.Get<const WorshipIcon>(first).charging);
	EXPECT_FALSE(_system.CancelLatestChargeFor(entt::null, PlayerNames::PLAYER_ONE));
}

// The icon of a seed comes from the first site in the order of the places round the temple
TEST_F(SpellIconSystemTest, TheIconInATempleComesFromTheFirstPlaceThatHasIt)
{
	auto& registry = Entities();
	const auto made = MakeTemple(registry);
	const auto inFirst = Icon(made.first, SpellSeedType::Wood, entt::null);
	const auto inSecond = Icon(made.second, SpellSeedType::Wood, entt::null);
	const auto heal = Icon(made.first, SpellSeedType::Heal, entt::null);

	EXPECT_EQ(_system.IconInTemple(made.temple, SpellSeedType::Wood), inSecond);
	EXPECT_EQ(_system.IconInTemple(made.temple, SpellSeedType::Heal), heal);
	EXPECT_TRUE(_system.IconInTemple(made.temple, SpellSeedType::Water) == entt::null);
	EXPECT_TRUE(_system.IconInTemple(entt::null, SpellSeedType::Wood) == entt::null);
	EXPECT_NE(inFirst, inSecond);
}

// A miracle charges for a script while an icon of the temple has prayer power in its store, whatever its charging flag
TEST_F(GodChargeTest, AnIconWithAStoreCountsAsAMiracleThatCharges)
{
	auto& registry = Entities();
	const auto made = MakeTemple(registry);
	const auto heal = Icon(made.first, SpellSeedType::Heal, entt::null);
	registry.Get<WorshipIcon>(heal).charging = false;

	EXPECT_TRUE(_system.AnyIconStores(made.temple, std::nullopt));
	EXPECT_TRUE(_system.AnyIconStores(made.temple, MagicType::Heal));
	EXPECT_FALSE(_system.AnyIconStores(made.temple, MagicType::Water));
	registry.Get<WorshipIcon>(heal).store = 0.0f;
	EXPECT_FALSE(_system.AnyIconStores(made.temple, std::nullopt));
	EXPECT_FALSE(_system.AnyIconStores(entt::null, std::nullopt));
}

// On the Gods' Playground the hand of a player taps a neutral icon, and the full icon's miracle goes into that hand
TEST_F(GodChargeTest, OnThePlaygroundAFullNeutralIconGivesItsMiracleToTheHandThatTapsIt)
{
	Locator::playgroundTripSystem::emplace<FakePlaygroundTrip>(true);
	const auto icon = IdleIcon(PlayerNames::NEUTRAL, 100.0f);
	// The hand that taps may cast the miracle too
	const auto tapper = Entities().Create();
	Entities().Assign<Player>(tapper, PlayerNames::PLAYER_ONE);
	player_miracles::SetMagicTypeEnabled(Entities().Get<Player>(tapper).miracles, MagicType::Heal, true);
	EXPECT_TRUE(_system.Tap(icon, PlayerNames::PLAYER_ONE));
	ASSERT_EQ(_magic->holders.size(), 1u);
	EXPECT_EQ(_magic->holders[0], PlayerNames::PLAYER_ONE);
	EXPECT_EQ(_magic->taken[0], Entities().Get<const WorshipIcon>(icon).site);
	Locator::playgroundTripSystem::reset();
}

// Away from the Playground the neutral icon takes no tap from the hand of a player
TEST_F(GodChargeTest, AwayFromThePlaygroundANeutralIconTakesNoTap)
{
	Locator::playgroundTripSystem::emplace<FakePlaygroundTrip>(false);
	const auto icon = IdleIcon(PlayerNames::NEUTRAL, 100.0f);
	EXPECT_FALSE(_system.Tap(icon, PlayerNames::PLAYER_ONE));
	EXPECT_TRUE(_magic->taken.empty());
	Locator::playgroundTripSystem::reset();
}
