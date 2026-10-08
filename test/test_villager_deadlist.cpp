/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The dead list: an animal marked by ecs::ToBeDeleted with the deferral on. animal_ai::Forget takes it off its town
// and its flock; the walk tags and DownedVillager go at mark time (inferred: marking takes it off the living list);
// the AI's readers (detail::Available, ecs::IsAvailable) no longer take it; ProcessDeadList(drain) frees it.
// The villager side: villager::IsAvailable (not unavailable, and the final state is not DYING) is what a predator
// asks of its prey.

#define LOCATOR_IMPLEMENTATIONS

#include <memory>

#include <entt/entity/entity.hpp>
#include <gtest/gtest.h>

#include "ECS/AnimalAI.h"
#include "ECS/AnimalAIDetail.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/AnimalBrain.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Unavailable.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WallHug.h"
#include "ECS/Registry.h"
#include "ECS/ToBeDeleted.h"
#include "ECS/Villager/VillagerScript.h"
#include "Enums.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "support/TestServices.h"
#include "support/WorldSystems.h"

using namespace openblack;
using namespace openblack::ecs::components;

namespace
{
// info.dat's villager state table: 14 DYING and 17 DOWNED are final,
// 239 PAUSE is not
constexpr auto k_Dying = VillagerStates::Dying;
constexpr auto k_Downed = VillagerStates::Downed;
constexpr auto k_Pause = static_cast<VillagerStates>(239);
} // namespace

class VillagerDeadListTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		test::EmplaceMapAndVillagerDefaults();
		auto info = std::make_unique<InfoConstants>();
		for (const auto state : {k_Dying, k_Downed})
		{
			info->villagerStateTable.at(static_cast<size_t>(state)).isFinalState = 1;
		}
		info->villagerStateTable.at(static_cast<size_t>(k_Pause)).isFinalState = 0;
		Locator::infoConstants::reset(info.release());
		Locator::entitiesRegistry::emplace<ecs::Registry>();
		openblack::test::EmplaceWorldSystems();
	}
	void TearDown() override
	{
		ecs::ProcessDeadList(true);
		ecs::SetDeferredDeletion(false);
		Locator::entitiesRegistry::reset();
		openblack::test::ResetWorldSystems();
		test::ResetMapAndVillagerDefaults();
		Locator::infoConstants::reset();
	}

	static ecs::Registry& Reg() { return Locator::entitiesRegistry::value(); }

	/// a villager in the map with TOP / FINAL states
	static entt::entity MakeVillager(VillagerStates top, VillagerStates final)
	{
		auto& registry = Reg();
		const auto e = registry.Create();
		registry.Assign<Villager>(e);
		registry.Assign<Transform>(e);
		auto& action = registry.Assign<LivingAction>(e, top, static_cast<uint16_t>(0));
		action.states.at(static_cast<size_t>(LivingAction::Index::Final)) = static_cast<uint8_t>(final);
		return e;
	}

	/// a lion hunting the target
	static entt::entity MakeLion(entt::entity target)
	{
		auto& registry = Reg();
		const auto e = registry.Create();
		registry.Assign<Animal>(e).type = AnimalInfo::Lion;
		registry.Assign<Transform>(e);
		registry.Assign<AnimalBrain>(e).target = target;
		return e;
	}

	/// an animal of a town, walking round an obstacle, with a DownedVillager (in the game that one is on the downed
	/// villager; here it only checks that Forget takes it)
	static entt::entity MakeAnimal(entt::entity town)
	{
		auto& registry = Reg();
		const auto e = registry.Create();
		auto& animal = registry.Assign<Animal>(e);
		animal.town = town;
		registry.Assign<Transform>(e);
		registry.Assign<DownedVillager>(e);
		registry.Assign<MoveStateLinearTag>(e);
		registry.Assign<WallHugObjectReference>(e, static_cast<uint8_t>(1), town);
		return e;
	}
};

TEST_F(VillagerDeadListTest, ForgetClearsTownWalkAndDowned)
{
	const auto town = Reg().Create();
	const auto e = MakeAnimal(town);
	ecs::animal_ai::Forget(e);
	EXPECT_EQ(Reg().Get<Animal>(e).town, entt::entity(entt::null));
	EXPECT_EQ(Reg().Get<Animal>(e).flock, entt::entity(entt::null));
	EXPECT_FALSE((Reg().AnyOf<DownedVillager, MoveStateLinearTag, WallHugObjectReference>(e)));
	EXPECT_TRUE(Reg().AllOf<Transform>(e));
}

TEST_F(VillagerDeadListTest, ZombieIsNotAvailable)
{
	const auto town = Reg().Create();
	const auto e = MakeAnimal(town);
	EXPECT_TRUE(ecs::animal_ai::detail::Available(e));
	ecs::SetDeferredDeletion(true);
	ecs::ToBeDeleted(e);
	// marked: still in the registry (1-2 passes), unlinked, and no reader takes it
	ASSERT_TRUE(Reg().Valid(e));
	EXPECT_TRUE(Reg().AllOf<Unavailable>(e));
	EXPECT_FALSE(ecs::IsAvailable(e));
	EXPECT_FALSE(ecs::animal_ai::detail::Available(e));
	EXPECT_EQ(Reg().Get<Animal>(e).town, entt::entity(entt::null));
	EXPECT_FALSE((Reg().AnyOf<DownedVillager, MoveStateLinearTag, WallHugObjectReference>(e)));
	// a second ToBeDeleted is nothing; the drain frees it
	ecs::ToBeDeleted(e);
	ecs::ProcessDeadList(true);
	EXPECT_FALSE(Reg().Valid(e));
}

TEST_F(VillagerDeadListTest, VillagerZombieIsNotAvailable)
{
	const auto e = MakeVillager(k_Pause, VillagerStates::InvalidState);
	EXPECT_TRUE(ecs::villager::IsAvailable(e));
	EXPECT_TRUE(ecs::animal_ai::detail::Available(e));
	// marked by ToBeDeleted: villager::IsAvailable delegates to ecs::IsAvailable
	Reg().Assign<Unavailable>(e);
	EXPECT_FALSE(ecs::villager::IsAvailable(e));
	EXPECT_FALSE(ecs::animal_ai::detail::Available(e));
}

TEST_F(VillagerDeadListTest, LionTargetsByTheOriginalsAvailability)
{
	// a predator asks villager::IsAvailable: the final state (TOP if final, else FINAL) != DYING
	const auto downed = MakeVillager(k_Downed, k_Dying); // DOWNED is final: still prey
	const auto dying = MakeVillager(k_Dying, VillagerStates::InvalidState);
	const auto pausedDying = MakeVillager(k_Pause, k_Dying); // a passing TOP over FINAL DYING: no prey
	const auto noAction = Reg().Create();
	Reg().Assign<Villager>(noAction);
	Reg().Assign<Transform>(noAction);
	auto available = [](entt::entity villager) {
		return ecs::animal_ai::detail::Available(Reg().Get<AnimalBrain>(MakeLion(villager)).target);
	};
	EXPECT_TRUE(available(downed));
	EXPECT_FALSE(available(dying));
	EXPECT_FALSE(available(pausedDying));
	EXPECT_FALSE(available(noAction)); // no LivingAction: not available (no FINAL state to read)
}
