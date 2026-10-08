/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <vector>

#include <gtest/gtest.h>

#include "Magic/MagicTables.h"
#include "Particles/ParticleTypes.h"

using namespace openblack;
using namespace openblack::magic;

namespace
{
/// A zeroed info block with each magic record's magicType set to where it should be (as the file has it)
std::unique_ptr<InfoConstants> Synthetic()
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

void SetName(std::array<char, 0x30>& name, const char* text)
{
	name.fill('\0');
	std::strncpy(name.data(), text, name.size() - 1);
}

/// the FIRE seed's shape: FIREBALL, its two power-ups, gestures 2/1/0
GSpellSeedInfo& MakeFireSeed(InfoConstants& info, size_t index)
{
	auto& seed = info.spellSeed.at(index);
	seed.magicTypes = {MagicType::Fireball, MagicType::FireballPowerUpOne, MagicType::FireballPowerUpTwo, MagicType::None};
	seed.powerUpGestures = {GestureType::InverseSpiral, GestureType::Spiral, GestureType::None};
	return seed;
}
} // namespace

TEST(MagicTables, sectionOfEachMagicType)
{
	EXPECT_EQ(SlotOf(MagicType::None).section, MagicInfoSection::General);
	EXPECT_EQ(SlotOf(MagicType::ExplosionOnePuTwo).index, 9);
	EXPECT_EQ(SlotOf(MagicType::HealPowerUpOne).section, MagicInfoSection::Heal);
	EXPECT_EQ(SlotOf(MagicType::HealPowerUpOne).index, 1);
	EXPECT_EQ(SlotOf(MagicType::Teleport).section, MagicInfoSection::Teleport);
	EXPECT_EQ(SlotOf(MagicType::Forest).section, MagicInfoSection::Forest);
	EXPECT_EQ(SlotOf(MagicType::FoodPowerUpOne).section, MagicInfoSection::Food);
	EXPECT_EQ(SlotOf(MagicType::Tornado).section, MagicInfoSection::StormAndTornado);
	EXPECT_EQ(SlotOf(MagicType::Tornado).index, 2);
	EXPECT_EQ(SlotOf(MagicType::PhysicalShield).section, MagicInfoSection::Shield);
	EXPECT_EQ(SlotOf(MagicType::Wood).section, MagicInfoSection::Wood);
	EXPECT_EQ(SlotOf(MagicType::WaterPowerUpOne).section, MagicInfoSection::Water);
	EXPECT_EQ(SlotOf(MagicType::FlockFlying).section, MagicInfoSection::FlockFlying);
	EXPECT_EQ(SlotOf(MagicType::FlockGround).section, MagicInfoSection::FlockGround);
	EXPECT_EQ(SlotOf(MagicType::CreatureSpellFreeze).index, 0);
	EXPECT_EQ(SlotOf(MagicType::CreatureSpellItchy).section, MagicInfoSection::CreatureSpell);
	EXPECT_EQ(SlotOf(MagicType::CreatureSpellItchy).index, 15);
}

TEST(MagicTables, recordsInMagicTypeOrder)
{
	const auto info = Synthetic();
	for (uint32_t t = 0; t < k_MagicTypeCount; ++t)
	{
		EXPECT_EQ(static_cast<uint32_t>(GetMagicInfo(*info, static_cast<MagicType>(t)).magicType), t);
	}
	EXPECT_EQ(GetMagicInfoAs<GMagicHealInfo>(*info, MagicType::Heal), &info->magicHeal[0]);
	EXPECT_EQ(GetMagicInfoAs<GMagicHealInfo>(*info, MagicType::Fireball), nullptr);
	EXPECT_EQ(GetMagicInfoAs<GMagicResourceInfo>(*info, MagicType::Wood), &info->magicWood[0]);
	EXPECT_EQ(GetMagicInfoAs<GMagicResourceInfo>(*info, MagicType::FoodPowerUpOne), &info->magicFood[1]);
	EXPECT_EQ(GetMagicInfoAs<GMagicRadiusSpellInfo>(*info, MagicType::Shield), &info->magicShield[0]);
	EXPECT_EQ(GetMagicInfoAs<GMagicCreatureSpellInfo>(*info, MagicType::CreatureSpellAngry), &info->magicCreatureSpell[9]);
}

