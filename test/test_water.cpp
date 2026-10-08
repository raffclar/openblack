/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The water miracle (Magic/Spells/SpellWater, Particles/Creators/Mist): the spell's numbers (the
// rain radius, the ring growth and colours, the drop distance, the reach, the ring timing in single precision), the
// ParticleMistCreator's cloud atom under UR_HandSprinkle and its colour, and, with OPENBLACK_GAME_PATH, the real rows
// (info.dat WATER / WATER_PU1, the fields' effectOfWaterSpell) and SF_Water / SF_WaterPU1.

#include <cstdlib>
#include <cstring>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <vector>

#include <glm/geometric.hpp>
#include <gtest/gtest.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#include "Common/Zip.h"
#include "InfoConstants.h"
#include "Magic/MagicTables.h"
#include "Magic/Spells/SpellWater.h"
#include "Particles/Creators/Mist.h"
#include "Particles/PSys.h"
#include "Particles/PSysFile.h"
#include "Particles/PSysRegistry.h"

using namespace openblack;
using namespace openblack::magic;

namespace
{
std::shared_ptr<const psys::File> Parse(const std::string& text)
{
	auto file = psys::File::Parse(text, "test");
	return file.has_value() ? std::make_shared<const psys::File>(std::move(*file)) : nullptr;
}

/// SF_Water.txt's cloud: UR_HandSprinkle with the ParticleMistCreator source (the cone and the sound left out)
const std::string k_Cloud = "BEGINPROPERTIES\n"
                            "PROPERTY DeleteOnCloseDown BOOL 0\n"
                            "PROPERTY Hierarchies ARRAY SIZE 25 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0\n"
                            "PROPERTY InitiallyCreated ARRAY SIZE 25 1 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0\n"
                            "ENDPROPERTIES\n"
                            "BEGINCLASS ParticleMistCreator ParticleMistCreator0\nBEGINPROPERTIES\n"
                            "PROPERTY ColorA INTEGER 200\nPROPERTY ColorB INTEGER 255\nPROPERTY ColorG INTEGER 181\n"
                            "PROPERTY ColorR INTEGER 183\nPROPERTY InitialScale FLOAT 0.2\nPROPERTY Ratio FLOAT 2.0\n"
                            "PROPERTY IsShadowMap BOOL 0\nPROPERTY LoadLightMap BOOL 0\nPROPERTY Pitch INTEGER 12\n"
                            "PROPERTY RandomiseScale BOOL 0\nPROPERTY TextureFileName STRING NULL_STRING\n"
                            "ENDPROPERTIES\nENDCLASS\n"
                            "BEGINCLASS UR_HandSprinkle UR_HandSprinkle0\nBEGINPROPERTIES\n"
                            "PROPERTY PCreator PERSIS_PNTR ParticleMistCreator0\nPROPERTY NextGroups ARRAY SIZE 0\n"
                            "PROPERTY AngleToRaise FLOAT 0.0\nPROPERTY ClampHand BOOL 0\nPROPERTY Group INTEGER 0\n"
                            "PROPERTY HeightToRaise FLOAT 8\nPROPERTY TotalTime FLOAT 8\n"
                            "ENDPROPERTIES\nENDCLASS\n";

/// Data\Spells\ZSpellFiles\<name>_txt.zzz: the length then a deflate stream (PSysFile.cpp does the same)
std::optional<psys::File> LoadSpellFile(const std::filesystem::path& root, const std::string& name)
{
	std::ifstream stream(root / "Data" / "Spells" / "ZSpellFiles" / (name + "_txt.zzz"), std::ios::binary);
	if (!stream.is_open())
	{
		return std::nullopt;
	}
	const std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
	if (bytes.size() <= 4)
	{
		return std::nullopt;
	}
	uint32_t size = 0;
	std::memcpy(&size, bytes.data(), sizeof(size));
	const auto inflated = zip::Inflate(std::vector<uint8_t>(bytes.begin() + 4, bytes.end()), size);
	return psys::File::Parse(std::string(inflated.begin(), inflated.end()), name);
}
} // namespace

TEST(Water, getters)
{
	// RainRadius / RippleGrowth: 22 WATER, 23 WATER_PU1, anything else 1
	EXPECT_FLOAT_EQ(water::RainRadius(MagicType::Water), 6.0f);
	EXPECT_FLOAT_EQ(water::RainRadius(MagicType::WaterPowerUpOne), 12.0f);
	EXPECT_FLOAT_EQ(water::RainRadius(MagicType::Heal), 1.0f);
	EXPECT_FLOAT_EQ(water::RippleGrowth(MagicType::Water), 2.0f);
	EXPECT_FLOAT_EQ(water::RippleGrowth(MagicType::WaterPowerUpOne), 4.0f);
	EXPECT_FLOAT_EQ(water::RippleGrowth(MagicType::Food), 1.0f);
	EXPECT_FLOAT_EQ(water::k_RippleEvery, 0.1f);
	EXPECT_EQ(water::k_RippleColours[0], 0xFF80CBC5u);
	EXPECT_EQ(water::k_RippleColours[4], 0xFFBD9C8Au);
}

