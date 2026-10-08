/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The map script's villager commands (map commands 15..18): the lookups of LHScriptX/VillagerCommands, with the
// expected values of the original game.

#define LOCATOR_IMPLEMENTATIONS

#include <memory>

#include <entt/entity/entity.hpp>
#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "ECS/Components/Abode.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/ObjectCreationIndex.h"
#include "ECS/Registry.h"
#include "Enums.h"
#include "InfoConstants.h"
#include "LHScriptX/VillagerCommands.h"
#include "Locator.h"
#include "support/WorldSystems.h"

using namespace openblack;
using namespace openblack::ecs::components;
namespace vc = openblack::lhscriptx::villager_commands;

TEST(ScriptVillagerCommands, InfoFromText)
{
	// VillagerInfoFromText: tribe x 7 + villager, case-insensitive
	EXPECT_EQ(vc::VillagerInfoFromText("CELTIC_HOUSEWIFE"), 0);
	EXPECT_EQ(vc::VillagerInfoFromText("CELTIC_FISHERMAN"), 2); // Land2's CREATE_TOWN_VILLAGER
	EXPECT_EQ(vc::VillagerInfoFromText("celtic_Fisherman"), 2);
	EXPECT_EQ(vc::VillagerInfoFromText("AFRICAN_HOUSEWIFE"), 7);
	EXPECT_EQ(vc::VillagerInfoFromText("TIBETAN_TRADER"), 62);
	// none: 0x54; the tribe needs its '_' and a whole villager name
	EXPECT_EQ(vc::VillagerInfoFromText("CELTICFISHERMAN"), vc::k_NoVillagerInfo);
	EXPECT_EQ(vc::VillagerInfoFromText("CELTIC_"), vc::k_NoVillagerInfo);
	EXPECT_EQ(vc::VillagerInfoFromText("CELTIC_FISHERMANX"), vc::k_NoVillagerInfo);
	// CREATE_VILLAGER's second argument is a position: never a name
	EXPECT_EQ(vc::VillagerInfoFromText("3079.69,3117.83"), vc::k_NoVillagerInfo);
	EXPECT_EQ(vc::k_NoVillagerInfo, 84);
}

class ScriptVillagerCommandsAbodes: public ::testing::Test
{
protected:
	void SetUp() override
	{
		auto info = std::make_unique<InfoConstants>();
		auto& abode = info->abode.at(0);
		abode.abodeNumber = AbodeNumber::A;
		abode.tribeType = Tribe::CELTIC;
		abode.maxVillagersInAbode = 2;
		Locator::infoConstants::reset(info.release());
		Locator::entitiesRegistry::emplace<ecs::Registry>();
		openblack::test::EmplaceWorldSystems();
		// entity 0 is "no thing" to some lookups
		static_cast<void>(Locator::entitiesRegistry::value().Create());
	}

	void TearDown() override
	{
		Locator::entitiesRegistry::reset();
		openblack::test::ResetWorldSystems();
		Locator::infoConstants::reset();
	}

	static entt::entity MakeTown(uint32_t id, PlayerNames owner, uint32_t stamp)
	{
		auto& registry = Locator::entitiesRegistry::value();
		const auto town = registry.Create();
		auto& t = registry.Assign<Town>(town, id);
		t.owner = owner;
		t.ownerListStamp = stamp;
		registry.Assign<Tribe>(town, Tribe::CELTIC);
		registry.Assign<Transform>(town, glm::vec3(50.0f, 0.0f, 50.0f), glm::mat3(1.0f), glm::vec3(1.0f));
		registry.Context().towns[id] = town;
		return town;
	}

	/// An abode without a mesh: its door is its position (GetDoorPos without a door point)
	static entt::entity MakeAbode(glm::vec2 at, uint32_t townId)
	{
		auto& registry = Locator::entitiesRegistry::value();
		const auto abode = registry.Create();
		ecs::object_index::Assign(abode);
		registry.Assign<Abode>(abode, AbodeNumber::A, townId, 0u, 0u);
		registry.Assign<Transform>(abode, glm::vec3(at.x, 0.0f, at.y), glm::mat3(1.0f), glm::vec3(1.0f));
		return abode;
	}
};

