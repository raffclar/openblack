/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <memory>
#include <vector>

#include <gtest/gtest.h>

#include "Debug/LeftClickCapture.h"
#include "Debug/MiraclesModel.h"

using namespace openblack;
using namespace openblack::debug::miracles;

namespace
{
/// Zeroed tables where each magic record names the magic type it sits at, as the file does
std::unique_ptr<InfoConstants> MakeTables()
{
	auto info = std::make_unique<InfoConstants>();
	uint32_t type = 0;
	const auto number = [&type](auto& records) {
		for (auto& record : records)
		{
			record.magicType = static_cast<MagicType>(type++);
		}
	};
	number(info->magicGeneral);
	number(info->magicHeal);
	number(info->magicTeleport);
	number(info->magicForest);
	number(info->magicFood);
	number(info->magicStormAndTornado);
	number(info->magicShield);
	number(info->magicWood);
	number(info->magicWater);
	number(info->magicFlockFlying);
	number(info->magicFlockGround);
	number(info->magicCreatureSpell);
	return info;
}

GSpellSeedInfo& Seed(InfoConstants& info, SpellSeedType seed)
{
	return info.spellSeed.at(static_cast<size_t>(seed));
}

GMagicEffectInfo& Effect(InfoConstants& info, MagicType type)
{
	return info.magicEffect.at(static_cast<size_t>(type));
}

/// A fire seed with a fireball and its two power-ups, and a shield seed with only its plain miracle
std::unique_ptr<InfoConstants> MakeSeeds()
{
	auto info = MakeTables();
	Seed(*info, SpellSeedType::Fire).magicTypes = {MagicType::Fireball, MagicType::FireballPowerUpOne,
	                                               MagicType::FireballPowerUpTwo, MagicType::None};
	Seed(*info, SpellSeedType::Shield).magicTypes = {MagicType::Shield, MagicType::None, MagicType::None, MagicType::None};
	return info;
}

/// Records what it is asked to cast, and starts a spell unless told it fails
class FakeCaster final: public CasterInterface
{
public:
	entt::entity CastAtPoint(const CastPlan& plan, PlayerNames player, glm::vec3 point) override
	{
		casts.push_back({.type = plan.type, .player = player, .point = point, .target = entt::null});
		return fails ? entt::null : k_Spell;
	}
	entt::entity CastOnObject(const CastPlan& plan, PlayerNames player, entt::entity target) override
	{
		casts.push_back({.type = plan.type, .player = player, .point = glm::vec3(0.0f), .target = target});
		return fails ? entt::null : k_Spell;
	}
	[[nodiscard]] bool CanCastAt(MagicType, PlayerNames, glm::vec3) const override { return true; }

	struct Asked
	{
		MagicType type;
		PlayerNames player;
		glm::vec3 point;
		entt::entity target;
	};
	static constexpr auto k_Spell = static_cast<entt::entity>(42);
	std::vector<Asked> casts;
	bool fails {false};
};

/// Records what it is asked to make, and makes it unless told it fails
class FakeDispenserCreator final: public DispenserCreatorInterface
{
public:
	entt::entity CreateOneShot(SpellSeedType seed, int powerUpLevel, glm::vec3 point) override
	{
		made.push_back({.kind = DispenserKind::OneShot, .seed = seed, .powerUpLevel = powerUpLevel, .point = point});
		return fails ? entt::null : k_Made;
	}
	entt::entity CreateOneShotInHand(SpellSeedType seed, int powerUpLevel, PlayerNames player) override
	{
		made.push_back({.kind = DispenserKind::OneShotInHand, .seed = seed, .powerUpLevel = powerUpLevel, .player = player});
		return fails ? entt::null : k_Made;
	}
	entt::entity CreatePermanent(AbodeInfo abode, MagicType magic, uint32_t periodTurns, glm::vec3 point) override
	{
		made.push_back(
		    {.kind = DispenserKind::Permanent, .magic = magic, .abode = abode, .periodTurns = periodTurns, .point = point});
		return fails ? entt::null : k_Made;
	}