TEST(Water, dropDistanceAndReach)
{
	// GameFloatRand(R) x 0.7 + 0.3: 0.3 at the centre, 0.7 R + 0.3 at most (4.5 m for WATER, 8.7 m for WATER_PU1)
	EXPECT_FLOAT_EQ(water::DropDistance(0.0f), 0.3f);
	EXPECT_FLOAT_EQ(water::DropDistance(6.0f), 4.5f);
	EXPECT_FLOAT_EQ(water::DropDistance(12.0f), 8.7f);
	// 2.5 x the spell's power (1) > distance - radius, strictly
	EXPECT_TRUE(water::InReach(2.4f, 0.0f));
	EXPECT_FALSE(water::InReach(2.5f, 0.0f));
	EXPECT_TRUE(water::InReach(7.4f, 5.0f)); // a field (Get2DRadius 5) is watered from 7.5 m of its centre
	EXPECT_FALSE(water::InReach(7.5f, 5.0f));
}

TEST(Water, ringTimingInSinglePrecision)
{
	// the upkeep adds 0.1 to the age every turn before the spell runs; a ring when 0.1 < age - last (float)
	float age = 0.0f;
	float last = 0.0f;
	int rings = 0;
	int firstRingTurn = -1;
	for (int turn = 0; turn < 60; ++turn)
	{
		age += 0.1f;
		if (water::RippleDue(age, last))
		{
			last = age;
			++rings;
			if (firstRingTurn < 0)
			{
				firstRingTurn = turn;
			}
		}
	}
	// the first turn (age 0.1 - 0) is not strictly more than 0.1: no ring; after that the float rounding decides
	EXPECT_EQ(firstRingTurn, 1);
	EXPECT_GE(rings, 20);
	EXPECT_LE(rings, 59);
	EXPECT_FALSE(water::RippleDue(0.1f, 0.0f));
	EXPECT_TRUE(water::RippleDue(0.3f, 0.1f)); // 0.3f - 0.1f rounds above 0.1f
}

TEST(Water, mistColour)
{
	// MistColour: (c x base) >> 8 per channel, the alpha x 0xFF >> 8
	EXPECT_EQ(psys::mist_atoms::MistColour(0xC8B7B5FFu, 0xFFFFFFFFu), 0xC7B6B4FEu);
	EXPECT_EQ(psys::mist_atoms::MistColour(0xFF808080u, 0x00404040u), 0xFE202020u);
}

TEST(Water, mistCreatorMakesTheCloudAtom)
{
	ASSERT_NE(psys::FindCreatorFactory("ParticleMistCreator"), nullptr);
	const auto file = Parse(k_Cloud);
	ASSERT_NE(file, nullptr);
	psys::Effect effect(file, glm::vec3(0.0f), 1.0f);
	psys::ProcessInfo info;
	info.handPos = glm::vec3(100.0f, 30.0f, 200.0f);
	effect.SetProcessInfo(info);
	effect.Step(0.1f);
	std::vector<psys::Effect::DrawAtom> atoms;
	effect.Collect(1.0f, atoms, psys::Creator::Kind::Other);
	ASSERT_EQ(atoms.size(), 1u);
	const auto* mist = dynamic_cast<const psys::MistCreator*>(atoms[0].creator);
	ASSERT_NE(mist, nullptr);
	EXPECT_FLOAT_EQ(mist->ratio, 2.0f);
	EXPECT_FALSE(mist->loadLightMap);
	EXPECT_TRUE(mist->lightMap.empty());
	EXPECT_NEAR(atoms[0].scale, 0.2f, 1e-5f);
	EXPECT_NEAR(glm::distance(atoms[0].position, info.handPos), 0.0f, 1e-4f);
	EXPECT_EQ(atoms[0].colour[0], 183);
	EXPECT_NEAR(atoms[0].alpha, 200.0f, 1e-3f);
}