TEST(MagicTables, infoFromText)
{
	auto info = Synthetic();
	SetName(info->magicEffect[16].debugString, "MAGIC_TYPE_STORM_WIND_RAIN");
	EXPECT_EQ(GetInfoFromText(*info, "magic_type_storm_wind_rain"), 16);
	EXPECT_FALSE(GetInfoFromText(*info, "MAGIC_TYPE_STORM").has_value());
}

TEST(MagicTables, maintainedSpells)
{
	EXPECT_TRUE(IsMaintainedSpell(MagicType::Forest));
	EXPECT_TRUE(IsMaintainedSpell(MagicType::Shield));
	EXPECT_TRUE(IsMaintainedSpell(MagicType::PhysicalShield));
	EXPECT_FALSE(IsMaintainedSpell(MagicType::Teleport));
	EXPECT_FALSE(IsMaintainedSpell(MagicType::Tornado));
	EXPECT_FALSE(IsMaintainedSpell(MagicType::Wood));
}

TEST(MagicTables, effectGetters)
{
	auto info = Synthetic();
	auto& storm = info->magicEffect[16];
	storm.timerWhenPlayerCasting = 40.0f;
	storm.costToCreate = 3500.0f;
	storm.agressiveRangeMin = 10.0f;
	storm.agressiveRangeMax = 50.0f;
	info->magicStormAndTornado[0].isCreatureCastFromAbove = 1;
	EXPECT_FLOAT_EQ(GetTimerWhenPlayerCasting(*info, MagicType::StormWindRain), 40.0f);
	EXPECT_FLOAT_EQ(GetChantsRequiredToCreate(*info, MagicType::StormWindRain), 3500.0f);
	EXPECT_TRUE(IsCreatureCastFromAbove(*info, MagicType::StormWindRain));
	EXPECT_FLOAT_EQ(IsInAggressiveRange(*info, MagicType::StormWindRain, 10.0f), 1.0f);
	EXPECT_FLOAT_EQ(IsInAggressiveRange(*info, MagicType::StormWindRain, 50.5f), 0.0f);
}

TEST(MagicTables, tribalPower)
{
	GMagicEffectInfo effect {};
	effect.useTribalPowerMultiplier[0] = 1;
	effect.useTribalPowerMultiplier[2] = 1;
	std::array<float, 9> power {};
	power.fill(1.0f);
	EXPECT_FLOAT_EQ(GetTribalPower(effect, nullptr), 1.0f);
	EXPECT_FLOAT_EQ(GetTribalPower(effect, &power), 1.0f);
	EXPECT_FALSE(GetTribalPowerTribe(effect, &power).has_value());
	power[1] = 5.0f; // not flagged
	power[2] = 3.0f;
	power[0] = 2.0f;
	EXPECT_FLOAT_EQ(GetTribalPower(effect, &power), 6.0f);
	EXPECT_EQ(GetTribalPowerTribe(effect, &power), 0);
	power[0] = 100.0f;
	EXPECT_FLOAT_EQ(GetTribalPower(effect, &power), 100.0f);
	power[0] = 0.1f;
	EXPECT_FLOAT_EQ(GetTribalPower(effect, &power), 0.5f);
	EXPECT_EQ(GetTribalPowerTribe(effect, &power), 2);
}