	struct Asked
	{
		DispenserKind kind {DispenserKind::OneShot};
		SpellSeedType seed {SpellSeedType::None};
		int powerUpLevel {k_BasePowerUpLevel};
		MagicType magic {MagicType::None};
		AbodeInfo abode {AbodeInfo::_COUNT};
		uint32_t periodTurns {0};
		PlayerNames player {PlayerNames::NEUTRAL};
		glm::vec3 point {0.0f};
	};
	static constexpr auto k_Made = static_cast<entt::entity>(43);
	std::vector<Asked> made;
	bool fails {false};
};

ecs::effects::reactions::Reaction MakeReaction(uint32_t id, entt::entity initiator, Reaction type)
{
	ecs::effects::reactions::Reaction reaction;
	reaction.id = id;
	reaction.initiator = initiator;
	reaction.type = type;
	reaction.player = PlayerNames::PLAYER_ONE;
	reaction.radius = 30.0f;
	return reaction;
}
} // namespace

TEST(MiraclesMenu, castableSeedsAreThoseWithAPlainMiracle)
{
	const auto info = MakeSeeds();
	const std::vector<SpellSeedType> expected {SpellSeedType::Fire, SpellSeedType::Shield};
	EXPECT_EQ(CastableSeeds(*info), expected);
}

TEST(MiraclesMenu, noSeedCastsInEmptyTables)
{
	const auto info = MakeTables();
	EXPECT_TRUE(CastableSeeds(*info).empty());
}

TEST(MiraclesMenu, powerUpLevelsFromThePlainMiracleUp)
{
	const auto info = MakeSeeds();
	const std::vector<int> fire {k_BasePowerUpLevel, 0, 1};
	EXPECT_EQ(PowerUpLevels(Seed(*info, SpellSeedType::Fire)), fire);
	const std::vector<int> shield {k_BasePowerUpLevel};
	EXPECT_EQ(PowerUpLevels(Seed(*info, SpellSeedType::Shield)), shield);
}

TEST(MiraclesMenu, aLevelTheNewSeedLacksFallsBackToPlain)
{
	const auto info = MakeSeeds();
	EXPECT_EQ(KeepLevel(Seed(*info, SpellSeedType::Fire), 1), 1);
	EXPECT_EQ(KeepLevel(Seed(*info, SpellSeedType::Shield), 1), k_BasePowerUpLevel);
	EXPECT_EQ(KeepLevel(Seed(*info, SpellSeedType::Shield), k_BasePowerUpLevel), k_BasePowerUpLevel);
}

TEST(MiraclesMenu, theChoiceCastsItsLevelsMiracle)
{
	const auto info = MakeSeeds();
	EXPECT_EQ(MagicTypeOf(*info, {.seed = SpellSeedType::Fire, .powerUpLevel = k_BasePowerUpLevel}), MagicType::Fireball);
	EXPECT_EQ(MagicTypeOf(*info, {.seed = SpellSeedType::Fire, .powerUpLevel = 1}), MagicType::FireballPowerUpTwo);
	EXPECT_EQ(MagicTypeOf(*info, {}), MagicType::None);
}

TEST(MiraclesMenu, levelNames)
{
	EXPECT_EQ(LevelName(k_BasePowerUpLevel), "plain");
	EXPECT_EQ(LevelName(0), "power-up 1");
	EXPECT_EQ(LevelName(2), "power-up 3");
}

TEST(MiraclesMenu, aCastIsThrownDownOntoItsPointAtItsCharge)
{
	auto info = MakeSeeds();
	Effect(*info, MagicType::Fireball).initialChants = 1000.0f;
	Effect(*info, MagicType::Fireball).timerWhenPlayerCasting = 8.0f;
	const glm::vec3 point(100.0f, 5.0f, 200.0f);
	const glm::vec3 forward(0.0f, 0.0f, 1.0f);
	const auto plan = PlanCast(*info, {.seed = SpellSeedType::Fire}, 0.5f, point, forward);
	EXPECT_EQ(plan.type, MagicType::Fireball);
	EXPECT_FLOAT_EQ(plan.cast.chants, 500.0f);
	EXPECT_FLOAT_EQ(plan.cast.duration, 4.0f);
	EXPECT_FLOAT_EQ(plan.cast.magnitude, 1.0f);
	EXPECT_EQ(plan.cast.maxObjectsToCreate, -1);
	EXPECT_EQ(plan.process.handPos, point + glm::vec3(0.0f, k_CastHeight, 0.0f));
	EXPECT_EQ(plan.process.direction, glm::vec3(0.0f, -k_CastThrowSpeed, 0.0f));
	EXPECT_EQ(plan.process.cameraForward, forward);
}

