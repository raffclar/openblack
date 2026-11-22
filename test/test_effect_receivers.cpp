/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <array>
#include <memory>

#include <glm/mat3x3.hpp>
#include <gtest/gtest.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#include "ECS/Components/Abode.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/AtHome.h"
#include "ECS/Components/Ball.h"
#include "ECS/Components/Construction.h"
#include "ECS/Components/CreatureNeeds.h"
#include "ECS/Components/Feature.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/Fire.h"
#include "ECS/Components/Firefly.h"
#include "ECS/Components/FishFarm.h"
#include "ECS/Components/HandGrab.h"
#include "ECS/Components/Indestructible.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/Physics.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/ScriptControl.h"
#include "ECS/Components/StreetLantern.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/TownDesireFlag.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/VillageTotem.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/VillagerDeath.h"
#include "ECS/Components/Vortex.h"
#include "ECS/Registry.h"
#include "ECS/StreetLanternRules.h"
#include "ECS/WorldObjects.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/AreaEffect.h"

using namespace openblack;
using namespace openblack::ecs::components;

namespace
{
/// The world's things with tables of their own, and no models
class EffectReceivers: public ::testing::Test
{
protected:
	void SetUp() override
	{
		// The rules of the world write to the game's log
		if (spdlog::get("game") == nullptr)
		{
			spdlog::create<spdlog::sinks::null_sink_st>("game");
			_madeLogger = true;
		}
		Locator::entitiesRegistry::emplace<ecs::Registry>();
		_info = std::make_unique<InfoConstants>();
		Locator::infoConstants::emplace(*_info);
	}
	void TearDown() override
	{
		Locator::infoConstants::reset();
		Locator::entitiesRegistry::reset();
		if (_madeLogger)
		{
			spdlog::drop("game");
		}
	}

	static entt::entity Made()
	{
		auto& registry = Locator::entitiesRegistry::value();
		const auto entity = registry.Create();
		registry.Assign<Transform>(entity, glm::vec3(100.0f, 0.0f, 100.0f), glm::mat3(1.0f), glm::vec3(1.0f));
		return entity;
	}

	std::unique_ptr<InfoConstants> _info;
	bool _madeLogger {false};
};
} // namespace

TEST_F(EffectReceivers, AStreetLanternTakesEffectsByItsRowOfTheStatics)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto town = Made();
	registry.Assign<StreetLantern>(town, StreetLantern {.country = false, .info = MobileStaticInfo::StreetLantern});
	const auto country = Made();
	registry.Assign<StreetLantern>(country, StreetLantern {.country = true, .info = MobileStaticInfo::CountryLantern});
	EXPECT_EQ(ecs::world_objects::EffectInfoOf(town),
	          &Locator::infoConstants::value().mobileStatic.at(static_cast<size_t>(MobileStaticInfo::StreetLantern)));
	EXPECT_EQ(ecs::world_objects::EffectInfoOf(country),
	          &Locator::infoConstants::value().mobileStatic.at(static_cast<size_t>(MobileStaticInfo::CountryLantern)));
	// A lantern is not crushed and is the neutral player's
	EXPECT_FALSE(ecs::world_objects::CanBeCrushed(town));
	EXPECT_EQ(ecs::world_objects::PlayerOf(town), PlayerNames::NEUTRAL);
}

TEST_F(EffectReceivers, AFishFarmTakesEffectsByItsOwnRowAndReachesFiveMetres)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto farm = Made();
	registry.Assign<FishFarm>(farm);
	EXPECT_EQ(ecs::world_objects::EffectInfoOf(farm), &Locator::infoConstants::value().fishFarm);
	const auto size = ecs::world_objects::SizeOf(farm);
	EXPECT_FLOAT_EQ(size.radius, 5.0f);
	EXPECT_FLOAT_EQ(size.height, 0.0f);
	EXPECT_TRUE(ecs::world_objects::CanBeCrushed(farm));
}