TEST(MagicTables, powerUpHelpers)
{
	auto info = Synthetic();
	const auto& seed = MakeFireSeed(*info, 2);
	EXPECT_EQ(GetPowerUpFromMagicType(seed, MagicType::Fireball), -1);
	EXPECT_EQ(GetPowerUpFromMagicType(seed, MagicType::FireballPowerUpOne), 0);
	EXPECT_EQ(GetPowerUpFromMagicType(seed, MagicType::FireballPowerUpTwo), 1);
	EXPECT_EQ(GetPowerUpFromMagicType(seed, MagicType::None), 2); // the original's quirk: slot 3 is 0
	EXPECT_EQ(GetPowerUpFromMagicType(seed, MagicType::Heal), -1);
	EXPECT_EQ(GetNumPowerUpLevels(seed), 3);
	EXPECT_EQ(MagicTypeForPowerUpLevel(seed, -1), MagicType::Fireball);
	EXPECT_EQ(MagicTypeForPowerUpLevel(seed, 1), MagicType::FireballPowerUpTwo);
	EXPECT_EQ(&MagicInfoForPowerUpLevel(*info, seed, 0), &info->magicGeneral[2]);
	EXPECT_EQ(&MagicInfoForPowerUpLevel(*info, seed, 2), &info->magicGeneral[1]); // no magic: the base one
	EXPECT_EQ(GetPowerUpGesture(seed, MagicType::FireballPowerUpTwo).gesture, GestureType::Spiral);
	EXPECT_EQ(GetPowerUpGesture(seed, MagicType::FireballPowerUpTwo).level, 1);
	EXPECT_EQ(GetPowerUpGesture(seed, MagicType::Fireball).gesture, GestureType::None);
	EXPECT_EQ(GetPowerUpGesture(seed, MagicType::Fireball).level, -1);
}

TEST(MagicTables, seedLookups)
{
	auto info = Synthetic();
	MakeFireSeed(*info, 2);
	auto& heal = info->spellSeed[7];
	heal.magicTypes = {MagicType::Heal, MagicType::HealPowerUpOne, MagicType::None, MagicType::None};
	heal.exists = 1;
	heal.iconIndex = 6;
	SetName(heal.debugString, "SPELL_SEED_TYPE_HEAL");

	EXPECT_TRUE(SpellSeedIsOfMagicType(info->spellSeed[2], MagicType::FireballPowerUpOne));
	EXPECT_FALSE(SpellSeedIsOfMagicType(info->spellSeed[2], MagicType::Heal));
	EXPECT_EQ(GetFirstSpellSeedForMagicType(*info, MagicType::FireballPowerUpTwo), SpellSeedType::Fire);
	EXPECT_EQ(GetFirstSpellSeedForMagicType(*info, MagicType::HealPowerUpOne), SpellSeedType::Heal);
	EXPECT_EQ(GetFirstSpellSeedForMagicType(*info, MagicType::Tornado), SpellSeedType::None);
	EXPECT_FALSE(GetSpellSeedForMagicType(*info, MagicType::Tornado).has_value());
	EXPECT_EQ(GetSpellSeedForMagicType(*info, MagicType::Heal), 7);
	EXPECT_EQ(GetSpellSeedFromText(*info, "spell_seed_type_heal"), 7);
	EXPECT_FALSE(GetSpellSeedFromText(*info, "SPELL_SEED_TYPE_FIRE").has_value());
	EXPECT_EQ(GetSpellSeedFromIconIndex(*info, 6), 7);
	EXPECT_FALSE(GetSpellSeedFromIconIndex(*info, 9).has_value());
	EXPECT_EQ(GetPowerUpGestureForMagicType(*info, MagicType::FireballPowerUpOne).gesture, GestureType::InverseSpiral);
	EXPECT_EQ(GetPowerUpGestureForMagicType(*info, MagicType::FireballPowerUpOne).level, 0);
	EXPECT_EQ(GetPowerUpGestureForMagicType(*info, MagicType::Tornado).gesture, GestureType::None);
	EXPECT_EQ(GetPowerUpGestureForMagicType(*info, MagicType::Tornado).level, -1);
}