TEST_F(ScriptVillagerCommandsAbodes, FindAbodeAt)
{
	// the neutral town (the last player slot) and PLAYER_ONE's: the player walk visits PLAYER_ONE first
	const auto neutral = MakeTown(2, PlayerNames::NEUTRAL, 1);
	const auto mine = MakeTown(1, PlayerNames::PLAYER_ONE, 2);
	const auto neutralAbode = MakeAbode({61.0f, 62.0f}, 2);
	const auto older = MakeAbode({63.0f, 64.0f}, 1);
	const auto newer = MakeAbode({68.0f, 69.0f}, 1);
	MakeAbode({75.0f, 62.0f}, 1); // the next cell in x
	// cell (6, 6): PLAYER_ONE's town first, its newest abode first (AddStructureToTown adds at the head)
	auto found = vc::FindAbodeAt({65.0f, 0.0f, 65.0f});
	EXPECT_EQ(found.abode, newer);
	EXPECT_EQ(found.town, mine);
	// only the cell counts (the high words), not the distance: 60.0 is cell 6 too, 59.9 is cell 5
	EXPECT_EQ(vc::FindAbodeAt({60.0f, 0.0f, 60.0f}).abode, newer);
	EXPECT_TRUE(vc::FindAbodeAt({59.9f, 0.0f, 65.0f}).abode == entt::null);
	EXPECT_TRUE(vc::FindAbodeAt({59.9f, 0.0f, 65.0f}).town == entt::null);
	// full (MaxVillagers 2 - 2 inhabitants == 0): no abode, its town kept; the search does not go on to the
	// older one in the same cell
	auto& registry = Locator::entitiesRegistry::value();
	auto& inhabitants = registry.Get<Abode>(newer).inhabitants;
	inhabitants.push_back(registry.Create());
	inhabitants.push_back(registry.Create());
	found = vc::FindAbodeAt({65.0f, 0.0f, 65.0f});
	EXPECT_TRUE(found.abode == entt::null);
	EXPECT_EQ(found.town, mine);
	// over-full is "room" to the inequality test (2 - 3 != 0)
	inhabitants.push_back(registry.Create());
	EXPECT_EQ(vc::FindAbodeAt({65.0f, 0.0f, 65.0f}).abode, newer);
	static_cast<void>(older);
	static_cast<void>(neutral);
	static_cast<void>(neutralAbode);
}

TEST_F(ScriptVillagerCommandsAbodes, FindVillagerInfoFirstMatch)
{
	// FindVillagerInfo: the FIRST record with the tribe and number, none -> no value
	auto& villagers = const_cast<InfoConstants&>(Locator::infoConstants::value()).villager;
	for (auto& v : villagers)
	{
		v.tribeType = Tribe::NORSE;
		v.villagerNumber = VillagerNumber::Trader;
	}
	villagers.at(9).tribeType = Tribe::CELTIC;
	villagers.at(9).villagerNumber = VillagerNumber::Fisherman;
	villagers.at(40).tribeType = Tribe::CELTIC;
	villagers.at(40).villagerNumber = VillagerNumber::Fisherman;
	EXPECT_EQ(vc::FindVillagerInfo(Tribe::CELTIC, VillagerNumber::Fisherman), static_cast<VillagerInfo>(9));
	EXPECT_EQ(vc::FindVillagerInfo(Tribe::NORSE, VillagerNumber::Trader), static_cast<VillagerInfo>(0));
	EXPECT_FALSE(vc::FindVillagerInfo(Tribe::AZTEC, VillagerNumber::Leader).has_value());
}

TEST_F(ScriptVillagerCommandsAbodes, FindAbodeAtNeutralLast)
{
	// no PLAYER_ONE abode in the cell: the neutral town's
	const auto neutral = MakeTown(2, PlayerNames::NEUTRAL, 1);
	MakeTown(1, PlayerNames::PLAYER_ONE, 2);
	const auto neutralAbode = MakeAbode({61.0f, 62.0f}, 2);
	MakeAbode({120.0f, 120.0f}, 1);
	const auto found = vc::FindAbodeAt({65.0f, 0.0f, 65.0f});
	EXPECT_EQ(found.abode, neutralAbode);
	EXPECT_EQ(found.town, neutral);
}