TEST_F(EffectReceivers, AFishFarmIsItsTownsPlayersOrNoOnes)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto farm = Made();
	registry.Assign<FishFarm>(farm);
	EXPECT_FALSE(ecs::world_objects::PlayerOf(farm).has_value());
	const auto town = registry.Create();
	registry.Assign<Town>(town, 3u, PlayerNames::PLAYER_ONE);
	registry.Get<FishFarm>(farm).town = town;
	EXPECT_EQ(ecs::world_objects::PlayerOf(farm), PlayerNames::PLAYER_ONE);
}

TEST_F(EffectReceivers, ABallTakesEffectsByTheBallsRowAndIsNotCrushed)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto ball = Made();
	registry.Assign<Ball>(ball);
	EXPECT_EQ(ecs::world_objects::EffectInfoOf(ball), &Locator::infoConstants::value().ball);
	EXPECT_FALSE(ecs::world_objects::CanBeCrushed(ball));
	EXPECT_EQ(ecs::world_objects::PlayerOf(ball), PlayerNames::NEUTRAL);
}

TEST_F(EffectReceivers, AnAnimalTakesHeatByTheRowOfItsKind)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto cow = Made();
	registry.Assign<Animal>(cow).type = AnimalInfo::Cow;
	const auto sheep = Made();
	registry.Assign<Animal>(sheep).type = AnimalInfo::Sheep;
	const auto& info = Locator::infoConstants::value();
	EXPECT_EQ(ecs::world_objects::EffectInfoOf(cow), &info.animal.at(static_cast<size_t>(AnimalInfo::Cow)));
	EXPECT_EQ(ecs::world_objects::EffectInfoOf(sheep), &info.animal.at(static_cast<size_t>(AnimalInfo::Sheep)));
}

TEST_F(EffectReceivers, AFireflyTakesHeatByItsRowOfTheFixedThings)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto firefly = Made();
	registry.Assign<Firefly>(firefly);
	EXPECT_EQ(ecs::world_objects::EffectInfoOf(firefly),
	          &Locator::infoConstants::value().singleMapFixed.at(Firefly::k_InfoRow));
	// It has the life of a plain thing, which one burn can take
	EXPECT_FLOAT_EQ(ecs::world_objects::LifeOf(firefly), 1.0f);
}

TEST_F(EffectReceivers, OneBurnCanTakeAllTheLifeOfAFirefly)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto firefly = Made();
	registry.Assign<Firefly>(firefly);
	EXPECT_FLOAT_EQ(ecs::world_objects::ReduceLife(firefly, 0.25f), 0.75f);
	EXPECT_FLOAT_EQ(ecs::world_objects::ReduceLife(firefly, 1.5f), 0.0f);
	EXPECT_FLOAT_EQ(ecs::world_objects::LifeOf(firefly), 0.0f);
}

TEST_F(EffectReceivers, HarmNeverBringsAnIndestructibleCreatureToNearlyNoLife)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto held = Made();
	registry.Assign<CreatureNeeds>(held).needs.life = 0.1f;
	registry.Assign<Indestructible>(held);
	const auto free = Made();
	registry.Assign<CreatureNeeds>(free).needs.life = 0.1f;
	// The harm takes the life while more than nearly no life stays
	EXPECT_FLOAT_EQ(ecs::world_objects::ReduceLife(held, 0.05f), 0.05f);
	// The life stays as it is when the harm leaves nearly no life, or when the harm is more than the life
	EXPECT_FLOAT_EQ(ecs::world_objects::ReduceLife(held, 0.045f), 0.05f);
	EXPECT_FLOAT_EQ(ecs::world_objects::ReduceLife(held, 1.0f), 0.05f);
	// A creature that is not indestructible loses all its life
	EXPECT_FLOAT_EQ(ecs::world_objects::ReduceLife(free, 1.0f), 0.0f);
}

