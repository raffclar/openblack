/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>

#include <algorithm>
#include <limits>
#include <vector>

#include <gtest/gtest.h>

#include "ECS/Components/Abode.h"
#include "ECS/Components/Disciple.h"
#include "ECS/Components/Fire.h"
#include "ECS/Components/Indestructible.h"
#include "ECS/Components/SpellDispenser.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Villager.h"
#include "ECS/Registry.h"
#include "ECS/TownCensus.h"
#include "ECS/WorldObjects.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;

namespace
{

/// The things of a town that a walk of the whole world finds, in the order of the walk
template <typename Component, typename Test>
std::vector<entt::entity> Walked(const Registry& registry, Test test)
{
	std::vector<entt::entity> found;
	registry.Each<const Component>([&](entt::entity entity, const Component& component) {
		if (test(component))
		{
			found.push_back(entity);
		}
	});
	return found;
}

std::vector<entt::entity> People(const Registry& registry, entt::entity town)
{
	const auto list = town_census::PeopleOf(registry, town);
	return {list.begin(), list.end()};
}

std::vector<entt::entity> Abodes(const Registry& registry, uint32_t number)
{
	const auto list = town_census::AbodesOf(registry, number);
	return {list.begin(), list.end()};
}

std::vector<entt::entity> Dispensers(const Registry& registry, entt::entity town)
{
	const auto list = town_census::DispensersOf(registry, town);
	return {list.begin(), list.end()};
}

entt::entity MakeTown(Registry& registry, uint32_t number)
{
	const auto town = registry.Create();
	registry.Assign<Town>(town, number, PlayerNames::NEUTRAL);
	return town;
}

entt::entity MakeVillager(Registry& registry, entt::entity town)
{
	const auto villager = registry.Create();
	registry.Assign<Villager>(villager).town = town;
	registry.TownMembersChanged();
	return villager;
}

entt::entity MakeAbode(Registry& registry, uint32_t number, AbodeNumber type)
{
	const auto abode = registry.Create();
	auto& data = registry.Assign<Abode>(abode);
	data.type = type;
	data.townId = number;
	return abode;
}

void ExpectSameAsWalk(const Registry& registry, entt::entity town, uint32_t number)
{
	EXPECT_EQ(People(registry, town),
	          Walked<Villager>(registry, [town](const Villager& villager) { return villager.town == town; }));
	EXPECT_EQ(Abodes(registry, number),
	          Walked<Abode>(registry, [number](const Abode& abode) { return abode.townId == number; }));
	EXPECT_EQ(Dispensers(registry, town),
	          Walked<SpellDispenser>(registry, [town](const SpellDispenser& dispenser) { return dispenser.town == town; }));
}

} // namespace

TEST(TownCensus, TheListsFollowJoinsLeavesBirthsAndDeaths)
{
	Registry registry;
	const auto first = MakeTown(registry, 1);
	const auto second = MakeTown(registry, 2);
	std::vector<entt::entity> people;
	for (int i = 0; i < 6; ++i)
	{
		people.push_back(MakeVillager(registry, i % 2 == 0 ? first : second));
	}
	ExpectSameAsWalk(registry, first, 1);
	ExpectSameAsWalk(registry, second, 2);
	EXPECT_EQ(People(registry, first).size(), 3u);

	// A villager leaves its town and joins the other
	registry.Get<Villager>(people[0]).town = second;
	registry.TownMembersChanged();
	ExpectSameAsWalk(registry, first, 1);
	ExpectSameAsWalk(registry, second, 2);
	EXPECT_EQ(People(registry, second).size(), 4u);

	// A villager dies, and a child is born
	registry.Destroy(people[3]);
	MakeVillager(registry, first);
	ExpectSameAsWalk(registry, first, 1);
	ExpectSameAsWalk(registry, second, 2);

	// A villager with no town is in no town's list
	MakeVillager(registry, entt::null);
	ExpectSameAsWalk(registry, first, 1);
	EXPECT_EQ(People(registry, entt::null).size(), 1u);
}

