/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The worship economy (docs/bw1-notes/magic.md): the site's capacity and battery, its per-turn accounting, the
// icons' chant store with the original's excess quirk, how many villagers a town needs, and the fireflies' weighted
// draw. No world is needed beyond a registry.

#define LOCATOR_IMPLEMENTATIONS

#include <cstdlib>

#include <memory>

#include <gtest/gtest.h>

#include "ECS/Components/Influence.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/SpellIcon.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/TownMagic.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/ObjectResources.h"
#include "ECS/Registry.h"
#include "Enums.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Worship/FireFlyReward.h"
#include "Worship/SpellDispenser.h"
#include "Worship/WorshipPercentage.h"
#include "Worship/WorshipSite.h"
#include "Worship/WorshipSpellIcon.h"
#include "support/TestServices.h"
#include "support/WorldSystems.h"

using namespace openblack;
using namespace openblack::ecs::components;

namespace
{
/// GWorshipSiteInfo of a Norse site as info.dat ships it. chantsToReserveForMaintaining is the int 500 read as a
/// float, which is what the original does.
GWorshipSiteInfo ShippedNorseSiteInfo()
{
	GWorshipSiteInfo info {};
	info.radiusFromCitadel = 37.5f;
	info.chantsPerVillager = 3.0f;
	info.prayerSiteDistance = 44.0f;
	info.maxDancersVisible = 20;
	info.chantsToFillBattery = 9000.0f;
	info.eachVillagerAddToFillBattery = 300.0f;
	info.chantsToReserveForMaintaining = 7.0e-43f;
	info.artifactPowerupMultiplier = 1e-5f;
	return info;
}

class WorshipTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		test::EmplaceMapAndVillagerDefaults();
		auto info = std::make_unique<InfoConstants>();
		info->worshipSite.at(static_cast<size_t>(Tribe::NORSE)) = ShippedNorseSiteInfo();
		info->villager.at(0).damageThresholdToGoHome = 0.3f;
		info->villager.at(0).chantLifeRate = 5e-6f;
		// the desire for food (the dancers' foodReqiredForDinner; the food pot's info holds FOOD)
		info->villager.at(0).foodReqiredForDinner = 4;
		info->pot.at(static_cast<size_t>(PotInfo::StoragePitFoodPile)).resourceType = ResourceType::Food;
		// the food pot takes all it is given (no cap: as the magic and loose piles)
		info->pot.at(static_cast<size_t>(PotInfo::StoragePitFoodPile)).nextPotForResource = static_cast<PotInfo>(19);
		// the FIRE seed and its rows: base magic FIREBALL, costToCreate 3500
		info->spellSeed.at(static_cast<size_t>(SpellSeedType::Fire)).magicTypes = {MagicType::Fireball, MagicType::None,
		                                                                           MagicType::None, MagicType::None};
		info->magicEffect.at(static_cast<size_t>(MagicType::Fireball)).costToCreate = 3500.0f;
		// the GMagicInfo rows live in per-class sections: types 0..9 are magicGeneral
		info->magicGeneral.at(static_cast<size_t>(MagicType::Fireball)).magicType = MagicType::Fireball;
		Locator::infoConstants::reset(info.release());
		Locator::entitiesRegistry::emplace<ecs::Registry>();
		openblack::test::EmplaceWorldSystems();
	}

	void TearDown() override
	{
		// the fireflies' reward probabilities as the program starts (all 0, and so their running sums): Reset clears
		// the probabilities only, a SetRewardProbability then sums them again
		worship::fire_fly::Reset();
		worship::fire_fly::SetRewardProbability(MagicType::Fireball, 0.0f);
		Locator::entitiesRegistry::reset();
		openblack::test::ResetWorldSystems();
		test::ResetMapAndVillagerDefaults();
		Locator::infoConstants::reset();
	}

	/// A bare site: only the fields the accounting reads (the real one comes from worship::site::Create)
	static WorshipSite NorseSite(int dancers)
	{
		WorshipSite site;
		site.infoIndex = static_cast<uint8_t>(Tribe::NORSE);
		site.tribe = Tribe::NORSE;
		site.player = PlayerNames::NEUTRAL;
		site.dancers.assign(static_cast<size_t>(dancers), entt::null);
		return site;
	}

	static entt::entity MakeTown(uint32_t id, float percentage, int villagers)
	{
		auto& registry = Locator::entitiesRegistry::value();
		const auto town = registry.Create();
		registry.Assign<Town>(town, id).owner = PlayerNames::PLAYER_ONE;
		registry.Assign<Transform>(town, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
		registry.Assign<TownInfluence>(town);
		auto& magic = registry.Assign<TownMagic>(town);
		magic.worshipPercentage = percentage;
		const auto site = registry.Create();
		registry.Assign<Transform>(site, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
		registry.Assign<WorshipSite>(site, NorseSite(0));
		magic.worshipSite = site;
		for (int i = 0; i < villagers; ++i)
		{
			const auto villager = registry.Create();
			auto& component = registry.Assign<Villager>(villager);
			component.town = town;
			registry.Assign<Transform>(villager, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
		}
		return town;
	}

	/// An icon with no site: its store is its own (worship::icon::AddToChantStore reads only the requirement)
	static entt::entity MakeIcon(SpellSeedType seed)
	{
		auto& registry = Locator::entitiesRegistry::value();
		const auto icon = registry.Create();
		auto& spellIcon = registry.Assign<SpellIcon>(icon);
		spellIcon.seedType = seed;
		spellIcon.player = PlayerNames::PLAYER_ONE;
		registry.Assign<WorshipSpellIcon>(icon);
		return icon;
	}
};
} // namespace

TEST_F(WorshipTest, CapacityAndBattery)
{
	// Capacity: N x chantsPerVillager x the player's tribal power (1 by default). MaxBattery: 9000 + N x 300.
	const auto empty = NorseSite(0);
	EXPECT_FLOAT_EQ(worship::site::Capacity(empty), 0.0f);
	EXPECT_FLOAT_EQ(worship::site::MaxBattery(empty), 9000.0f);
	const auto full = NorseSite(11);
	EXPECT_FLOAT_EQ(worship::site::Capacity(full), 33.0f);
	EXPECT_FLOAT_EQ(worship::site::MaxBattery(full), 12300.0f);
	EXPECT_EQ(worship::site::DancerCount(full), 11);
}

TEST_F(WorshipTest, ReserveForMaintainingIsTheInfoDatBug)
{
	// the file holds the int 500 and the original reads it as a float: the reserve is ~0, so seeds out change nothing
	auto site = NorseSite(0);
	site.available = 1000.0f;
	site.used = 0.0f;
	EXPECT_FLOAT_EQ(worship::site::Available(site), 1000.0f);
	EXPECT_FLOAT_EQ(worship::site::AvailableForIcons(site, false), 1000.0f);
	EXPECT_NEAR(worship::site::AvailableForIcons(site, true), 1000.0f, 1e-3f);
}

TEST_F(WorshipTest, InfiniteChantsCheat)
{
	auto site = NorseSite(0);
	site.infiniteChants = true;
	EXPECT_FLOAT_EQ(worship::site::Available(site), 1e6f);
	EXPECT_FLOAT_EQ(worship::site::TotalChantsAvailable(site), 1e6f);
}

TEST_F(WorshipTest, UseChantsTakesAtMostWhatIsAvailable)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto entity = registry.Create();
	auto site = NorseSite(0);
	site.available = 100.0f;
	registry.Assign<WorshipSite>(entity, site);
	EXPECT_FLOAT_EQ(worship::site::UseChants(entity, 40.0f), 40.0f);
	EXPECT_FLOAT_EQ(worship::site::UseChants(entity, 100.0f), 60.0f); // clamped
	EXPECT_FLOAT_EQ(registry.Get<const WorshipSite>(entity).used, 100.0f);
	EXPECT_FLOAT_EQ(registry.Get<const WorshipSite>(entity).requested, 140.0f);
	EXPECT_FLOAT_EQ(worship::site::UseChants(entity, -5.0f), 0.0f);
}