TEST_F(EffectReceivers, HarmAndHealChangeTheLifeOfAnAnimal)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto cow = Made();
	registry.Assign<Animal>(cow).type = AnimalInfo::Cow;
	EXPECT_FLOAT_EQ(ecs::world_objects::ReduceLife(cow, 0.25f), 0.75f);
	EXPECT_FLOAT_EQ(registry.Get<Animal>(cow).life, 0.75f);
	EXPECT_FLOAT_EQ(ecs::world_objects::IncreaseLife(cow, 0.5f), 1.0f);
	EXPECT_FLOAT_EQ(ecs::world_objects::ReduceLife(cow, 1.5f), 0.0f);
	EXPECT_FLOAT_EQ(ecs::world_objects::LifeOf(cow), 0.0f);
}

TEST_F(EffectReceivers, HarmNeverBringsAnIndestructibleAnimalOrThingToNearlyNoLife)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto cow = Made();
	registry.Assign<Animal>(cow).type = AnimalInfo::Cow;
	registry.Assign<Indestructible>(cow);
	EXPECT_FLOAT_EQ(ecs::world_objects::ReduceLife(cow, 0.5f), 0.5f);
	EXPECT_FLOAT_EQ(ecs::world_objects::ReduceLife(cow, 1.0f), 0.5f);
	const auto firefly = Made();
	registry.Assign<Firefly>(firefly);
	registry.Assign<Indestructible>(firefly);
	EXPECT_FLOAT_EQ(ecs::world_objects::ReduceLife(firefly, 0.25f), 0.75f);
	EXPECT_FLOAT_EQ(ecs::world_objects::ReduceLife(firefly, 1.5f), 0.75f);
}

TEST_F(EffectReceivers, AnIndestructibleCraterIsNeverDestroyedByAMiracleNorHarmedToNoLife)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto crater = Made();
	registry.Assign<Feature>(crater, FeatureInfo::Crater);
	registry.Assign<Indestructible>(crater);
	// The wave of a blast and the pull of a tornado ask this before they shatter or take the crater
	EXPECT_FALSE(ecs::world_objects::CanBeDestroyedBySpell(crater));
	// The heat and the blows of a miracle, a fire or a creature never take the last of its life
	EXPECT_FLOAT_EQ(ecs::world_objects::ReduceLife(crater, 0.5f), 0.5f);
	EXPECT_FLOAT_EQ(ecs::world_objects::ReduceLife(crater, 1.0f), 0.5f);
	EXPECT_GT(ecs::world_objects::LifeOf(crater), 0.0f);
}

TEST_F(EffectReceivers, ACraterThatNoScriptMadeIndestructibleCanBeDestroyed)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto crater = Made();
	registry.Assign<Feature>(crater, FeatureInfo::Crater);
	EXPECT_TRUE(ecs::world_objects::CanBeDestroyedBySpell(crater));
	EXPECT_FLOAT_EQ(ecs::world_objects::ReduceLife(crater, 1.5f), 0.0f);
}

TEST_F(EffectReceivers, NoMiracleDestroysAVortexOrTheVolcanosMouth)
{
	auto& registry = Locator::entitiesRegistry::value();
	// The volcano's mouth on Land 5 has no indestructible mark, but it takes no effect
	for (const auto type : {VortexType::In, VortexType::Out, VortexType::Volcano})
	{
		const auto vortex = Made();
		registry.Assign<Vortex>(vortex, Vortex {.type = type});
		EXPECT_FALSE(ecs::world_objects::CanBeDestroyedBySpell(vortex));
	}
}

TEST_F(EffectReceivers, NoMiracleDestroysATownsDesireFlag)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto flag = Made();
	registry.Assign<TownDesireFlag>(flag);
	EXPECT_FALSE(ecs::world_objects::CanBeDestroyedBySpell(flag));
}