TEST(MagicTables, spellSeedOfMagicInfo)
{
	// The rows name no seed, as info.dat ships them; the running game names each magic type's first seed
	auto info = Synthetic();
	for (auto& row : info->magicGeneral)
	{
		row.spellSeedType = SpellSeedType::None;
	}
	info->magicWood.at(0).spellSeedType = SpellSeedType::None;
	MakeFireSeed(*info, 2);
	info->spellSeed[8].magicTypes = {MagicType::Wood, MagicType::None, MagicType::None, MagicType::None};

	EXPECT_EQ(GetSpellSeedOfMagicInfo(*info, MagicType::Fireball), SpellSeedType::Fire);
	EXPECT_EQ(GetSpellSeedOfMagicInfo(*info, MagicType::FireballPowerUpTwo), SpellSeedType::Fire);
	EXPECT_EQ(GetSpellSeedOfMagicInfo(*info, MagicType::Wood), SpellSeedType::Wood);
	// a type no seed has keeps its row's value
	EXPECT_EQ(GetSpellSeedOfMagicInfo(*info, MagicType::LightningBolt), SpellSeedType::None);
	info->magicGeneral.at(static_cast<size_t>(MagicType::LightningBolt)).spellSeedType = SpellSeedType::Storm;
	EXPECT_EQ(GetSpellSeedOfMagicInfo(*info, MagicType::LightningBolt), SpellSeedType::Storm);
	// NONE is left as its row has it, though every seed's empty slots are NONE
	EXPECT_EQ(GetFirstSpellSeedForMagicType(*info, MagicType::None), SpellSeedType::Storm);
	EXPECT_EQ(GetSpellSeedOfMagicInfo(*info, MagicType::None), SpellSeedType::None);
	// past the table: none
	EXPECT_EQ(GetSpellSeedOfMagicInfo(*info, static_cast<MagicType>(k_MagicTypeCount)), SpellSeedType::None);
}

TEST(MagicTables, gestureAndLevelOfMagicInfo)
{
	// The rows name no gesture and no level, as info.dat ships them; the running game names a power-up's gesture and
	// level on its first seed
	auto info = Synthetic();
	for (auto& row : info->magicGeneral)
	{
		row.gestureType = GestureType::None;
		row.powerupType = static_cast<PowerUpType>(-1);
	}
	MakeFireSeed(*info, 2);
	// a later seed with the same power-up and another gesture does not count: the first seed's does
	info->spellSeed[6].magicTypes = {MagicType::LightningBolt, MagicType::FireballPowerUpOne, MagicType::None, MagicType::None};
	info->spellSeed[6].powerUpGestures = {GestureType::SShape, GestureType::None, GestureType::None};

	EXPECT_EQ(GetGestureOfMagicInfo(*info, MagicType::FireballPowerUpOne), GestureType::InverseSpiral);
	EXPECT_EQ(GetPowerUpLevelOfMagicInfo(*info, MagicType::FireballPowerUpOne), 0);
	EXPECT_EQ(GetGestureOfMagicInfo(*info, MagicType::FireballPowerUpTwo), GestureType::Spiral);
	EXPECT_EQ(GetPowerUpLevelOfMagicInfo(*info, MagicType::FireballPowerUpTwo), 1);
	// the base type has no gesture: its row keeps its values
	EXPECT_EQ(GetGestureOfMagicInfo(*info, MagicType::Fireball), GestureType::None);
	EXPECT_EQ(GetPowerUpLevelOfMagicInfo(*info, MagicType::Fireball), -1);
	// a type no seed has keeps its row's values, whatever they are
	auto& tornado = info->magicStormAndTornado.at(static_cast<size_t>(SlotOf(MagicType::Tornado).index));
	tornado.powerupType = static_cast<PowerUpType>(-1);
	EXPECT_EQ(GetPowerUpLevelOfMagicInfo(*info, MagicType::Tornado), -1);
	tornado.gestureType = GestureType::Circle;
	tornado.powerupType = PowerUpType::Three;
	EXPECT_EQ(GetGestureOfMagicInfo(*info, MagicType::Tornado), GestureType::Circle);
	EXPECT_EQ(GetPowerUpLevelOfMagicInfo(*info, MagicType::Tornado), 2);
	// a power-up slot with no gesture is not written: the level stays the row's too
	info->spellSeed[2].powerUpGestures[1] = GestureType::None;
	EXPECT_EQ(GetGestureOfMagicInfo(*info, MagicType::FireballPowerUpTwo), GestureType::None);
	EXPECT_EQ(GetPowerUpLevelOfMagicInfo(*info, MagicType::FireballPowerUpTwo), -1);
	// NONE is left as its row has it, though the first seed's empty slot 3 has it at level 2
	info->spellSeed[0].magicTypes = {MagicType::Heal, MagicType::HealPowerUpOne, MagicType::Teleport, MagicType::None};
	info->spellSeed[0].powerUpGestures = {GestureType::Spiral, GestureType::Spiral, GestureType::Circle};
	EXPECT_EQ(GetPowerUpGestureForMagicType(*info, MagicType::None).gesture, GestureType::Circle);
	EXPECT_EQ(GetGestureOfMagicInfo(*info, MagicType::None), GestureType::None);
	EXPECT_EQ(GetPowerUpLevelOfMagicInfo(*info, MagicType::None), -1);
	// past the table: none
	EXPECT_EQ(GetGestureOfMagicInfo(*info, static_cast<MagicType>(k_MagicTypeCount)), GestureType::None);
	EXPECT_EQ(GetPowerUpLevelOfMagicInfo(*info, static_cast<MagicType>(k_MagicTypeCount)), -1);
}