TEST(TownCensus, TheAbodesKeepTheOrderOfTheWalkSoTheLastStoragePitStaysLast)
{
	Registry registry;
	const auto town = MakeTown(registry, 7);
	const auto pitA = MakeAbode(registry, 7, AbodeNumber::StoragePit);
	MakeAbode(registry, 8, AbodeNumber::StoragePit);
	const auto pitB = MakeAbode(registry, 7, AbodeNumber::StoragePit);
	ExpectSameAsWalk(registry, town, 7);
	// A pit that goes changes the order of the walk; the list follows it
	registry.Destroy(pitA);
	MakeAbode(registry, 7, AbodeNumber::StoragePit);
	ExpectSameAsWalk(registry, town, 7);
	const auto pits = Abodes(registry, 7);
	EXPECT_EQ(pits.size(), 2u);
	EXPECT_TRUE(std::ranges::find(pits, pitB) != pits.end());
}

TEST(TownCensus, TheDispensersFollowTheirTown)
{
	Registry registry;
	const auto town = MakeTown(registry, 3);
	const auto dispenser = registry.Create();
	registry.Assign<SpellDispenser>(dispenser).town = town;
	registry.TownMembersChanged();
	ExpectSameAsWalk(registry, town, 3);
	EXPECT_EQ(Dispensers(registry, town).size(), 1u);
	registry.Get<SpellDispenser>(dispenser).town = entt::null;
	registry.TownMembersChanged();
	EXPECT_TRUE(Dispensers(registry, town).empty());
}

TEST(TownCensus, ATownByItsNumberIsTheLastTownOfTheWalkWithIt)
{
	Registry registry;
	EXPECT_EQ(town_census::TownByNumber(registry, 4), static_cast<entt::entity>(entt::null));
	const auto first = MakeTown(registry, 4);
	EXPECT_EQ(town_census::TownByNumber(registry, 4), first);
	// Two towns with one number: the walk keeps the last
	const auto twin = MakeTown(registry, 4);
	entt::entity walked = entt::null;
	registry.Each<const Town>([&walked](entt::entity entity, const Town& town) {
		if (town.id == 4)
		{
			walked = entity;
		}
	});
	EXPECT_EQ(town_census::TownByNumber(registry, 4), walked);
	registry.Destroy(twin);
	EXPECT_EQ(town_census::TownByNumber(registry, 4), first);
	// A number kept as a signed value reads as the same number
	const auto high = MakeTown(registry, 0x80000001u);
	EXPECT_EQ(town_census::TownBySignedNumber(registry, static_cast<int32_t>(0x80000001u)), high);
	EXPECT_EQ(town_census::TownBySignedNumber(registry, -1), static_cast<entt::entity>(entt::null));
	// The registry empties, and the lists with it
	registry.Reset();
	EXPECT_EQ(town_census::TownByNumber(registry, 4), static_cast<entt::entity>(entt::null));
	EXPECT_TRUE(People(registry, first).empty());
}

class VillagerLife: public ::testing::Test
{
protected:
	void SetUp() override { Locator::entitiesRegistry::emplace<ecs::Registry>(); }
	void TearDown() override { Locator::entitiesRegistry::reset(); }
	static Registry& Entities() { return Locator::entitiesRegistry::value(); }
	static entt::entity Person(float life)
	{
		auto& registry = Entities();
		const auto villager = registry.Create();
		auto& person = registry.Assign<Villager>(villager);
		person.life = life;
		person.town = entt::null;
		return villager;
	}
};

TEST_F(VillagerLife, HarmTakesTheLifeOfAVillagerDownToNothing)
{
	const auto villager = Person(0.75f);
	EXPECT_FLOAT_EQ(world_objects::LifeOf(villager), 0.75f);
	EXPECT_FLOAT_EQ(world_objects::ReduceLife(villager, 0.25f), 0.5f);
	EXPECT_FLOAT_EQ(world_objects::ReduceLife(villager, 0.0f), 0.5f);
	EXPECT_FLOAT_EQ(world_objects::ReduceLife(villager, 2.0f), 0.0f);
	EXPECT_FLOAT_EQ(Entities().Get<const Villager>(villager).life, 0.0f);
}