TEST_F(EffectReceivers, ATownsDesireFlagTakesNoEffectEvenWithTheDataOfThePrayerSite)
{
	auto& registry = Locator::entitiesRegistry::value();
	_info->prayerSite.combustionTemperature = 1200.0f;
	_info->prayerSite.heatCapacity = 50000.0f;
	const auto flag = Made();
	registry.Assign<TownDesireFlag>(flag);
	EXPECT_EQ(ecs::world_objects::EffectInfoOf(flag), nullptr);
}

TEST(StreetLanternRules, NoLanternWithinHalfAMetreOfAStatic)
{
	constexpr glm::vec3 k_At {100.0f, 0.0f, 100.0f};
	EXPECT_TRUE(ecs::street_lanterns::Refused(k_At, std::array {glm::vec3(100.4f, 0.0f, 100.0f)}));
	EXPECT_FALSE(ecs::street_lanterns::Refused(k_At, std::array {glm::vec3(100.6f, 0.0f, 100.0f)}));
	// Only the distance across the land counts, not the height
	EXPECT_TRUE(ecs::street_lanterns::Refused(k_At, std::array {glm::vec3(100.0f, 50.0f, 100.3f)}));
	// One near thing among far ones is enough
	EXPECT_TRUE(
	    ecs::street_lanterns::Refused(k_At, std::array {glm::vec3(108.0f, 0.0f, 100.0f), glm::vec3(99.8f, 0.0f, 99.8f)}));
	EXPECT_FALSE(ecs::street_lanterns::Refused(k_At, {}));
}

TEST_F(EffectReceivers, OnlyTheHelpReadsTheRowOfTheVortex)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto vortex = Made();
	registry.Assign<Vortex>(vortex);
	_info->mobileStatic.at(static_cast<size_t>(MobileStaticInfo::Vortex)).helpStartEnum = static_cast<HelpText>(2917);
	Locator::infoConstants::emplace(*_info);
	const auto& row = Locator::infoConstants::value().mobileStatic.at(static_cast<size_t>(MobileStaticInfo::Vortex));
	EXPECT_EQ(ecs::world_objects::HelpInfoOf(vortex), &row);
	EXPECT_EQ(ecs::world_objects::HelpInfoOf(vortex)->helpStartEnum, static_cast<HelpText>(2917));
	// Miracles, heat and the other users of the rows do not know the vortex
	EXPECT_EQ(ecs::world_objects::InfoOf(vortex), nullptr);
	EXPECT_EQ(ecs::world_objects::EffectInfoOf(vortex), nullptr);
	// A thing with a row of its own gives that row to the help too
	const auto ball = Made();
	registry.Assign<Ball>(ball);
	EXPECT_EQ(ecs::world_objects::HelpInfoOf(ball), &Locator::infoConstants::value().ball);
	EXPECT_EQ(ecs::world_objects::HelpInfoOf(Made()), nullptr);
}

TEST_F(EffectReceivers, AScriptsBuildingOfNoTownStaysAsItIsWhenAnEffectDestroysIt)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto hut = Made();
	registry.Assign<Abode>(hut, Abode {.type = AbodeNumber::A, .townId = 7});
	registry.Assign<InScript>(hut);
	registry.Assign<BuildProgress>(hut, BuildProgress {.built = 0.5f});
	registry.Assign<ObjectLife>(hut, ObjectLife {.life = 0.0f});
	ecs::world_objects::DestroyedByEffect(hut, {});
	ASSERT_TRUE(registry.Valid(hut));
	EXPECT_FLOAT_EQ(ecs::world_objects::LifeOf(hut), 0.0f);
	EXPECT_FALSE(registry.AllOf<BuildingSites>(hut));
}