/// With OPENBLACK_GAME_PATH set to the install: the real rows and spell files
// Integration test: needs the original game data (OPENBLACK_GAME_PATH); skipped without it
TEST(Water, realData)
{
	const char* game = std::getenv("OPENBLACK_GAME_PATH");
	if (game == nullptr)
	{
		GTEST_SKIP() << "OPENBLACK_GAME_PATH not set";
	}
	std::ifstream file(std::filesystem::path(game) / "Scripts" / "info.dat", std::ios::binary);
	ASSERT_TRUE(file.is_open());
	const std::vector<char> data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
	ASSERT_EQ(data.size(), 0x2C + sizeof(InfoConstants));
	auto info = std::make_unique<InfoConstants>();
	std::memcpy(info.get(), data.data() + 0x2C, sizeof(InfoConstants));
	for (const auto type : {MagicType::Water, MagicType::WaterPowerUpOne})
	{
		const auto* row = GetMagicInfoAs<GMagicWaterInfo>(*info, type);
		ASSERT_NE(row, nullptr);
		EXPECT_EQ(row->magicType, type);
		EXPECT_EQ(row->isSpellRecharged, 1u);
		const auto& effect = GetMagicEffectInfo(*info, type);
		EXPECT_FLOAT_EQ(effect.effectBurn, -4000.0f); // the drop cools fires
		EXPECT_FLOAT_EQ(effect.radius, 1.0f);
		EXPECT_FLOAT_EQ(effect.costPerEvent, 10.0f);
		EXPECT_FLOAT_EQ(effect.timerWhenPlayerCasting, 6.0f);
		EXPECT_FLOAT_EQ(effect.timerWhenComputerPlayerCasting, 10.0f);
		EXPECT_EQ(effect.createReactionOnEvent, 1u);
		EXPECT_EQ(effect.reactionType, Reaction::LookAtNiceSpell);
	}
	EXPECT_EQ(GetMagicInfoAs<GMagicWaterInfo>(*info, MagicType::Water)->particleType, ParticleType::Water);
	EXPECT_EQ(GetMagicInfoAs<GMagicWaterInfo>(*info, MagicType::WaterPowerUpOne)->particleType, ParticleType::WaterPuOne);
	EXPECT_FLOAT_EQ(GetMagicEffectInfo(*info, MagicType::Water).costToCreate, 5000.0f);
	EXPECT_FLOAT_EQ(GetMagicEffectInfo(*info, MagicType::WaterPowerUpOne).costToCreate, 7000.0f);
	// a field's water spell reads effectOfWaterSpell: 2.0 in the 6 rows (SpellWater uses the first row)
	for (const auto& field : info->fieldType)
	{
		EXPECT_FLOAT_EQ(field.effectOfWaterSpell, 2.0f);
		EXPECT_FLOAT_EQ(field.timesToSow, 30.0f);
		EXPECT_FLOAT_EQ(field.ageRecolt, 1200.0f);
		EXPECT_FLOAT_EQ(field.totalFoodInField, 350.0f);
	}
	// the mesh creator logs a missing mesh (no resources in the test) to the "game" logger
	if (spdlog::get("game") == nullptr)
	{
		spdlog::create<spdlog::sinks::null_sink_mt>("game");
	}
	// SF_Water / SF_WaterPU1: the cloud (a ParticleMistCreator, Ratio 2) that UR_HandSprinkle raises 8 m over 8 s
	for (const auto& [name, cloudScale, coneScale] :
	     {std::tuple {"SF_Water", 0.2f, 0.7f}, std::tuple {"SF_WaterPU1", 0.4f, 1.4f}})
	{
		const auto spell = LoadSpellFile(game, name);
		ASSERT_TRUE(spell.has_value()) << name;
		const auto* sprinkle = spell->Find("UR_HandSprinkle0");
		ASSERT_NE(sprinkle, nullptr) << name;
		EXPECT_FLOAT_EQ(sprinkle->Float("HeightToRaise", 0.0f), 8.0f);
		EXPECT_FLOAT_EQ(sprinkle->Float("TotalTime", 0.0f), 8.0f);
		EXPECT_FALSE(sprinkle->Bool("ClampHand", true));
		const auto* cloud = spell->Find(sprinkle->String("PCreator"));
		ASSERT_NE(cloud, nullptr) << name;
		EXPECT_EQ(cloud->className, "ParticleMistCreator");
		EXPECT_FLOAT_EQ(cloud->Float("Ratio", 0.0f), 2.0f);
		EXPECT_NE(spell->Find("ParticleMeshCreator_RainCone"), nullptr) << name;
		// run it: the cloud atom (a mist) at the hand and, in its group 4, the rain cone (a mesh atom) under it
		psys::Effect effect(std::make_shared<const psys::File>(*spell), glm::vec3(0.0f), 1.0f);
		psys::ProcessInfo processInfo;
		processInfo.handPos = glm::vec3(50.0f, 40.0f, 60.0f);
		effect.SetProcessInfo(processInfo);
		effect.Step(0.1f);
		effect.Step(0.1f);
		effect.Step(0.1f);
		EXPECT_EQ(effect.AtomCount(), 2u) << name;
		std::vector<psys::Effect::DrawAtom> mists;
		effect.Collect(1.0f, mists, psys::Creator::Kind::Other);
		ASSERT_EQ(mists.size(), 1u) << name;
		EXPECT_NEAR(mists[0].scale, cloudScale, 1e-5f) << name;
		std::vector<psys::Effect::DrawAtom> cones;
		effect.Collect(1.0f, cones, psys::Creator::Kind::Mesh);
		ASSERT_EQ(cones.size(), 1u) << name;
		EXPECT_NEAR(glm::distance(cones[0].position, mists[0].position), 0.0f, 1e-3f) << name;
		EXPECT_NEAR(cones[0].scale, coneScale, 1e-5f) << name;
	}
}