TEST_F(VillagerLife, HarmNeverBringsAnIndestructibleVillagerToNearlyNoLife)
{
	const auto villager = Person(0.5f);
	Entities().Assign<Indestructible>(villager);
	EXPECT_FLOAT_EQ(world_objects::ReduceLife(villager, 0.25f), 0.25f);
	EXPECT_FLOAT_EQ(world_objects::ReduceLife(villager, 1.0f), 0.25f);
}

TEST_F(VillagerLife, ALifeThatIsNotANumberStaysSo)
{
	const auto villager = Person(std::numeric_limits<float>::quiet_NaN());
	EXPECT_TRUE(std::isnan(world_objects::ReduceLife(villager, 0.1f)));
	EXPECT_TRUE(std::isnan(world_objects::LifeOf(villager)));
}

TEST_F(VillagerLife, AVillagerWithALifeOfAThingFollowsTheGeneralRule)
{
	// The life of the thing comes before the life of the villager. The harm still goes to the villager's own life.
	const auto villager = Person(0.75f);
	Entities().Assign<ObjectLife>(villager, ObjectLife {.life = 0.5f});
	EXPECT_FLOAT_EQ(world_objects::LifeOf(villager), 0.5f);
	EXPECT_FLOAT_EQ(world_objects::ReduceLife(villager, 0.25f), 0.5f);
	EXPECT_FLOAT_EQ(Entities().Get<const Villager>(villager).life, 0.5f);
}

TEST_F(VillagerLife, AnInvalidThingHasNoLife)
{
	const auto villager = Person(1.0f);
	Entities().Destroy(villager);
	EXPECT_FLOAT_EQ(world_objects::LifeOf(villager), 0.0f);
	EXPECT_FLOAT_EQ(world_objects::ReduceLife(villager, 0.25f), 0.0f);
}

namespace
{
/// The count of a town as a walk of all villagers gives it
town_census::TownCount WalkedCount(const Registry& registry, entt::entity town)
{
	town_census::TownCount count;
	registry.Each<const Villager>([&](entt::entity entity, const Villager& villager) {
		if (villager.town != town)
		{
			return;
		}
		++count.people;
		if (villager.lifeStage != Villager::LifeStage::Child)
		{
			++count.adults;
		}
		if (const auto* disciple = registry.TryGet<const Disciple>(entity); disciple != nullptr)
		{
			++count.disciples.at(static_cast<size_t>(disciple->type));
		}
	});
	return count;
}

entt::entity MakeCountedVillager(Registry& registry, entt::entity town, Villager::LifeStage stage)
{
	const auto villager = registry.Create();
	auto& person = registry.Assign<Villager>(villager);
	person.lifeStage = stage;
	person.town = town;
	registry.TownMembersChanged(villager);
	return villager;
}

/// A reader of the count in the signal of a villager that goes, as the town reads it when its villager goes
struct CountInSignal
{
	entt::entity town {entt::null};
	std::vector<uint32_t> people;
	std::vector<uint32_t> walked;
	void Gone(entt::registry& /*unused*/, entt::entity /*unused*/)
	{
		const auto& registry = *registryOf;
		people.push_back(town_census::CountOf(registry, town).people);
		walked.push_back(WalkedCount(registry, town).people);
	}
	const Registry* registryOf {nullptr};
};
} // namespace