TEST_F(EffectReceivers, AScriptsUnfinishedBuildingWithASiteGetsAllItsLifeAgain)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto hut = Made();
	registry.Assign<Abode>(hut, Abode {.type = AbodeNumber::A, .townId = 7});
	registry.Assign<InScript>(hut);
	registry.Assign<BuildProgress>(hut, BuildProgress {.built = 0.5f});
	registry.Assign<ObjectLife>(hut, ObjectLife {.life = 0.0f});
	registry.Assign<BuildingSites>(hut, BuildingSites {.sites = {BuildingSite {.takesBroughtWood = true}}});
	ecs::world_objects::DestroyedByEffect(hut, {});
	ASSERT_TRUE(registry.Valid(hut));
	EXPECT_FLOAT_EQ(ecs::world_objects::LifeOf(hut), 1.0f);
	// A finished one keeps no life
	registry.Remove<BuildProgress>(hut);
	registry.AssignOrReplace<ObjectLife>(hut, ObjectLife {.life = 0.0f});
	ecs::world_objects::DestroyedByEffect(hut, {});
	ASSERT_TRUE(registry.Valid(hut));
	EXPECT_FLOAT_EQ(ecs::world_objects::LifeOf(hut), 0.0f);
}

TEST_F(EffectReceivers, ATownsFieldStaysAndLosesOnlyItsCropWhenAnEffectDestroysIt)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto field = Made();
	registry.Assign<Abode>(field, Abode {.type = AbodeNumber::Field, .townId = 7});
	auto& crop = registry.Assign<Field>(field).crop;
	crop.timesSown = 3;
	crop.age = 40.0f;
	crop.food = 900.0f;
	ecs::world_objects::DestroyedByEffect(field, {});
	// The field and its footprint stay. Its town does not plan it again.
	ASSERT_TRUE(registry.Valid(field));
	EXPECT_TRUE(registry.AllOf<Abode>(field));
	const auto& after = registry.Get<const Field>(field).crop;
	EXPECT_EQ(after.timesSown, 0);
	EXPECT_FLOAT_EQ(after.age, 0.0f);
	EXPECT_FLOAT_EQ(after.food, 0.0f);
}

TEST_F(EffectReceivers, AVillageTotemGivesHarmAndLifeToItsTownCentreAndTakesTheLifeOfItsTownCentre)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto centre = Made();
	registry.Assign<Abode>(centre, Abode {.type = AbodeNumber::TownCentre});
	const auto totem = Made();
	registry.Assign<VillageTotem>(totem, VillageTotem {.townCentre = centre});
	EXPECT_FLOAT_EQ(ecs::world_objects::ReduceLife(totem, 0.25f), 0.75f);
	EXPECT_FLOAT_EQ(ecs::world_objects::LifeOf(centre), 0.75f);
	EXPECT_FLOAT_EQ(ecs::world_objects::LifeOf(totem), 0.75f);
	EXPECT_FLOAT_EQ(ecs::world_objects::IncreaseLife(totem, 0.125f), 0.875f);
	EXPECT_FLOAT_EQ(ecs::world_objects::LifeOf(centre), 0.875f);
	EXPECT_FLOAT_EQ(ecs::world_objects::LifeOf(totem), 0.875f);
	// Harm to the town centre itself also goes to the totem
	EXPECT_FLOAT_EQ(ecs::world_objects::ReduceLife(centre, 1.0f), 0.0f);
	EXPECT_FLOAT_EQ(ecs::world_objects::LifeOf(totem), 0.0f);
	// A script can still set the life of the totem itself
	ecs::world_objects::SetLife(totem, 0.5f);
	EXPECT_FLOAT_EQ(ecs::world_objects::LifeOf(totem), 0.5f);
	EXPECT_FLOAT_EQ(ecs::world_objects::LifeOf(centre), 0.0f);
}

