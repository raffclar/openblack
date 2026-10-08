/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The intro opcodes: the Flock class (ECS/Flocks.h: the member order by each Living's flock order and the leader,
// the tail; Remove, Separate, Merge, RandomMember, PosWithinDomain) and the script containers
// (ECS/ScriptContainers.h: ID_SIZE, FLOCK_DETACH, CHANGE_INNER_OUTER).

#define LOCATOR_IMPLEMENTATIONS

#include <cstdint>

#include <memory>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#include "Common/GameRandomTesting.h"
#include "ECS/AnimalAI.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/Flock.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Flocks.h"
#include "ECS/Registry.h"
#include "ECS/ScriptContainers.h"
#include "ECS/ScriptHeld.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "support/TestServices.h"
#include "support/WorldSystems.h"

using namespace openblack;
using namespace openblack::ecs::components;
namespace flocks = openblack::ecs::flocks;
namespace containers = openblack::ecs::script_containers;

namespace
{
class FlocksTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		test::EmplaceMapAndVillagerDefaults();
		// the scripts' logger (ScriptContainers / ScriptHeld log their errors there): a null sink, as in
		// test_interface_interaction; without one SPDLOG_LOGGER_ERROR dereferences a null logger
		if (!spdlog::get("scripting"))
		{
			spdlog::create<spdlog::sinks::null_sink_mt>("scripting");
		}
		Locator::infoConstants::reset(std::make_unique<InfoConstants>().release());
		Locator::entitiesRegistry::emplace<ecs::Registry>();
		openblack::test::EmplaceWorldSystems();
		// entity 0 is "no thing" for a script id (a script id of 0 finds nothing), as the game has made
		// other things before: the test's first entity must not be a scripted one
		Locator::entitiesRegistry::value().Create();
	}

	void TearDown() override
	{
		game_random::testing::SetGameRand({}, {});
		Locator::entitiesRegistry::reset();
		openblack::test::ResetWorldSystems();
		test::ResetMapAndVillagerDefaults();
		Locator::infoConstants::reset();
	}

	static ecs::Registry& Reg() { return Locator::entitiesRegistry::value(); }

	static entt::entity MakeVillager(glm::vec3 position)
	{
		const auto e = Reg().Create();
		auto& v = Reg().Assign<Villager>(e);
		v.town = entt::null;
		v.abode = entt::null;
		Reg().Assign<Transform>(e, position, glm::mat3(1.0f), glm::vec3(1.0f));
		return e;
	}

	static std::vector<entt::entity> Members(entt::entity flock) { return Reg().Get<const Flock>(flock).members; }
	static uint32_t Id(entt::entity e) { return static_cast<uint32_t>(e); }
};
} // namespace

TEST_F(FlocksTest, CreateIsTheScriptFlock)
{
	const auto f = flocks::Create({10.0f, 2.0f, 20.0f}, flocks::k_ScriptFlockId);
	const auto& flock = Reg().Get<const Flock>(f);
	EXPECT_EQ(flock.id, 0xABA52);
	EXPECT_EQ(flock.domainRadius, 0x50);
	EXPECT_EQ(flock.flockDistance, 0x1E);
	EXPECT_TRUE(flock.domainCentre == glm::vec3(10.0f, 2.0f, 20.0f));
	EXPECT_TRUE(flock.savedDomainCentre == flock.domainCentre);
	EXPECT_EQ(flock.maxMembers, 0u);
	EXPECT_TRUE(flocks::Leader(f) == entt::null);
	// no tail: GetFlockPos is the flock's domain centre
	EXPECT_TRUE(flocks::GetFlockPos(f) == glm::vec3(10.0f, 2.0f, 20.0f));
}

TEST_F(FlocksTest, MembersGoBeforeTheHeadAndTheFirstIsTheLeader)
{
	const auto f = flocks::Create({}, flocks::k_ScriptFlockId);
	const auto a = MakeVillager({1.0f, 0.0f, 1.0f});
	const auto b = MakeVillager({2.0f, 0.0f, 2.0f});
	const auto c = MakeVillager({3.0f, 0.0f, 3.0f});
	EXPECT_TRUE(flocks::AddMember(f, a));
	EXPECT_TRUE(flocks::AddMember(f, b));
	EXPECT_TRUE(flocks::AddMember(f, c));
	// order 0 each: every one before the head; the list head..tail is c, b, a (members reversed: a, b, c)
	EXPECT_EQ(Members(f), (std::vector<entt::entity> {a, b, c}));
	EXPECT_EQ(flocks::MembersFromHead(f), (std::vector<entt::entity> {c, b, a}));
	EXPECT_TRUE(flocks::Leader(f) == a);
	EXPECT_TRUE(flocks::IsLeader(a));
	EXPECT_FALSE(flocks::IsLeader(b));
	EXPECT_TRUE(flocks::FlockOf(b) == f);
	EXPECT_EQ(flocks::Size(f), 3u);
	EXPECT_TRUE(flocks::GetFlockPos(f) == glm::vec3(1.0f, 0.0f, 1.0f));
	// AddMember of a member: it leaves its flock first and comes back as the head
	EXPECT_TRUE(flocks::AddMember(f, a));
	EXPECT_EQ(Members(f), (std::vector<entt::entity> {b, c, a}));
	EXPECT_TRUE(flocks::Leader(f) == b);
}