TEST(MagicTables, particleTypeFiles)
{
	EXPECT_EQ(psys::ParticleTypeFile(ParticleType::Leaves), "SF_Forest");
	EXPECT_EQ(psys::ParticleTypeFile(ParticleType::FoodPoisoned), "SF_Food");
	EXPECT_EQ(psys::ParticleTypeFile(ParticleType::Heal), "SF_HealChakra");
	EXPECT_EQ(psys::ParticleTypeFile(ParticleType::Bonfire), "SF_Bonfire");
	EXPECT_EQ(psys::ParticleTypeFile(ParticleType::SeeThisBeam), "SF_SeeThisBeam");
	EXPECT_TRUE(psys::ParticleTypeFile(ParticleType::None).empty());
	EXPECT_TRUE(psys::ParticleTypeFile(ParticleType::Tornado).empty());
	EXPECT_TRUE(psys::ParticleTypeFile(static_cast<ParticleType>(200)).empty());
}

/// With OPENBLACK_GAME_PATH set to the install: the real info.dat
// Integration test: needs the original game data (OPENBLACK_GAME_PATH); skipped without it
TEST(MagicTables, realInfoDat)
{
	const char* game = std::getenv("OPENBLACK_GAME_PATH");
	if (game == nullptr)
	{
		GTEST_SKIP() << "OPENBLACK_GAME_PATH not set";
	}
	// Scripts/info.dat: a 0x2C-byte pack header, then the InfoConstants block (what InfoFile reads, without the Locator)
	std::ifstream file(std::filesystem::path(game) / "Scripts" / "info.dat", std::ios::binary);
	ASSERT_TRUE(file.is_open());
	const std::vector<char> data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
	ASSERT_EQ(data.size(), 0x2C + sizeof(InfoConstants));
	auto info = std::make_unique<InfoConstants>();
	std::memcpy(info.get(), data.data() + 0x2C, sizeof(InfoConstants));
	for (uint32_t t = 0; t < k_MagicTypeCount; ++t)
	{
		EXPECT_EQ(static_cast<uint32_t>(GetMagicInfo(*info, static_cast<MagicType>(t)).magicType), t);
		// the file names no seed for any magic; the running game gives each its first seed
		EXPECT_EQ(GetMagicInfo(*info, static_cast<MagicType>(t)).spellSeedType, SpellSeedType::None) << t;
		EXPECT_EQ(GetSpellSeedOfMagicInfo(*info, static_cast<MagicType>(t)),
		          t == 0 ? SpellSeedType::None : GetFirstSpellSeedForMagicType(*info, static_cast<MagicType>(t)))
		    << t;
	}
	// the file names no gesture and no level either; the running game names a power-up's own
	for (uint32_t t = 0; t < k_MagicTypeCount; ++t)
	{
		const auto type = static_cast<MagicType>(t);
		EXPECT_EQ(GetMagicInfo(*info, type).gestureType, GestureType::None) << t;
		EXPECT_EQ(static_cast<int>(GetMagicInfo(*info, type).powerupType), -1) << t;
		const auto powerUp = t == 0 ? PowerUpGesture {} : GetPowerUpGestureForMagicType(*info, type);
		EXPECT_EQ(GetGestureOfMagicInfo(*info, type), powerUp.gesture) << t;
		EXPECT_EQ(GetPowerUpLevelOfMagicInfo(*info, type), powerUp.level) << t;
	}
	// the hand's FIRE test: the fireball and its two power-ups, and no other magic, name the FIRE seed
	for (uint32_t t = 0; t < k_MagicTypeCount; ++t)
	{
		const auto type = static_cast<MagicType>(t);
		EXPECT_EQ(GetSpellSeedOfMagicInfo(*info, type) == SpellSeedType::Fire,
		          type == MagicType::Fireball || type == MagicType::FireballPowerUpOne || type == MagicType::FireballPowerUpTwo)
		    << t;
	}
	// so every magic a seed casts names FIRE exactly when the seed is the FIRE one
	for (size_t i = 0; i < k_SpellSeedCount; ++i)
	{
		for (const auto type : info->spellSeed.at(i).magicTypes)
		{
			if (type != MagicType::None)
			{
				EXPECT_EQ(GetSpellSeedOfMagicInfo(*info, type) == SpellSeedType::Fire,
				          static_cast<SpellSeedType>(i) == SpellSeedType::Fire)
				    << i;
			}
		}
	}
	EXPECT_EQ(GetGestureOfMagicInfo(*info, MagicType::FireballPowerUpOne), GestureType::InverseSpiral);
	EXPECT_EQ(GetGestureOfMagicInfo(*info, MagicType::FireballPowerUpTwo), GestureType::Spiral);
	EXPECT_EQ(GetPowerUpLevelOfMagicInfo(*info, MagicType::FireballPowerUpTwo), 1);
	EXPECT_EQ(GetGestureOfMagicInfo(*info, MagicType::Fireball), GestureType::None);
	// the dispensers' magics: Land 3's WOOD and Land 5's FIRE_PU2
	EXPECT_EQ(GetSpellSeedOfMagicInfo(*info, MagicType::Wood), SpellSeedType::Wood);
	EXPECT_EQ(GetSpellSeedOfMagicInfo(*info, MagicType::FireballPowerUpTwo), SpellSeedType::Fire);
	EXPECT_FLOAT_EQ(GetChantsRequiredToCreate(*info, MagicType::Fireball), 3500.0f);
	EXPECT_FLOAT_EQ(GetChantsRequiredToCreate(*info, MagicType::StormWindRain), 8000.0f);
	EXPECT_FLOAT_EQ(GetTimerWhenPlayerCasting(*info, MagicType::StormWindRain), 40.0f);
	EXPECT_EQ(GetInfoFromText(*info, "storm_pu2"), 18);
	const auto& food = GetSpellSeedInfo(*info, SpellSeedType::Food);
	EXPECT_EQ(food.castType, SpellCastType::SpellCastInHand);
	EXPECT_EQ(food.magicTypes[0], MagicType::Food);
	EXPECT_EQ(food.holdType, HoldType::Side);
	const auto& storm = GetSpellSeedInfo(*info, SpellSeedType::Storm);
	EXPECT_EQ(storm.sizingGesture, GestureType::Circle);
	EXPECT_EQ(storm.holderParticle, ParticleType::LightningStormOnHolder);
	EXPECT_FLOAT_EQ(storm.unknown0x15C, 0.1f);
	EXPECT_EQ(GetPowerUpLevelOfMagicInfo(*info, MagicType::Tornado), 1);
	// a reaction's radius: whetherReactionGrows ? 1 : maxReactionDistance (REACT_TO_FIRE 10,
	// REACT_TO_TELEPORT)
	const auto& fire = info->reaction.at(static_cast<size_t>(Reaction::ReactToFire));
	// REACT_TO_FIRE does not grow: its spread reaches 35 m, the maxReactionDistance
	EXPECT_EQ(fire.whetherReactionGrows, 0u);
	EXPECT_FLOAT_EQ(fire.maxReactionDistance, 35.0f);
	EXPECT_EQ(GetSpellSeedFromText(*info, "Heal"), 7);
	EXPECT_EQ(psys::ParticleTypeFile(GetMagicInfo(*info, MagicType::Forest).particleType), "SF_Forest");
	// a villager's starting life (a new Living takes GLivingInfo::life)
	for (const auto& villager : info->villager)
	{
		EXPECT_FLOAT_EQ(villager.life, 1.0f) << villager.debugString.data();
	}
}