TEST_F(EffectReceivers, ATownCentreWithItsFullLifeGainsNothingAndLeavesItsTotemAsItIs)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto centre = Made();
	registry.Assign<Abode>(centre, Abode {.type = AbodeNumber::TownCentre});
	const auto totem = Made();
	registry.Assign<VillageTotem>(totem, VillageTotem {.townCentre = centre});
	ecs::world_objects::SetLife(totem, 0.5f);
	EXPECT_FLOAT_EQ(ecs::world_objects::IncreaseLife(centre, 0.25f), 0.0f);
	EXPECT_FLOAT_EQ(ecs::world_objects::IncreaseLife(totem, 0.25f), 0.0f);
	EXPECT_FLOAT_EQ(ecs::world_objects::LifeOf(centre), 1.0f);
	EXPECT_FLOAT_EQ(ecs::world_objects::LifeOf(totem), 0.5f);
}

// A football pitch that is built takes no damage, but harm takes away what is built of an unfinished pitch
TEST_F(EffectReceivers, ABuiltFootballPitchTakesNoDamage)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto pitch = Made();
	registry.Assign<Abode>(pitch, Abode {.type = AbodeNumber::FootballPitch, .townId = 7});
	registry.Assign<ObjectLife>(pitch, ObjectLife {.life = 1.0f});
	EXPECT_FLOAT_EQ(ecs::world_objects::ReduceLife(pitch, 0.5f), 1.0f);
	EXPECT_FLOAT_EQ(ecs::world_objects::LifeOf(pitch), 1.0f);

	const auto unfinished = Made();
	registry.Assign<Abode>(unfinished, Abode {.type = AbodeNumber::FootballPitch, .townId = 7});
	registry.Assign<BuildProgress>(unfinished, BuildProgress {.built = 0.5f});
	ecs::world_objects::ReduceLife(unfinished, 0.25f);
	EXPECT_FLOAT_EQ(registry.Get<const BuildProgress>(unfinished).built, 0.25f);
}

TEST_F(EffectReceivers, APotTakesEffectsOnlyWhileItHoldsSomething)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto pot = Made();
	registry.Assign<Pot>(pot, Pot {.amount = 0, .maxAmount = 100});
	EXPECT_FALSE(ecs::world_objects::TakesEffect(pot, 0.0f, 0.0f));
	EXPECT_FALSE(ecs::world_objects::TakesEffect(pot, 50.0f, 0.0f));
	registry.Get<Pot>(pot).amount = 1;
	EXPECT_TRUE(ecs::world_objects::TakesEffect(pot, 50.0f, 0.0f));
}

TEST_F(EffectReceivers, AFieldWithoutFoodTakesNoEffectThatBurns)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto field = Made();
	auto& crop = registry.Assign<Field>(field).crop;
	// No age and no food: only an effect that does not burn
	EXPECT_FALSE(ecs::world_objects::TakesEffect(field, 10.0f, 0.0f));
	EXPECT_TRUE(ecs::world_objects::TakesEffect(field, 0.0f, 0.0f));
	EXPECT_TRUE(ecs::world_objects::TakesEffect(field, -10.0f, 0.0f));
	// Food but no age is not enough, and an age without food is not enough
	crop.food = 5.0f;
	EXPECT_FALSE(ecs::world_objects::TakesEffect(field, 10.0f, 0.0f));
	crop.food = 0.0f;
	crop.age = 1.0f;
	EXPECT_FALSE(ecs::world_objects::TakesEffect(field, 10.0f, 0.0f));
	crop.food = 5.0f;
	EXPECT_TRUE(ecs::world_objects::TakesEffect(field, 10.0f, 0.0f));
}

