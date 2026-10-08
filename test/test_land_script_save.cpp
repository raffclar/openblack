/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The physics classes' SaveObject writers (ECS/LandScriptSave.h): MobileStatic and MobileObject, with the save pass
// (CheckAndSetSaved), the position relative to `at` and the duplicate pass.

#define LOCATOR_IMPLEMENTATIONS

#include <algorithm>
#include <memory>
#include <string>

#include <glm/mat3x3.hpp>
#include <gtest/gtest.h>

#include "3D/MapCoords.h"
#include "ECS/Components/Fragment.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/OneOffSpellSeed.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/Scaffold.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/LandScriptSave.h"
#include "ECS/MapCells.h"
#include "ECS/ObjectCreationIndex.h"
#include "ECS/Registry.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/MagicTables.h"
#include "support/TestServices.h"
#include "support/WorldSystems.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;
namespace save = openblack::ecs::land_script_save;

class LandScriptSave: public ::testing::Test
{
protected:
	void SetUp() override
	{
		test::EmplaceMapAndVillagerDefaults();
		Locator::entitiesRegistry::emplace<ecs::Registry>();
		openblack::test::EmplaceWorldSystems();
		map_cells::Clear();
		object_index::OnLoadMap();
		save::BeginSavePass();
	}
	void TearDown() override
	{
		map_cells::Clear();
		Locator::entitiesRegistry::reset();
		openblack::test::ResetWorldSystems();
		test::ResetMapAndVillagerDefaults();
	}
	static ecs::Registry& Reg() { return Locator::entitiesRegistry::value(); }

	template <typename Class, typename Info>
	static entt::entity Make(const glm::vec3& p, Info type, float scale = 1.0f)
	{
		const auto e = Reg().Create();
		object_index::Assign(e);
		Reg().Assign<Transform>(e, p, glm::mat3(1.0f), glm::vec3(scale));
		Reg().Assign<Class>(e, Class {type});
		map_cells::InsertMapObject(e);
		return e;
	}
};

TEST_F(LandScriptSave, MobileStaticLine)
{
	// 1000 m / 5 m: ToFixed gives exact cells, so the text is exact (no island: altitude 0)
	const auto rock = Make<MobileStatic>(glm::vec3(1000.0f, 0.0f, 5.0f), MobileStaticInfo::CeltFenceShort, 1.5f);
	EXPECT_EQ(save::SaveMobileStatic(rock, nullptr),
	          "CREATE_MOBILE_STATIC(\"1000.00,5.00\", 0, 0.000000, 0.000000, 0.000000, 0.000000, 1.500000)\n");
	// already saved in this pass
	EXPECT_FALSE(save::SaveMobileStatic(rock, nullptr).has_value());
	// a new pass writes it again; relative to an In at 990 m (MapCoords - at)
	save::BeginSavePass();
	const auto at = map_coords::FromWorld(glm::vec3(990.0f, 0.0f, 0.0f));
	EXPECT_EQ(save::SaveMobileStatic(rock, &at),
	          "CREATE_MOBILE_STATIC(\"10.00,5.00\", 0, 0.000000, 0.000000, 0.000000, 0.000000, 1.500000)\n");
}