TEST_F(WorshipTest, EndOfTurnDanceIntensityAndBattery)
{
	// ProcessSpellIcons with 11 dancers, an empty battery and nothing used: the boost is 0.5, so k = 0.5 and the
	// battery gains capacity x k
	auto& registry = Locator::entitiesRegistry::value();
	const auto entity = registry.Create();
	registry.Assign<Transform>(entity, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	registry.Assign<WorshipSite>(entity, NorseSite(11));
	worship::site::ProcessSpellIcons(entity);
	const auto& site = registry.Get<const WorshipSite>(entity);
	EXPECT_FLOAT_EQ(site.danceSpeed, 0.5f);
	EXPECT_FLOAT_EQ(site.battery, 16.5f); // 33 x 0.5
	EXPECT_FLOAT_EQ(site.chantDamage, 16.5f / 11.0f);
	EXPECT_FLOAT_EQ(site.available, 16.5f + 33.0f);
	EXPECT_FLOAT_EQ(site.strain, -1.0f); // nothing asked and the dancers make 33: (0 - 33) / 33
	EXPECT_EQ(site.danceState, 1);       // k > 0 starts the dance
}

TEST_F(WorshipTest, StrainIsDemandOverCapacity)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto entity = registry.Create();
	registry.Assign<Transform>(entity, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	auto site = NorseSite(10); // capacity 30
	site.requested = 90.0f;    // three times what the dancers make
	site.available = 90.0f;
	site.used = 90.0f;
	registry.Assign<WorshipSite>(entity, site);
	worship::site::ProcessSpellIcons(entity);
	// (90 - 30) / 30 = 2; with the strain positive the charging icons get nothing
	EXPECT_FLOAT_EQ(registry.Get<const WorshipSite>(entity).strain, 2.0f);
}

TEST_F(WorshipTest, ChantStoreKeepsTheExcessQuirk)
{
	// AddToChantStore with no site: below the requirement it returns what it took, above it
	// returns the excess and the store stops at the requirement
	const auto icon = MakeIcon(SpellSeedType::Fire);
	const float required = worship::icon::GetChantRequired(icon);
	ASSERT_FLOAT_EQ(required, 3500.0f);
	EXPECT_FLOAT_EQ(worship::icon::AddToChantStore(icon, required * 0.25f), required * 0.25f);
	EXPECT_FLOAT_EQ(worship::icon::GetChantNeeded(icon), required * 0.75f);
	const float excess = worship::icon::AddToChantStore(icon, required);
	EXPECT_FLOAT_EQ(excess, required - required * 0.75f);
	EXPECT_FLOAT_EQ(worship::icon::GetChantNeeded(icon), 0.0f);
	EXPECT_FLOAT_EQ(worship::icon::ChargeFraction(icon), 1.0f);
	// RemoveFromChantStore takes at most the store
	EXPECT_FLOAT_EQ(worship::icon::RemoveFromChantStore(icon, required * 2.0f), required);
	EXPECT_FLOAT_EQ(worship::icon::ChargeFraction(icon), 0.0f);
}

TEST_F(WorshipTest, WorshipersNeeded)
{
	// GetWorshipersNeeded: target = max(1, int(pop x pct + 0.5)), result = target - current + go-homes
	const auto town = MakeTown(1, 0.5f, 22);
	bool reachable = false;
	EXPECT_EQ(worship::percentage::GetWorshipersNeeded(town, true, true, &reachable), 11);
	EXPECT_FALSE(reachable); // result > 0 but nobody is there yet
	worship::percentage::AddWorshipper(town);
	EXPECT_EQ(worship::percentage::GetWorshipersNeeded(town, true, true, nullptr), 10);
	// a percentage that rounds to 0 still asks for one villager
	auto& magic = Locator::entitiesRegistry::value().Get<TownMagic>(town);
	magic.worshipPercentage = 0.01f;
	EXPECT_EQ(worship::percentage::GetWorshipersNeeded(town, true, true, nullptr), 0); // max(1, 0) - 1
	// no percentage, no target
	magic.worshipPercentage = 0.0f;
	EXPECT_EQ(worship::percentage::GetWorshipersNeeded(town, true, true, nullptr), -1);
}

TEST_F(WorshipTest, WorshipScoreFallsOffWithTheDistance)
{
	// WorshipScore uses GetDistanceModifier(d1, d2), which is SigmoidThreshold(0.5, 1 - min / d2) with the threshold
	// as its FIRST argument. openblack used to pass them the other way round, which mirrored the curve and sent the
	// farthest villagers first; it is the nearest who go. d1 is the villager's distance to the site's centre (the
	// site + (12.55, 0, -26.1) here, as the site is at the origin unrotated) and d2 the town's + 100, so about 128.9 m
	const auto town = MakeTown(1, 0.5f, 0);
	auto& registry = Locator::entitiesRegistry::value();
	const auto villager = [&](glm::vec3 position, float life) {
		const auto entity = registry.Create();
		auto& component = registry.Assign<Villager>(entity);
		component.town = town;
		component.life = life;
		registry.Assign<Transform>(entity, position, glm::mat3(1.0f), glm::vec3(1.0f));
		return entity;
	};
	const auto atCentre = villager(glm::vec3(12.55f, 0.0f, -26.1f), 0.5f);
	const auto farAway = villager(glm::vec3(1000.0f, 0.0f, 1000.0f), 1.0f);
	// at the centre the modifier is k_Sigmoid[30] = 0.99996 and the life goes in THREE times, not twice: 0.5^3, not
	// 0.5^2. The mirrored curve would give k_Sigmoid[10] = 3.6e-5 here
	EXPECT_FLOAT_EQ(worship::percentage::WorshipScore(atCentre), 0.5f * 0.5f * 0.5f * 0.999963939f);
	// beyond d2 the modifier is k_Sigmoid[10]: a full-life villager far away scores below a half-life one at the centre
	EXPECT_FLOAT_EQ(worship::percentage::WorshipScore(farAway), 3.60351005e-5f);
	EXPECT_GT(worship::percentage::WorshipScore(atCentre), worship::percentage::WorshipScore(farAway));
}

TEST(SpellDispenserTurn, CountsToThePeriodWhileItsOrbIsGone)
{
	using worship::dispenser::StepTurn;
	using worship::dispenser::TurnStep;
	SpellDispenser dispenser;
	dispenser.period = 3;
	// inactive, or without a magic: nothing counts
	EXPECT_EQ(StepTurn(dispenser, false), TurnStep::Idle);
	dispenser.active = true;
	EXPECT_EQ(StepTurn(dispenser, false), TurnStep::Idle);
	EXPECT_EQ(dispenser.tick, 0u);
	dispenser.magicType = MagicType::Wood;
	EXPECT_EQ(StepTurn(dispenser, false), TurnStep::Count);
	EXPECT_EQ(StepTurn(dispenser, false), TurnStep::Count);
	EXPECT_EQ(StepTurn(dispenser, false), TurnStep::MakeOrb);
	EXPECT_EQ(dispenser.tick, 3u);
	// with no orb made the tick goes on, and every turn asks again
	EXPECT_EQ(StepTurn(dispenser, false), TurnStep::MakeOrb);
	EXPECT_EQ(dispenser.tick, 4u);
	// an orb made (the tick back to 0): it waits while the orb is on it
	dispenser.oneShot = static_cast<entt::entity>(5);
	dispenser.tick = 0;
	EXPECT_EQ(StepTurn(dispenser, true), TurnStep::Wait);
	EXPECT_EQ(StepTurn(dispenser, true), TurnStep::Wait);
	EXPECT_EQ(dispenser.tick, 0u);
	// taken: that turn only forgets it, the count starts on the next
	dispenser.tick = 2;
	EXPECT_EQ(StepTurn(dispenser, false), TurnStep::OrbGone);
	EXPECT_TRUE(dispenser.oneShot == entt::null);
	EXPECT_EQ(dispenser.tick, 0u);
	EXPECT_EQ(StepTurn(dispenser, false), TurnStep::Count);
	EXPECT_EQ(StepTurn(dispenser, false), TurnStep::Count);
	EXPECT_EQ(StepTurn(dispenser, false), TurnStep::MakeOrb);
	// inactive again: its orb is still forgotten when taken, but nothing counts
	dispenser.oneShot = static_cast<entt::entity>(6);
	dispenser.active = false;
	EXPECT_EQ(StepTurn(dispenser, false), TurnStep::OrbGone);
	EXPECT_EQ(StepTurn(dispenser, false), TurnStep::Idle);
}

TEST_F(WorshipTest, FireFlyRewardProbabilities)
{
	// SetRewardProbability keeps running sums; Land1.txt gives HEAL 20 and six miracles 1 each
	worship::fire_fly::Reset();
	EXPECT_FLOAT_EQ(worship::fire_fly::Total(), 0.0f);
	worship::fire_fly::SetRewardProbability(MagicType::Fireball, 1.0f);
	worship::fire_fly::SetRewardProbability(MagicType::Heal, 20.0f);
	worship::fire_fly::SetRewardProbability(MagicType::Food, 1.0f);
	worship::fire_fly::SetRewardProbability(MagicType::Wood, 1.0f);
	worship::fire_fly::SetRewardProbability(MagicType::Water, 1.0f);
	worship::fire_fly::SetRewardProbability(MagicType::Forest, 1.0f);
	worship::fire_fly::SetRewardProbability(MagicType::LightningBolt, 1.0f);
	EXPECT_FLOAT_EQ(worship::fire_fly::Total(), 26.0f);
	// Reset (on clearing the map) zeroes the probabilities but not the running sums: the total of the land before
	// stays until the next FIRE_FLY_SPELL_REWARD_PROB (the original's quirk, kept)
	worship::fire_fly::Reset();
	EXPECT_FLOAT_EQ(worship::fire_fly::Total(), 26.0f);
	worship::fire_fly::SetRewardProbability(MagicType::Heal, 5.0f);
	EXPECT_FLOAT_EQ(worship::fire_fly::Total(), 5.0f);
}

TEST_F(WorshipTest, DesireForFoodOfASite)
{
	// CalculateDesireForFood: 1 - min((food + 1e-4) / (needed + 1e-4), 1); needed is CalculateFoodNeededByDancers,
	// the sum of (1 - food) x foodReqiredForDinner
	auto& registry = Locator::entitiesRegistry::value();
	const auto siteEntity = registry.Create();
	registry.Assign<WorshipSite>(siteEntity, NorseSite(0));
	EXPECT_EQ(worship::site::DancerCount(siteEntity), 0);
	// no dancers and no pot: 1 - min(1e-4 / 1e-4, 1) = 0
	EXPECT_FLOAT_EQ(worship::site::CalculateDesireForFood(siteEntity), 0.0f);
	for (const float food : {0.5f, 0.75f})
	{
		const auto villager = registry.Create();
		registry.Assign<Villager>(villager).food = food;
		registry.Get<WorshipSite>(siteEntity).dancers.push_back(villager);
	}
	EXPECT_EQ(worship::site::DancerCount(siteEntity), 2);
	// (1 - 0.5) x 4 + (1 - 0.75) x 4 = 3
	EXPECT_FLOAT_EQ(worship::site::CalculateFoodNeededByDancers(siteEntity), 3.0f);
	// still no pot: 1 - 1e-4 / 3.0001
	EXPECT_FLOAT_EQ(worship::site::CalculateDesireForFood(siteEntity), 1.0f - 0.0001f / 3.0001f);
	const auto pot = registry.Create();
	registry.Assign<Pot>(pot, static_cast<uint16_t>(1), static_cast<uint16_t>(100), PotInfo::StoragePitFoodPile);
	registry.Get<WorshipSite>(siteEntity).foodPot = pot;
	EXPECT_EQ(worship::site::GetFoodResource(siteEntity), 1u);
	EXPECT_FLOAT_EQ(worship::site::CalculateDesireForFood(siteEntity), 1.0f - 1.0001f / 3.0001f);
	// enough food: the ratio is held at 1, no desire
	registry.Get<Pot>(pot).amount = 50;
	EXPECT_FLOAT_EQ(worship::site::CalculateDesireForFood(siteEntity), 0.0f);
	// not a site: nothing
	EXPECT_FLOAT_EQ(worship::site::CalculateDesireForFood(pot), 0.0f);
}

TEST(WorshipSiteAddResource, FoodGoesToThePotWoodOnlyToABuildingSite)
{
	using worship::site::AddResourceRoute;
	using worship::site::AddResourceRouteOf;
	EXPECT_EQ(AddResourceRouteOf(ResourceType::Food, false), AddResourceRoute::FoodPot);
	EXPECT_EQ(AddResourceRouteOf(ResourceType::Food, true), AddResourceRoute::FoodPot);
	EXPECT_EQ(AddResourceRouteOf(ResourceType::Wood, true), AddResourceRoute::BuildingSite);
	EXPECT_EQ(AddResourceRouteOf(ResourceType::Any, true), AddResourceRoute::BuildingSite);
	EXPECT_EQ(AddResourceRouteOf(ResourceType::Wood, false), AddResourceRoute::Nothing);
	EXPECT_EQ(AddResourceRouteOf(ResourceType::Any, false), AddResourceRoute::Nothing); // ANY is not FOOD
	EXPECT_EQ(AddResourceRouteOf(ResourceType::None, false), AddResourceRoute::Nothing);
}

TEST_F(WorshipTest, FoodGivenToASiteFillsItsFoodPot)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto siteEntity = registry.Create();
	registry.Assign<WorshipSite>(siteEntity, NorseSite(0));
	const auto pot = registry.Create();
	registry.Assign<Pot>(pot, static_cast<uint16_t>(10), static_cast<uint16_t>(100), PotInfo::StoragePitFoodPile);
	registry.Assign<Transform>(pot, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	registry.Get<WorshipSite>(siteEntity).foodPot = pot;
	EXPECT_EQ(worship::site::AddResource(siteEntity, ResourceType::Food, 300, true), 300u);
	EXPECT_EQ(worship::site::GetFoodResource(siteEntity), 310u);
	EXPECT_TRUE(registry.Get<Pot>(pot).poisoned);
	// through the objects' AddResource too, as a store's delivery asks it
	EXPECT_EQ(ecs::object_resources::AddResource(siteEntity, ResourceType::Food, 40), 40u);
	EXPECT_EQ(worship::site::GetFoodResource(siteEntity), 350u);
	// wood with no building site, or not a site: nothing
	EXPECT_EQ(worship::site::AddResource(siteEntity, ResourceType::Wood, 50), 0u);
	EXPECT_EQ(worship::site::AddResource(pot, ResourceType::Food, 50), 0u);
	EXPECT_EQ(worship::site::GetFoodResource(siteEntity), 350u);
}