TEST(TownCensus, TheCountsFollowJoinsGrowthDisciplesAndDeaths)
{
	Registry registry;
	const auto first = MakeTown(registry, 1);
	const auto second = MakeTown(registry, 2);
	std::vector<entt::entity> people;
	for (int i = 0; i < 6; ++i)
	{
		people.push_back(MakeCountedVillager(registry, i % 2 == 0 ? first : second,
		                                     i < 3 ? Villager::LifeStage::Adult : Villager::LifeStage::Child));
	}
	EXPECT_EQ(town_census::CountOf(registry, first), WalkedCount(registry, first));
	EXPECT_EQ(town_census::CountOf(registry, first).people, 3u);
	EXPECT_EQ(town_census::CountOf(registry, first).adults, 2u);

	// A villager goes to the other town, and a child grows up
	registry.Get<Villager>(people[0]).town = second;
	registry.TownMembersChanged(people[0]);
	registry.Get<Villager>(people[4]).lifeStage = Villager::LifeStage::Adult;
	town_census::VillagerChanged(registry, people[4]);
	EXPECT_EQ(town_census::CountOf(registry, first), WalkedCount(registry, first));
	EXPECT_EQ(town_census::CountOf(registry, second), WalkedCount(registry, second));
	EXPECT_EQ(town_census::CountOf(registry, second).people, 4u);

	// Villagers become disciples, change their work and stop it
	registry.Assign<Disciple>(people[1], Disciple {.type = VillagerDisciple::Builder});
	registry.Assign<Disciple>(people[3], Disciple {.type = VillagerDisciple::Builder});
	registry.AssignOrReplace<Disciple>(people[3], Disciple {.type = VillagerDisciple::Breeder});
	EXPECT_EQ(town_census::CountOf(registry, second).disciples.at(static_cast<size_t>(VillagerDisciple::Builder)), 1u);
	EXPECT_EQ(town_census::CountOf(registry, second).disciples.at(static_cast<size_t>(VillagerDisciple::Breeder)), 1u);
	registry.Remove<Disciple>(people[3]);
	EXPECT_EQ(town_census::CountOf(registry, second), WalkedCount(registry, second));

	// A disciple dies, and a villager with no town comes
	registry.Destroy(people[1]);
	MakeCountedVillager(registry, entt::null, Villager::LifeStage::Adult);
	EXPECT_EQ(town_census::CountOf(registry, second), WalkedCount(registry, second));
	EXPECT_EQ(town_census::CountOf(registry, entt::null).people, 1u);
	EXPECT_TRUE(town_census::CountsMatchWalk(registry));

	// A change that the code does not tell of by villager makes all counts come again from a walk
	registry.Get<Villager>(people[2]).town = second;
	registry.TownMembersChanged();
	EXPECT_EQ(town_census::CountOf(registry, first), WalkedCount(registry, first));
	EXPECT_TRUE(town_census::CountsMatchWalk(registry));
}

TEST(TownCensus, AVillagerThatGoesCountsUntilItsSignalEnds)
{
	Registry registry;
	const auto town = MakeTown(registry, 3);
	std::vector<entt::entity> people;
	for (int i = 0; i < 4; ++i)
	{
		people.push_back(MakeCountedVillager(registry, town, Villager::LifeStage::Adult));
	}
	registry.Assign<Disciple>(people[2], Disciple {.type = VillagerDisciple::Farmer});
	CountInSignal reader {.town = town, .registryOf = &registry};
	registry.Underlying().on_destroy<Villager>().connect<&CountInSignal::Gone>(reader);
	// A death wave: each read in a signal sees the villager that goes, as the walk does
	for (const auto villager : people)
	{
		registry.Destroy(villager);
	}
	EXPECT_EQ(reader.people, reader.walked);
	EXPECT_EQ(reader.people, (std::vector<uint32_t> {4, 3, 2, 1}));
	EXPECT_EQ(town_census::CountOf(registry, town), town_census::TownCount {});
	EXPECT_TRUE(town_census::CountsMatchWalk(registry));
}

TEST(TownCensus, TheCountsComeAgainAfterAReset)
{
	Registry registry;
	auto town = MakeTown(registry, 4);
	MakeCountedVillager(registry, town, Villager::LifeStage::Adult);
	EXPECT_EQ(town_census::CountOf(registry, town).people, 1u);
	registry.Reset();
	town = MakeTown(registry, 4);
	EXPECT_EQ(town_census::CountOf(registry, town).people, 0u);
	MakeCountedVillager(registry, town, Villager::LifeStage::Child);
	MakeCountedVillager(registry, town, Villager::LifeStage::Child);
	EXPECT_EQ(town_census::CountOf(registry, town), WalkedCount(registry, town));
	EXPECT_EQ(town_census::CountOf(registry, town).adults, 0u);
	EXPECT_TRUE(town_census::CountsMatchWalk(registry));
}