TEST_F(EffectReceivers, AFieldHasItsRowOfTheBuildingsTableForEveryUseNotItsCropRow)
{
	auto& registry = Locator::entitiesRegistry::value();
	// The two rows have different numbers, as the rows of the game do
	auto& abodeRow = _info->abode.at(static_cast<size_t>(AbodeInfo::CelticField));
	abodeRow.abodeNumber = AbodeNumber::Field;
	abodeRow.type = ObjectType::Abode;
	abodeRow.heatCapacity = 20.0f;
	abodeRow.defenceEffectCrush = 0.08f;
	abodeRow.collideSound = SoundCollisionType::BrickBuilding;
	abodeRow.woodValue = 2000;
	abodeRow.helpStartEnum = static_cast<HelpText>(2847);
	auto& crop = _info->fieldType.at(static_cast<size_t>(FieldTypeInfo::WheatWithFence));
	crop.type = ObjectType::Field;
	crop.heatCapacity = 100.0f;
	crop.defenceEffectCrush = 0.8f;
	crop.collideSound = SoundCollisionType::Default;
	crop.woodValue = 0;
	crop.helpStartEnum = static_cast<HelpText>(2848);
	Locator::infoConstants::emplace(*_info);
	const auto field = Made();
	registry.Assign<Abode>(field, Abode {.type = AbodeNumber::Field, .townId = 0, .foodAmount = 0, .woodAmount = 0});
	registry.Assign<Field>(field).type = FieldTypeInfo::WheatWithFence;
	const auto& row = Locator::infoConstants::value().abode.at(static_cast<size_t>(AbodeInfo::CelticField));
	// The kind, the effects and the heat, and the help all read the row of the buildings' table
	EXPECT_EQ(ecs::world_objects::InfoOf(field), &row);
	EXPECT_EQ(ecs::world_objects::EffectInfoOf(field), &row);
	EXPECT_EQ(ecs::world_objects::HelpInfoOf(field), &row);
	const auto* info = ecs::world_objects::InfoOf(field);
	ASSERT_NE(info, nullptr);
	// A creature that eats the field remembers the object type of a building
	EXPECT_EQ(info->type, ObjectType::Abode);
	EXPECT_FLOAT_EQ(info->heatCapacity, 20.0f);
	EXPECT_FLOAT_EQ(info->defenceEffectCrush, 0.08f);
	EXPECT_EQ(info->collideSound, SoundCollisionType::BrickBuilding);
	EXPECT_EQ(info->woodValue, 2000u);
	// A field with a fence shows the help text of a field, not the text of its crop row
	EXPECT_EQ(info->helpStartEnum, static_cast<HelpText>(2847));
}

TEST_F(EffectReceivers, EveryOtherThingTakesEveryEffect)
{
	const auto thing = Made();
	EXPECT_TRUE(ecs::world_objects::TakesEffect(thing, 10.0f, 0.0f));
	EXPECT_TRUE(ecs::world_objects::TakesEffect(thing, 0.0f, 0.0f));
}

TEST_F(EffectReceivers, AVillagerInItsHomeInAHandOrOnItsWayToHideTakesOnlyAHeal)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto villager = Made();
	registry.Assign<Villager>(villager);
	auto& action = registry.Assign<LivingAction>(villager);
	const auto set = [&action](LivingAction::Index index, VillagerStates state) {
		action.states.at(static_cast<size_t>(index)) = static_cast<uint8_t>(state);
	};
	set(LivingAction::Index::Top, VillagerStates::MoveToPos);
	set(LivingAction::Index::Final, VillagerStates::MoveToPos);
	EXPECT_TRUE(ecs::world_objects::IsAvailable(villager));
	EXPECT_TRUE(ecs::world_objects::TakesEffect(villager, 10.0f, 0.0f));
	EXPECT_TRUE(ecs::world_objects::TakesEffect(villager, 0.0f, 0.5f));
	registry.Assign<AtHome>(villager);
	EXPECT_FALSE(ecs::world_objects::TakesEffect(villager, 10.0f, 0.0f));
	EXPECT_TRUE(ecs::world_objects::TakesEffect(villager, 0.0f, 0.5f));
	registry.Remove<AtHome>(villager);
	registry.Assign<InHand>(villager);
	EXPECT_FALSE(ecs::world_objects::TakesEffect(villager, 10.0f, 0.0f));
	EXPECT_TRUE(ecs::world_objects::TakesEffect(villager, 0.0f, 0.5f));
	registry.Remove<InHand>(villager);
	set(LivingAction::Index::Top, VillagerStates::GoAndHideInNearbyBuilding);
	EXPECT_FALSE(ecs::world_objects::TakesEffect(villager, 10.0f, 0.0f));
	EXPECT_TRUE(ecs::world_objects::TakesEffect(villager, 0.0f, 0.5f));
	// A villager stays available in these places
	EXPECT_TRUE(ecs::world_objects::IsAvailable(villager));
}