TEST_F(FlocksTest, AddLeaderTakesTheTail)
{
	const auto f = flocks::Create({}, flocks::k_ScriptFlockId);
	const auto a = MakeVillager({});
	const auto b = MakeVillager({});
	const auto father = MakeVillager({});
	// an empty flock: order 5
	flocks::AddLeader(f, father);
	EXPECT_EQ(flocks::OrderOf(father), 5);
	EXPECT_TRUE(flocks::Leader(f) == father);
	// the next members (0) go before the head: the leader stays the tail
	flocks::AddMember(f, a);
	flocks::AddMember(f, b);
	EXPECT_EQ(flocks::MembersFromHead(f), (std::vector<entt::entity> {b, a, father}));
	EXPECT_TRUE(flocks::Leader(f) == father);
	// a member made leader: the tail's + 1, moved to the tail
	flocks::AddLeader(f, a);
	EXPECT_EQ(flocks::OrderOf(a), 6);
	EXPECT_EQ(flocks::MembersFromHead(f), (std::vector<entt::entity> {b, father, a}));
	EXPECT_TRUE(flocks::Leader(f) == a);
	// the tail itself: nothing
	flocks::AddLeader(f, a);
	EXPECT_EQ(flocks::OrderOf(a), 6);
}

TEST_F(FlocksTest, RemoveAndDelete)
{
	const auto f = flocks::Create({}, flocks::k_ScriptFlockId);
	const auto a = MakeVillager({});
	const auto b = MakeVillager({});
	flocks::AddMember(f, a);
	flocks::AddMember(f, b);
	// 0: an emptied flock is kept
	EXPECT_TRUE(flocks::RemoveLiving(f, a, false));
	EXPECT_TRUE(flocks::FlockOf(a) == entt::null);
	EXPECT_FALSE(flocks::RemoveLiving(f, a, false)); // not a member any more
	EXPECT_TRUE(flocks::RemoveLiving(f, b, false));
	EXPECT_TRUE(Reg().Valid(f));
	// 1: deleted with its last member
	flocks::AddMember(f, a);
	EXPECT_TRUE(flocks::RemoveLiving(f, a, true));
	EXPECT_FALSE(Reg().Valid(f));
}

TEST_F(FlocksTest, JoiningAnotherFlockDeletesTheEmptiedOne)
{
	const auto first = flocks::Create({}, flocks::k_ScriptFlockId);
	const auto second = flocks::Create({}, flocks::k_ScriptFlockId);
	const auto a = MakeVillager({});
	flocks::AddMember(first, a);
	EXPECT_TRUE(flocks::AddMember(second, a));
	EXPECT_FALSE(Reg().Valid(first)); // leaving it deletes the emptied flock
	EXPECT_TRUE(flocks::FlockOf(a) == second);
}

TEST_F(FlocksTest, SeparateKeepsTheDomain)
{
	const auto f = flocks::Create({}, flocks::k_ScriptFlockId);
	Reg().Get<Flock>(f).domainRadius = 10;
	Reg().Get<Flock>(f).flockDistance = 2;
	const auto a = MakeVillager({5.0f, 0.0f, 6.0f});
	const auto b = MakeVillager({});
	flocks::AddMember(f, a);
	flocks::AddMember(f, b);
	const auto own = flocks::SeparateIntoNewFlock(f, a, false);
	EXPECT_TRUE(flocks::FlockOf(a) == own);
	EXPECT_EQ(Reg().Get<const Flock>(own).domainRadius, 10);
	EXPECT_EQ(Reg().Get<const Flock>(own).flockDistance, 2);
	EXPECT_EQ(Reg().Get<const Flock>(own).id, -1);
	EXPECT_TRUE(Reg().Get<const Flock>(own).domainCentre == glm::vec3(5.0f, 0.0f, 6.0f));
	EXPECT_EQ(Members(f), (std::vector<entt::entity> {b}));
}

