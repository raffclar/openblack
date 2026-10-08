/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// town_queries::GetTown: the object's town by class. An abode's own, a scaffold's own,
// a totem statue's (its town centre's), a pot structure's (its structure's, when available) and none for the rest.

#define LOCATOR_IMPLEMENTATIONS

#include <gtest/gtest.h>

#include "ECS/Components/Abode.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/FishFarm.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/Scaffold.h"
#include "ECS/Components/StoragePit.h"
#include "ECS/Components/TotemStatue.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/Workshop.h"
#include "ECS/Registry.h"
#include "ECS/Town/TownQueries.h"
#include "Locator.h"
#include "support/WorldSystems.h"

using namespace openblack;
using namespace openblack::ecs::components;
namespace town_queries = openblack::ecs::town_queries;

namespace
{
class TownGetTownTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		Locator::entitiesRegistry::emplace<ecs::Registry>();
		openblack::test::EmplaceWorldSystems();
		_town = Reg().Create();
		Reg().Assign<Town>(_town).id = k_TownId;
		Reg().Context().towns[k_TownId] = _town;
	}
	void TearDown() override
	{
		Locator::entitiesRegistry::reset();
		openblack::test::ResetWorldSystems();
	}
	static ecs::Registry& Reg() { return Locator::entitiesRegistry::value(); }

	/// An abode of the town (through its town id)
	entt::entity MakeAbode()
	{
		const auto abode = Reg().Create();
		Reg().Assign<Abode>(abode).townId = k_TownId;
		return abode;
	}

	static constexpr uint32_t k_TownId = 7;
	entt::entity _town {entt::null};
};
} // namespace

TEST_F(TownGetTownTest, AnAbodeAnswersItsTown)
{
	EXPECT_EQ(town_queries::GetTown(MakeAbode()), _town);
}

TEST_F(TownGetTownTest, AScaffoldAnswersItsTownField)
{
	const auto scaffold = Reg().Create();
	Reg().Assign<Scaffold>(scaffold).town = _town;
	EXPECT_EQ(town_queries::GetTown(scaffold), _town);
}

TEST_F(TownGetTownTest, ATotemAnswersItsTownCentresTown)
{
	const auto totem = Reg().Create();
	Reg().Assign<TotemStatue>(totem).townCentre = MakeAbode();
	EXPECT_EQ(town_queries::GetTown(totem), _town);
	// no town centre, 0
	Reg().Get<TotemStatue>(totem).townCentre = entt::null;
	EXPECT_TRUE(town_queries::GetTown(totem) == entt::null);
}

TEST_F(TownGetTownTest, AStoragePitsPileAnswersThePitsTown)
{
	const auto pit = MakeAbode();
	const auto pile = Reg().Create();
	Reg().Assign<Pot>(pile);
	Reg().Assign<StoragePit>(pit).foodPile = pile;
	EXPECT_EQ(town_queries::GetTown(pile), _town);
	// a pile of no structure: not part of a structure, GetTown 0
	const auto loose = Reg().Create();
	Reg().Assign<Pot>(loose);
	EXPECT_TRUE(town_queries::GetTown(loose) == entt::null);
}

TEST_F(TownGetTownTest, AFieldAnswersItsOwnTownBeforeTheAbodes)
{
	// a field's town overrides the abode's: the field's town id wins
	const auto field = Reg().Create();
	Reg().Assign<Abode>(field).townId = 99;
	Reg().Assign<Field>(field).town = static_cast<int>(k_TownId);
	EXPECT_EQ(town_queries::GetTown(field), _town);
}

TEST_F(TownGetTownTest, AFishFarmAndAVillagerAnswerTheirTownFields)
{
	const auto farm = Reg().Create();
	Reg().Assign<FishFarm>(farm).town = _town;
	EXPECT_EQ(town_queries::GetTown(farm), _town);
	const auto villager = Reg().Create();
	Reg().Assign<Villager>(villager).town = _town;
	EXPECT_EQ(town_queries::GetTown(villager), _town);
}

TEST_F(TownGetTownTest, AWorkshopsPileAnswersTheWorkshopsTown)
{
	const auto workshop = MakeAbode();
	const auto pile = Reg().Create();
	Reg().Assign<Pot>(pile);
	Reg().Assign<Workshop>(workshop).woodPile = pile;
	EXPECT_EQ(town_queries::GetTown(pile), _town);
}

TEST_F(TownGetTownTest, AnyOtherObjectAnswersNone)
{
	// animals, trees, mobile objects: none
	const auto object = Reg().Create();
	Reg().Assign<Transform>(object);
	EXPECT_TRUE(town_queries::GetTown(object) == entt::null);
	EXPECT_TRUE(town_queries::GetTown(entt::null) == entt::null);
}