TEST(MiraclesMenu, aRadiusMiracleIsCastAtItsNormalCostRadius)
{
	auto info = MakeSeeds();
	info->magicShield.at(0).radiusForNormalCost = 30.0f;
	const auto plan = PlanCast(*info, {.seed = SpellSeedType::Shield}, 1.0f, glm::vec3(0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
	EXPECT_EQ(plan.type, MagicType::Shield);
	EXPECT_FLOAT_EQ(plan.cast.magnitude, 30.0f);
}

TEST(MiraclesMenu, noSeedPlansNoCast)
{
	const auto info = MakeSeeds();
	EXPECT_EQ(PlanCast(*info, {}, 1.0f, glm::vec3(0.0f), glm::vec3(0.0f)).type, MagicType::None);
}

TEST(MiraclesMenu, aCastGoesToThePointOrTheThingAsThePlayer)
{
	const auto info = MakeSeeds();
	const auto plan = PlanCast(*info, {.seed = SpellSeedType::Fire}, 1.0f, glm::vec3(0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
	FakeCaster caster;
	const glm::vec3 point(10.0f, 0.0f, 20.0f);
	const auto atPoint = Cast(caster, plan, "fireball", PlayerNames::PLAYER_TWO, point);
	EXPECT_EQ(atPoint.spell, FakeCaster::k_Spell);
	EXPECT_EQ(atPoint.message, "Cast fireball at (10, 20)");
	const auto thing = static_cast<entt::entity>(7);
	const auto onThing = Cast(caster, plan, "fireball", PlayerNames::NEUTRAL, thing);
	EXPECT_EQ(onThing.message, "Cast fireball on thing 7");

	ASSERT_EQ(caster.casts.size(), 2);
	EXPECT_EQ(caster.casts[0].type, MagicType::Fireball);
	EXPECT_EQ(caster.casts[0].player, PlayerNames::PLAYER_TWO);
	EXPECT_EQ(caster.casts[0].point, point);
	EXPECT_EQ(caster.casts[1].player, PlayerNames::NEUTRAL);
	EXPECT_EQ(caster.casts[1].target, thing);
}

TEST(MiraclesMenu, aCastThatCannotStartSaysSo)
{
	const auto info = MakeSeeds();
	const auto plan = PlanCast(*info, {.seed = SpellSeedType::Fire}, 1.0f, glm::vec3(0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
	FakeCaster caster;
	caster.fails = true;
	const auto result = Cast(caster, plan, "fireball", PlayerNames::PLAYER_ONE, glm::vec3(0.0f));
	EXPECT_TRUE(result.spell == entt::null);
	EXPECT_EQ(result.message, "fireball could not start");
}

TEST(MiraclesMenu, noMiracleIsNotCast)
{
	FakeCaster caster;
	const auto result = Cast(caster, CastPlan {}, "none", PlayerNames::PLAYER_ONE, glm::vec3(0.0f));
	EXPECT_TRUE(result.spell == entt::null);
	EXPECT_TRUE(caster.casts.empty());
}

TEST(MiraclesMenu, aOneShotOrbIsTheChosenSeedAtItsLevelAtThePoint)
{
	const auto info = MakeSeeds();
	FakeDispenserCreator creator;
	const glm::vec3 point(10.0f, 4.0f, 20.0f);
	const DispenserPlan plan {.kind = DispenserKind::OneShot, .choice = {.seed = SpellSeedType::Fire, .powerUpLevel = 1}};
	const auto result = CreateDispenser(creator, *info, plan, "fireball", PlayerNames::PLAYER_ONE, point);
	EXPECT_EQ(result.spell, FakeDispenserCreator::k_Made);
	EXPECT_EQ(result.message, "A one-shot fireball at (10, 20)");
	ASSERT_EQ(creator.made.size(), 1);
	EXPECT_EQ(creator.made[0].kind, DispenserKind::OneShot);
	EXPECT_EQ(creator.made[0].seed, SpellSeedType::Fire);
	EXPECT_EQ(creator.made[0].powerUpLevel, 1);
	EXPECT_EQ(creator.made[0].point, point);
}

TEST(MiraclesMenu, aOneShotInTheHandGoesToThePlayersHand)
{
	const auto info = MakeSeeds();
	FakeDispenserCreator creator;
	const DispenserPlan plan {.kind = DispenserKind::OneShotInHand, .choice = {.seed = SpellSeedType::Shield}};
	const auto result = CreateDispenser(creator, *info, plan, "shield", PlayerNames::PLAYER_TWO, glm::vec3(5.0f));
	EXPECT_EQ(result.message, "A one-shot shield in the hand");
	ASSERT_EQ(creator.made.size(), 1);
	EXPECT_EQ(creator.made[0].kind, DispenserKind::OneShotInHand);
	EXPECT_EQ(creator.made[0].seed, SpellSeedType::Shield);
	EXPECT_EQ(creator.made[0].powerUpLevel, k_BasePowerUpLevel);
	EXPECT_EQ(creator.made[0].player, PlayerNames::PLAYER_TWO);
}

TEST(MiraclesMenu, aPermanentDispenserGivesTheChoicesMiracleItsBuildingAndPeriod)
{
	const auto info = MakeSeeds();
	FakeDispenserCreator creator;
	const glm::vec3 point(30.0f, 2.0f, 40.0f);
	const DispenserPlan plan {.kind = DispenserKind::Permanent,
	                          .choice = {.seed = SpellSeedType::Fire, .powerUpLevel = 0},
	                          .abode = AbodeInfo::GreekSpellDispenser,
	                          .periodTurns = 300};
	const auto result = CreateDispenser(creator, *info, plan, "fireball", PlayerNames::PLAYER_ONE, point);
	EXPECT_EQ(result.message, "A fireball dispenser at (30, 40), every 300 turns");
	ASSERT_EQ(creator.made.size(), 1);
	EXPECT_EQ(creator.made[0].kind, DispenserKind::Permanent);
	EXPECT_EQ(creator.made[0].magic, MagicType::FireballPowerUpOne);
	EXPECT_EQ(creator.made[0].abode, AbodeInfo::GreekSpellDispenser);
	EXPECT_EQ(creator.made[0].periodTurns, 300);
	EXPECT_EQ(creator.made[0].point, point);
}

TEST(MiraclesMenu, aDispenserThatCannotBeMadeSaysSo)
{
	const auto info = MakeSeeds();
	FakeDispenserCreator creator;
	creator.fails = true;
	const DispenserPlan plan {.kind = DispenserKind::OneShotInHand, .choice = {.seed = SpellSeedType::Fire}};
	const auto result = CreateDispenser(creator, *info, plan, "fireball", PlayerNames::PLAYER_ONE, glm::vec3(0.0f));
	EXPECT_TRUE(result.spell == entt::null);
	EXPECT_EQ(result.message, "The fireball could not be made");
}

TEST(MiraclesMenu, noMiracleMakesNoDispenser)
{
	const auto info = MakeSeeds();
	FakeDispenserCreator creator;
	for (const auto kind : {DispenserKind::OneShot, DispenserKind::OneShotInHand, DispenserKind::Permanent})
	{
		const auto result = CreateDispenser(creator, *info, {.kind = kind}, "none", PlayerNames::PLAYER_ONE, glm::vec3(0.0f));
		EXPECT_TRUE(result.spell == entt::null);
		EXPECT_EQ(result.message, "No miracle chosen");
	}
	EXPECT_TRUE(creator.made.empty());
}

TEST(MiraclesMenu, theDispenserBuildingsAreThoseOfTheirTypeNorseFirst)
{
	auto info = MakeTables();
	EXPECT_TRUE(DispenserAbodes(*info).empty());
	EXPECT_FALSE(DefaultDispenserAbode({}).has_value());

	info->abode.at(static_cast<size_t>(AbodeInfo::CelticSpellDispenser)).abodeType = AbodeType::SpellDispenser;
	info->abode.at(static_cast<size_t>(AbodeInfo::NorseSpellDispenser)).abodeType = AbodeType::SpellDispenser;
	info->abode.at(static_cast<size_t>(AbodeInfo::NorseSpellDispenser)).timeEachMobileObjectTakesToProduce = 300.0f;
	const std::vector<AbodeInfo> expected {AbodeInfo::CelticSpellDispenser, AbodeInfo::NorseSpellDispenser};
	const auto abodes = DispenserAbodes(*info);
	EXPECT_EQ(abodes, expected);
	EXPECT_EQ(DefaultDispenserAbode(abodes), AbodeInfo::NorseSpellDispenser);
	const std::vector<AbodeInfo> celtic {AbodeInfo::CelticSpellDispenser};
	EXPECT_EQ(DefaultDispenserAbode(celtic), AbodeInfo::CelticSpellDispenser);
	EXPECT_EQ(DefaultPeriod(*info, AbodeInfo::NorseSpellDispenser), 300);
	EXPECT_EQ(DefaultPeriod(*info, AbodeInfo::_COUNT), 0);
}

TEST(MiraclesMenu, theHandIsTheRightHandElseTheLeft)
{
	const glm::vec3 left(1.0f, 2.0f, 3.0f);
	const glm::vec3 right(4.0f, 5.0f, 6.0f);
	EXPECT_EQ(HandPoint(left, right), right);
	EXPECT_EQ(HandPoint(left, std::nullopt), left);
	EXPECT_FALSE(HandPoint(std::nullopt, std::nullopt).has_value());
}

TEST(MiraclesMenu, theNearestThingAcrossTheLandWithinReach)
{
	const std::vector<ThingAt> things {
	    {.entity = static_cast<entt::entity>(1), .position = {8.0f, 0.0f, 0.0f}},
	    {.entity = static_cast<entt::entity>(2), .position = {0.0f, 50.0f, 3.0f}},
	    {.entity = static_cast<entt::entity>(3), .position = {0.0f, 0.0f, -3.0f}},
	};
	// The height doesn't count; on a tie the first one
	EXPECT_EQ(NearestThing(things, glm::vec3(0.0f)), static_cast<entt::entity>(2));
	EXPECT_EQ(NearestThing(things, glm::vec3(7.0f, 0.0f, 0.0f)), static_cast<entt::entity>(1));
	EXPECT_FALSE(NearestThing(things, glm::vec3(100.0f, 0.0f, 0.0f)).has_value());
	EXPECT_FALSE(NearestThing({}, glm::vec3(0.0f)).has_value());
}

TEST(MiraclesMenu, infinitePrayerDoesNothingWhileOff)
{
	InfinitePrayer prayer;
	PrayerCheats cheats;
	prayer.Apply(static_cast<entt::entity>(1), cheats);
	EXPECT_EQ(cheats, PrayerCheats {});
	EXPECT_EQ(prayer.HeldSites(), 0);
}

TEST(MiraclesMenu, infinitePrayerGivesEverySiteBackWhatItHad)
{
	const auto plain = static_cast<entt::entity>(1);
	const auto cheating = static_cast<entt::entity>(2);
	PrayerCheats plainCheats;
	PrayerCheats cheatingCheats {.infinite = false, .freeMaintenance = true};

	InfinitePrayer prayer;
	prayer.TurnOn();
	// Reached every frame while on: what each site had is kept once
	for (int frame = 0; frame < 3; ++frame)
	{
		prayer.Apply(plain, plainCheats);
		prayer.Apply(cheating, cheatingCheats);
	}
	EXPECT_EQ(plainCheats, (PrayerCheats {.infinite = true, .freeMaintenance = true}));
	EXPECT_EQ(cheatingCheats, (PrayerCheats {.infinite = true, .freeMaintenance = true}));
	EXPECT_EQ(prayer.HeldSites(), 2);

	prayer.TurnOff([&](entt::entity site, PrayerCheats cheats) {
		if (site == plain)
		{
			plainCheats = cheats;
		}
		else if (site == cheating)
		{
			cheatingCheats = cheats;
		}
	});
	EXPECT_FALSE(prayer.IsOn());
	EXPECT_EQ(prayer.HeldSites(), 0);
	EXPECT_EQ(plainCheats, PrayerCheats {});
	EXPECT_EQ(cheatingCheats, (PrayerCheats {.infinite = false, .freeMaintenance = true}));
}

TEST(MiraclesMenu, reactionNames)
{
	EXPECT_EQ(ReactionName(Reaction::FleeFromObject), "flee from object");
	EXPECT_EQ(ReactionName(Reaction::ReactToImpressiveSpell), "react to impressive spell");
	EXPECT_EQ(ReactionName(Reaction::ReactToFightWon), "react to fight won");
	EXPECT_EQ(ReactionName(Reaction::None), "none");
}

TEST(MiraclesMenu, reactionRowsKeepTheMiraclesUnlessAll)
{
	const auto spell = static_cast<entt::entity>(10);
	const auto rock = static_cast<entt::entity>(11);
	const std::vector<ecs::effects::reactions::Reaction> reactions {
	    MakeReaction(1, rock, Reaction::LookAtObject),
	    MakeReaction(2, spell, Reaction::FleeFromSpell),
	};
	const auto isMiracle = [spell](entt::entity initiator) { return initiator == spell; };
	const auto followers = [](uint32_t reaction) { return reaction * 3; };

	const auto miracles = ReactionRows(reactions, false, isMiracle, followers);
	ASSERT_EQ(miracles.size(), 1);
	EXPECT_EQ(miracles[0].id, 2);
	EXPECT_EQ(miracles[0].name, "flee from spell");
	EXPECT_EQ(miracles[0].player, PlayerNames::PLAYER_ONE);
	EXPECT_FLOAT_EQ(miracles[0].radius, 30.0f);
	EXPECT_EQ(miracles[0].followers, 6);
	EXPECT_TRUE(miracles[0].fromMiracle);

	const auto all = ReactionRows(reactions, true, isMiracle, followers);
	ASSERT_EQ(all.size(), 2);
	EXPECT_EQ(all[0].id, 1);
	EXPECT_FALSE(all[0].fromMiracle);
	EXPECT_EQ(all[1].id, 2);
}

TEST(MiraclesMenu, theReleaseOfAPressTheWindowTookIsTakenEvenOnceTheClickIsDone)
{
	openblack::debug::LeftClickCapture capture;
	// Not awaiting a click: neither the press nor the release is taken
	EXPECT_FALSE(capture.Takes(true, false, false));
	EXPECT_FALSE(capture.Takes(false, false, false));
	// Awaiting a click over the GUI: the GUI keeps it
	EXPECT_FALSE(capture.Takes(true, true, true));
	// Awaiting a click on the land: the press is taken, and its release too once the click no longer is awaited (the
	// game keeps the left button as a toggle, so a release on its own would leave it held)
	ASSERT_TRUE(capture.Takes(true, true, false));
	capture.Seen(true);
	EXPECT_TRUE(capture.Takes(false, false, false));
	EXPECT_TRUE(capture.Takes(false, false, true));
	capture.Seen(false);
	// After that release, nothing more is taken
	EXPECT_FALSE(capture.Takes(false, false, false));
	EXPECT_FALSE(capture.Takes(true, false, false));
}

TEST(MiraclesMenu, RunningMiraclesTexts)
{
	EXPECT_EQ(SpellAge(3.3f, 20.0f), "3.3/20 s");
	EXPECT_EQ(SpellAge(3.3f, -1.0f), "3.3 s");
	EXPECT_EQ(SpellState(false), "running");
	EXPECT_EQ(SpellState(true), "closing");
	EXPECT_EQ(DispenserState(true, true), "orb ready");
	EXPECT_EQ(DispenserState(false, true), "making one");
	EXPECT_EQ(DispenserState(false, false), "inactive");
}

TEST(MiraclesMenu, CreatureSpellsLine)
{
	EXPECT_EQ(CreatureSpellName(creature_spells::Spell::Freeze), "Freeze");
	EXPECT_EQ(CreatureSpellName(creature_spells::Spell::Itchy), "Itchy");
	EXPECT_EQ(CreatureSpellPhaseName(creature_spells::Phase::Holding), "holding");
	creature_spells::Spells spells;
	EXPECT_EQ(CreatureSpellsLine(7, spells), "Creature 7:");
	spells[creature_spells::Spell::Big].phase = creature_spells::Phase::Holding;
	spells[creature_spells::Spell::Big].turnsLeft = 12;
	spells[creature_spells::Spell::Ill].phase = creature_spells::Phase::Waiting;
	EXPECT_EQ(CreatureSpellsLine(7, spells), "Creature 7: Big (holding, 12 turns) Ill (waiting, 0 turns)");
}