TEST_F(LandScriptSave, FragmentIsWrittenAsARock)
{
	// a fragment saves as the mobile static row 2 (Rock), scale 1.0
	const auto e = Reg().Create();
	object_index::Assign(e);
	Reg().Assign<Transform>(e, glm::vec3(1000.0f, 0.0f, 5.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	Reg().Assign<Fragment>(e);
	save::RegisterPhysicsWriters();
	EXPECT_EQ(save::SaveObject(e, nullptr),
	          "CREATE_MOBILE_STATIC(\"1000.00,5.00\", 2, 0.000000, 0.000000, 0.000000, 0.000000, 1.000000)\n");
}

TEST_F(LandScriptSave, MobileObjectLineAndDuplicates)
{
	const auto barrel = Make<MobileObject>(glm::vec3(1000.0f, 0.0f, 5.0f), MobileObjectInfo::EgyptBarrel);
	const auto twin = Make<MobileObject>(glm::vec3(1000.0f, 0.0f, 5.0f), MobileObjectInfo::EgyptBarrel);
	// trunc(y x 1000), trunc(scale x 1000)
	EXPECT_EQ(save::SaveMobileObject(barrel, nullptr), "CREATE_MOBILEOBJECT(\"1000.00,5.00\", 0, 0, 1000)\n");
	// the same object at the same MapCoords was marked saved with it
	EXPECT_FALSE(save::SaveMobileObject(twin, nullptr).has_value());
}

TEST_F(LandScriptSave, PotLine)
{
	// a pot's resource type is its pot info row's resource type
	auto info = std::make_unique<InfoConstants>();
	info->pot.at(static_cast<size_t>(PotInfo::WoodPot)).resourceType = ResourceType::Wood;
	Locator::infoConstants::reset(info.release());
	const auto e = Reg().Create();
	object_index::Assign(e);
	Reg().Assign<Transform>(e, glm::vec3(1000.0f, 0.0f, 5.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	auto& pot = Reg().Assign<Pot>(e);
	pot.amount = 120;
	pot.maxAmount = 200;
	pot.type = PotInfo::WoodPot;
	// CREATE_POT(pos, the info's index, the resource type, the amount)
	EXPECT_EQ(save::SavePot(e, nullptr), "CREATE_POT(\"1000.00,5.00\", 1, 1, 120)\n");
	Locator::infoConstants::reset();
}

TEST_F(LandScriptSave, DeadTreeLine)
{
	// CREATE_DEAD_TREE(pos, the player's name, the tree info's index, the life, X, Y, Z)
	const auto e = Reg().Create();
	object_index::Assign(e);
	Reg().Assign<Transform>(e, glm::vec3(1000.0f, 0.0f, 5.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	Reg().Assign<DeadTree>(e, DeadTree {static_cast<TreeInfo>(3)});
	EXPECT_EQ(save::SaveDeadTree(e, nullptr),
	          "CREATE_DEAD_TREE(\"1000.00,5.00\", \"PLAYER_ONE\", 3, 1.000000, 0.000000, 0.000000, 0.000000)\n");
}

TEST_F(LandScriptSave, OneOffSpellSeedLine)
{
	// CREATE_ONE_SHOT_SPELL_PU(pos, the magic effect's debugString), the power-up level's magic
	auto info = std::make_unique<InfoConstants>();
	auto& seedInfo = info->spellSeed.at(4);
	seedInfo.magicTypes = {static_cast<MagicType>(7), static_cast<MagicType>(9), MagicType::None, MagicType::None};
	// the magic info records of 7 and 9 point at other effects (their own magicType): the text is that effect's
	const_cast<GMagicInfo&>(magic::GetMagicInfo(*info, static_cast<MagicType>(7))).magicType = static_cast<MagicType>(11);
	const_cast<GMagicInfo&>(magic::GetMagicInfo(*info, static_cast<MagicType>(9))).magicType = static_cast<MagicType>(13);
	const std::string base = "MAGIC_TYPE_BASE";
	const std::string level = "MAGIC_TYPE_LEVEL_0";
	const std::string wrong = "WRONG_EFFECT";
	std::copy(base.begin(), base.end(), info->magicEffect.at(11).debugString.begin());
	std::copy(level.begin(), level.end(), info->magicEffect.at(13).debugString.begin());
	std::copy(wrong.begin(), wrong.end(), info->magicEffect.at(7).debugString.begin());
	std::copy(wrong.begin(), wrong.end(), info->magicEffect.at(9).debugString.begin());
	Locator::infoConstants::reset(info.release());
	const auto e = Reg().Create();
	object_index::Assign(e);
	Reg().Assign<Transform>(e, glm::vec3(1000.0f, 0.0f, 5.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	auto& seed = Reg().Assign<OneOffSpellSeed>(e);
	seed.seedType = static_cast<SpellSeedType>(4);
	seed.powerUp = 0; // magicTypes[1]
	EXPECT_EQ(save::SaveOneOffSpellSeed(e, nullptr), "CREATE_ONE_SHOT_SPELL_PU(\"1000.00,5.00\", \"MAGIC_TYPE_LEVEL_0\")\n");
	save::BeginSavePass();
	seed.powerUp = -1; // the base: magicTypes[0]
	EXPECT_EQ(save::SaveOneOffSpellSeed(e, nullptr), "CREATE_ONE_SHOT_SPELL_PU(\"1000.00,5.00\", \"MAGIC_TYPE_BASE\")\n");
	Locator::infoConstants::reset();
}

TEST_F(LandScriptSave, DispatchByClass)
{
	save::RegisterPhysicsWriters();
	const auto rock = Make<MobileStatic>(glm::vec3(1000.0f, 0.0f, 5.0f), MobileStaticInfo::CeltFenceShort);
	EXPECT_TRUE(save::SaveObject(rock, nullptr).has_value());
	const auto other = Reg().Create();
	EXPECT_FALSE(save::SaveObject(other, nullptr).has_value()); // no writer for a plain object
	// a Scaffold (a MobileObject) and a Bonfire (a MobileStatic) have their own writers: not these
	const auto scaffold = Make<MobileObject>(glm::vec3(1010.0f, 0.0f, 5.0f), MobileObjectInfo::EgyptBarrel);
	Reg().Assign<Scaffold>(scaffold);
	EXPECT_FALSE(save::SaveObject(scaffold, nullptr).has_value());
	const auto bonfire = Make<MobileStatic>(glm::vec3(1020.0f, 0.0f, 5.0f), MobileStaticInfo::Bonfire);
	EXPECT_FALSE(save::SaveObject(bonfire, nullptr).has_value());
}