TEST_F(FlocksTest, MergeTakesTheOthersFromItsHead)
{
	const auto keeper = flocks::Create({}, flocks::k_ScriptFlockId);
	const auto other = flocks::Create({}, flocks::k_ScriptFlockId);
	const auto a = MakeVillager({});
	const auto b = MakeVillager({});
	const auto c = MakeVillager({});
	flocks::AddMember(keeper, a);
	flocks::AddMember(other, b);
	flocks::AddMember(other, c); // other: head c, tail b
	Reg().Get<Flock>(keeper).maxMembers = 1;
	Reg().Get<Flock>(other).maxMembers = 2;
	flocks::Merge(keeper, other);
	// c first (the other's head), then b: each before the keeper's head
	EXPECT_EQ(flocks::MembersFromHead(keeper), (std::vector<entt::entity> {b, c, a}));
	EXPECT_FALSE(Reg().Valid(other));
	// maxMembers += the other's at each member (no animal: no clamp)
	EXPECT_EQ(Reg().Get<const Flock>(keeper).maxMembers, 5u);
}

TEST_F(FlocksTest, RandomMemberSkipsTheExcluded)
{
	const auto f = flocks::Create({}, flocks::k_ScriptFlockId);
	const auto a = MakeVillager({});
	const auto b = MakeVillager({});
	const auto c = MakeVillager({});
	flocks::AddMember(f, a);
	flocks::AddMember(f, b);
	flocks::AddMember(f, c); // head c, b, tail a
	std::vector<uint32_t> ranges;
	uint32_t next = 0;
	game_random::testing::SetGameRand(
	    [&](uint32_t n) {
		    ranges.push_back(n);
		    return next;
	    },
	    [](float) { return 0.0f; });
	// exclude the tail with 3 members: GameRand(2)
	next = 1;
	EXPECT_TRUE(flocks::RandomMember(f, a) == b);
	EXPECT_EQ(ranges.back(), 2u);
	// r past the others: the excluded one is skipped, nothing after it
	next = 2;
	EXPECT_TRUE(flocks::RandomMember(f, a) == entt::null);
	// no exclude: GameRand(3)
	next = 0;
	EXPECT_TRUE(flocks::RandomMember(f, entt::null) == c);
	EXPECT_EQ(ranges.back(), 3u);
}

TEST_F(FlocksTest, PosWithinDomain)
{
	const auto a = MakeVillager({});
	// no flock: 0
	EXPECT_FALSE(flocks::PosWithinDomain(a, {0.0f, 0.0f}, 1.0f));
	const auto f = flocks::Create({100.0f, 0.0f, 100.0f}, flocks::k_ScriptFlockId);
	Reg().Get<Flock>(f).domainRadius = 3;
	flocks::AddMember(f, a);
	EXPECT_TRUE(flocks::PosWithinDomain(a, {102.0f, 100.0f}, 1.0f));
	// (no exact edge case: GetDistanceInMetres is the table distance, 3 m may come out a hair above 3)
	EXPECT_TRUE(flocks::PosWithinDomain(a, {102.9f, 100.0f}, 1.0f));
	EXPECT_FALSE(flocks::PosWithinDomain(a, {104.0f, 100.0f}, 1.0f));
	EXPECT_TRUE(flocks::PosWithinDomain(a, {104.0f, 100.0f}, 2.0f));
}

TEST_F(FlocksTest, ContainersSizeDetachAndInnerOuter)
{
	const auto f = containers::CreateFlock({50.0f, 0.0f, 50.0f});
	EXPECT_TRUE(containers::IsContainer(f));
	// a slot, controlled by the script only from its first reference
	EXPECT_FALSE(ecs::script_held::IsControlledByScript(f));
	const auto a = MakeVillager({});
	const auto b = MakeVillager({});
	flocks::AddMember(f, a);
	flocks::AddMember(f, b);
	EXPECT_EQ(containers::Size(Id(f)).value_or(-1.0f), 2.0f);
	EXPECT_FALSE(containers::Size(Id(a)).has_value());
	// CHANGE_INNER_OUTER (1, 3, 0): distance 1, radius 3; 0 keeps a field
	containers::ChangeInnerOuter(Id(f), 1.0f, 3.0f, 0.0f);
	EXPECT_EQ(Reg().Get<const Flock>(f).flockDistance, 1);
	EXPECT_EQ(Reg().Get<const Flock>(f).domainRadius, 3);
	containers::ChangeInnerOuter(Id(f), 0.0f, 10.0f, 2.9f);
	EXPECT_EQ(Reg().Get<const Flock>(f).flockDistance, 1);
	EXPECT_EQ(Reg().Get<const Flock>(f).domainRadius, 10);
	EXPECT_EQ(Reg().Get<const Flock>(f).calm, 2);
	// FLOCK_DETACH(a, flock): a villager is only removed, its id pushed; the emptied flock is kept
	EXPECT_EQ(containers::Detach(Id(f), Id(a)), Id(a));
	EXPECT_TRUE(flocks::FlockOf(a) == entt::null);
	EXPECT_EQ(containers::Size(Id(f)).value_or(-1.0f), 1.0f);
	// a Living as the container stands for its flock
	EXPECT_EQ(containers::Detach(Id(b), Id(b)), Id(b));
	EXPECT_EQ(containers::Size(Id(f)).value_or(-1.0f), 0.0f);
	EXPECT_TRUE(Reg().Valid(f));
	// a dead container: 0
	EXPECT_EQ(containers::Detach(0, Id(b)), 0u);
}