TEST_F(EffectReceivers, ADyingVillagerIsNotAvailableAndADeadOneIsNotHealed)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto villager = Made();
	registry.Assign<Villager>(villager);
	auto& action = registry.Assign<LivingAction>(villager);
	const auto set = [&action](LivingAction::Index index, VillagerStates state) {
		action.states.at(static_cast<size_t>(index)) = static_cast<uint8_t>(state);
	};
	set(LivingAction::Index::Top, VillagerStates::MoveToPos);
	set(LivingAction::Index::Final, VillagerStates::Dying);
	EXPECT_FALSE(ecs::world_objects::IsAvailable(villager));
	EXPECT_FALSE(ecs::world_objects::TakesEffect(villager, 10.0f, 0.0f));
	EXPECT_FALSE(ecs::world_objects::TakesEffect(villager, 0.0f, 0.5f));
	// In the dead state it is available again, but it is dead: harm reaches it and a heal does not
	set(LivingAction::Index::Top, VillagerStates::Dead);
	set(LivingAction::Index::Final, VillagerStates::Dead);
	EXPECT_TRUE(ecs::world_objects::IsAvailable(villager));
	EXPECT_TRUE(ecs::world_objects::TakesEffect(villager, 10.0f, 0.0f));
	EXPECT_FALSE(ecs::world_objects::TakesEffect(villager, 0.0f, 0.5f));
	// The death mark makes it dead in any state
	set(LivingAction::Index::Top, VillagerStates::MoveToPos);
	set(LivingAction::Index::Final, VillagerStates::MoveToPos);
	EXPECT_TRUE(ecs::world_objects::TakesEffect(villager, 0.0f, 0.5f));
	registry.Assign<VillagerDeath>(villager);
	EXPECT_FALSE(ecs::world_objects::TakesEffect(villager, 0.0f, 0.5f));
}

TEST(VillagerReceiver, AHealNeedsALiveVillagerAndEveryOtherEffectAReachableOne)
{
	using magic::VillagerReceiverFacts;
	EXPECT_TRUE(magic::VillagerTakesEffect({}, 0.0f));
	EXPECT_TRUE(magic::VillagerTakesEffect({}, 1.0f));
	// A heal of no strength counts as another effect
	EXPECT_FALSE(magic::VillagerTakesEffect(VillagerReceiverFacts {.atHome = true}, 0.0f));
	EXPECT_TRUE(magic::VillagerTakesEffect(VillagerReceiverFacts {.atHome = true}, 0.1f));
	EXPECT_FALSE(magic::VillagerTakesEffect(VillagerReceiverFacts {.dead = true}, 0.1f));
	// A dead villager that is not dying, not in its home, not in a hand and not on its way to hide takes harm
	EXPECT_TRUE(magic::VillagerTakesEffect(VillagerReceiverFacts {.dead = true}, -1.0f));
	EXPECT_FALSE(magic::VillagerTakesEffect(VillagerReceiverFacts {.goesToDying = true}, 0.0f));
	EXPECT_FALSE(magic::VillagerAvailable(VillagerReceiverFacts {.goesToDying = true}));
	EXPECT_TRUE(magic::VillagerAvailable(VillagerReceiverFacts {.dead = true, .atHome = true, .inHand = true, .hiding = true}));
}