TEST_F(FlocksTest, DetachWithoutAnObjPicksAMemberButTheLeader)
{
	const auto f = containers::CreateFlock({});
	const auto a = MakeVillager({});
	const auto b = MakeVillager({});
	flocks::AddMember(f, a);
	flocks::AddMember(f, b); // head b, tail a
	game_random::testing::SetGameRand([](uint32_t) { return 0u; }, [](float) { return 0.0f; });
	EXPECT_EQ(containers::Detach(Id(f), 0), Id(b));
	EXPECT_TRUE(flocks::Leader(f) == a);
	// one member left: no exclude, the leader goes too
	EXPECT_EQ(containers::Detach(Id(f), 0), Id(a));
	EXPECT_EQ(containers::Detach(Id(f), 0), 0u);
}

TEST_F(FlocksTest, CreateKeepsTheInfoAndThePlayer)
{
	// a flock made with its info and player keeps both; the rest as Create(pos, id)
	const auto f =
	    flocks::Create({1.0f, 0.0f, 2.0f}, flocks::k_DefaultFlockInfo, PlayerNames::PLAYER_TWO, flocks::k_ScriptFlockId);
	const auto& flock = Reg().Get<const Flock>(f);
	ASSERT_TRUE(flock.info.has_value());
	EXPECT_EQ(*flock.info, 0u);
	ASSERT_TRUE(flock.player.has_value());
	EXPECT_EQ(*flock.player, PlayerNames::PLAYER_TWO);
	EXPECT_EQ(flock.id, 0xABA52);
	EXPECT_EQ(flock.domainRadius, 0x50);
	// a flock made from a Living, and Create(pos, id): none
	const auto g = flocks::Create({1.0f, 0.0f, 2.0f}, -1);
	EXPECT_FALSE(Reg().Get<const Flock>(g).info.has_value());
	EXPECT_FALSE(Reg().Get<const Flock>(g).player.has_value());
}

TEST_F(FlocksTest, AnimalSetTownListsAtTheHeadAndForgetUnlinks)
{
	const auto town = Reg().Create();
	Reg().Assign<Town>(town, 3u);
	const auto other = Reg().Create();
	Reg().Assign<Town>(other, 4u);
	const auto MakeAnimal = [] {
		const auto e = Reg().Create();
		Reg().Assign<Animal>(e);
		Reg().Assign<Transform>(e, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
		return e;
	};
	const auto a = MakeAnimal();
	const auto b = MakeAnimal();
	// SetTown: the animal's town set, at the head of the town's animal list
	ecs::animal_ai::SetTown(a, town);
	ecs::animal_ai::SetTown(b, town);
	EXPECT_EQ(Reg().Get<const Town>(town).animals, std::vector<entt::entity>({b, a}));
	EXPECT_EQ(Reg().Get<const Animal>(a).town, town);
	// literal: a new town does not take it off the old list
	ecs::animal_ai::SetTown(a, other);
	EXPECT_EQ(Reg().Get<const Animal>(a).town, other);
	EXPECT_EQ(Reg().Get<const Town>(other).animals, std::vector<entt::entity>({a}));
	EXPECT_EQ(Reg().Get<const Town>(town).animals, std::vector<entt::entity>({b, a}));
	// no town: none and no list
	ecs::animal_ai::SetTown(b, entt::null);
	EXPECT_TRUE(Reg().Get<const Animal>(b).town == entt::null);
	// Forget: off the list of its own town only
	ecs::animal_ai::Forget(a);
	EXPECT_TRUE(Reg().Get<const Town>(other).animals.empty());
	EXPECT_EQ(Reg().Get<const Town>(town).animals, std::vector<entt::entity>({b, a}));
	EXPECT_TRUE(Reg().Get<const Animal>(a).town == entt::null);
}
