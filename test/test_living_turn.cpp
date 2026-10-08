/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The one living list (newest first), the walk only in the state functions that walk, and InitStepsXZ in integers.
// Every expected value is the original's: the list rules from its insert, unlink and loop, the walking rows from its
// state table, and the steps from its sine and cosine tables and constants.

#include <cstdint>

#include <algorithm>
#include <array>
#include <bit>
#include <memory>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#include "3D/ObjectMatrix.h"
#include "Common/GUtilsAngle.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Unavailable.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WallHug.h"
#include "ECS/LivingTurn.h"
#include "ECS/ObjectCreationIndex.h"
#include "ECS/Registry.h"
#include "ECS/Systems/PathfindingSystemInterface.h"
#include "ECS/Villager/VillagerOriginalFns.h"
#include "ECS/Villager/VillagerScript.h"
#include "ECS/VillagerSpeed.h"
#include "Locator.h"

#define LOCATOR_IMPLEMENTATIONS
#include "ECS/Systems/Implementations/LivingActionSystem.h"
#include "support/WorldSystems.h"

using namespace openblack;
using namespace openblack::ecs::components;

namespace
{
/// PathfindingSystem's place in the Locator: counts the steps the state functions ask for
class CountingPathfinding final: public ecs::systems::PathfindingSystemInterface
{
public:
	void Step(entt::entity entity) override { steps.push_back(entity); }
	std::vector<entt::entity> steps;
};

class LivingTurnTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		for (const auto* name : {"ai", "pathfinding", "game"})
		{
			if (spdlog::get(name) == nullptr)
			{
				spdlog::create<spdlog::sinks::null_sink_mt>(name);
			}
		}
		Locator::entitiesRegistry::emplace<ecs::Registry>();
		openblack::test::EmplaceWorldSystems();
		ecs::object_index::OnLoadMap();
	}

	void TearDown() override
	{
		Locator::pathfindingSystem::reset();
		Locator::livingActionSystem::reset();
		Locator::entitiesRegistry::reset();
		openblack::test::ResetWorldSystems();
	}

	static ecs::Registry& Reg() { return Locator::entitiesRegistry::value(); }

	/// A villager as the living list sees it (Villager + LivingAction), made now (its creation index)
	static entt::entity MakeVillager(uint8_t top = 163)
	{
		auto& registry = Reg();
		const auto e = registry.Create();
		ecs::object_index::Assign(e);
		registry.Assign<Villager>(e);
		registry.Assign<LivingAction>(e, static_cast<VillagerStates>(top), static_cast<uint16_t>(0));
		registry.Assign<Transform>(e);
		registry.Assign<WallHug>(e, glm::vec2(0.0f), glm::vec2(0.0f), 0.0f, 0.5f);
		return e;
	}

	/// An animal as the living list sees it (Animal + Transform)
	static entt::entity MakeAnimal()
	{
		auto& registry = Reg();
		const auto e = registry.Create();
		ecs::object_index::Assign(e);
		registry.Assign<Animal>(e);
		registry.Assign<Transform>(e);
		return e;
	}
};

// ---- the list: a new one goes in at the head, a deleted one is unlinked ---------------------------------------------

TEST_F(LivingTurnTest, ListIsNewestFirstVillagersAndAnimalsMixed)
{
	const auto v1 = MakeVillager();
	const auto a1 = MakeAnimal();
	const auto v2 = MakeVillager();
	const auto a2 = MakeAnimal();
	EXPECT_EQ(ecs::living_turn::LivingList(), (std::vector<entt::entity> {a2, v2, a1, v1}));
}

TEST_F(LivingTurnTest, ListLeavesOutTheUnlinkedAndTheNotLiving)
{
	const auto v1 = MakeVillager();
	const auto marked = MakeVillager();
	const auto a1 = MakeAnimal();
	// the deletion marked it (and unlinked it from the list)
	Reg().Assign<Unavailable>(marked);
	// a Villager without its LivingAction is no Living of the list
	const auto bare = Reg().Create();
	ecs::object_index::Assign(bare);
	Reg().Assign<Villager>(bare);
	EXPECT_EQ(ecs::living_turn::LivingList(), (std::vector<entt::entity> {a1, v1}));
}

// ---- the loop: the next one is read before each turn ----------------------------------------------------------------

TEST_F(LivingTurnTest, WalkVisitsTheListInOrder)
{
	const auto a = MakeVillager();
	const auto b = MakeAnimal();
	const auto c = MakeVillager();
	std::vector<entt::entity> seen;
	ecs::living_turn::WalkList(ecs::living_turn::LivingList(), [&seen](entt::entity e) { seen.push_back(e); });
	EXPECT_EQ(seen, (std::vector<entt::entity> {c, b, a}));
}

TEST_F(LivingTurnTest, WalkSkipsOneUnlinkedByAnEarlierTurn)
{
	const auto d = MakeVillager();
	const auto c = MakeVillager();
	const auto b = MakeVillager();
	const auto a = MakeVillager();
	std::vector<entt::entity> seen;
	ecs::living_turn::WalkList(ecs::living_turn::LivingList(), [&](entt::entity e) {
		seen.push_back(e);
		if (e == a)
		{
			Reg().Destroy(c); // unlinked before the loop reaches it: b's next points past it
		}
	});
	EXPECT_EQ(seen, (std::vector<entt::entity> {a, b, d}));
}

TEST_F(LivingTurnTest, WalkGoesOnWhenTheCurrentOneIsDeletedInItsTurn)
{
	const auto c = MakeVillager();
	const auto b = MakeVillager();
	const auto a = MakeVillager();
	std::vector<entt::entity> seen;
	ecs::living_turn::WalkList(ecs::living_turn::LivingList(), [&](entt::entity e) {
		seen.push_back(e);
		if (e == b)
		{
			Reg().Destroy(b); // its next was read before its turn
		}
	});
	EXPECT_EQ(seen, (std::vector<entt::entity> {a, b, c}));
}

TEST_F(LivingTurnTest, WalkEndsAfterTheNextOneUnlinkedDuringTheTurn)
{
	const auto c = MakeVillager();
	const auto b = MakeVillager();
	const auto a = MakeVillager();
	std::vector<entt::entity> seen;
	ecs::living_turn::WalkList(ecs::living_turn::LivingList(), [&](entt::entity e) {
		seen.push_back(e);
		if (e == a)
		{
			// marked (the dead list keeps it): the loop holds it and runs it, then its next of 0 ends the loop
			Reg().Assign<Unavailable>(b);
		}
	});
	EXPECT_EQ(seen, (std::vector<entt::entity> {a, b}));
	(void)c;
}

TEST_F(LivingTurnTest, WalkEndsWhenTheNextOneIsGoneDuringTheTurn)
{
	const auto c = MakeVillager();
	const auto b = MakeVillager();
	const auto a = MakeVillager();
	std::vector<entt::entity> seen;
	ecs::living_turn::WalkList(ecs::living_turn::LivingList(), [&](entt::entity e) {
		seen.push_back(e);
		if (e == a)
		{
			Reg().Destroy(b); // deleted at once (the deferral off): nothing to run, and its next was 0
		}
	});
	EXPECT_EQ(seen, (std::vector<entt::entity> {a}));
	(void)c;
}

TEST_F(LivingTurnTest, OneMadeDuringTheLoopRunsFromTheNextTurn)
{
	const auto b = MakeVillager();
	const auto a = MakeVillager();
	entt::entity made = entt::null;
	std::vector<entt::entity> seen;
	ecs::living_turn::WalkList(ecs::living_turn::LivingList(), [&](entt::entity e) {
		seen.push_back(e);
		if (e == a)
		{
			made = MakeAnimal(); // at the head, which the loop has passed
		}
	});
	EXPECT_EQ(seen, (std::vector<entt::entity> {a, b}));
	EXPECT_EQ(ecs::living_turn::LivingList(), (std::vector<entt::entity> {made, a, b}));
}

// ---- the walk in the state functions that walk ----------------------------------------------------------------------

TEST_F(LivingTurnTest, WalkStatesAreTheRowsWhoseFunctionReachesMoveTo)
{
	// the 18 rows of the original's table whose state function walks (their functions are listed in
	// docs/bw1-notes/villagers.md)
	constexpr std::array<uint8_t, 18> k_Rows = {1, 2, 3, 5, 29, 47, 60, 66, 71, 90, 140, 153, 154, 167, 193, 203, 222, 230};
	EXPECT_EQ(ecs::living_turn::k_WalkStates, k_Rows);
}

TEST_F(LivingTurnTest, OnlyTheStatesThatWalkTakeAStep)
{
	Locator::livingActionSystem::emplace<ecs::systems::LivingActionSystem>();
	auto& steps = static_cast<CountingPathfinding&>(Locator::pathfindingSystem::emplace<CountingPathfinding>()).steps;
	// 1 (VillagerMoveToPos) and the not-ported rows that walk (TodoWalk); 6 / 7 / 8 (TodoWithExitReaction) do not
	const std::array<uint8_t, 19> rows = {1, 2, 3, 5, 29, 66, 71, 90, 140, 153, 154, 167, 193, 203, 222, 230, 6, 7, 8};
	for (const auto row : rows)
	{
		const auto villager = MakeVillager(row);
		// a walk going on (a tag the old walk of everyone would have moved in any state)
		Reg().Assign<MoveStateStepThroughTag>(villager, MoveStateClockwise::Undefined, glm::vec2(0.0f));
		steps.clear();
		auto& stepAction = Reg().Get<LivingAction>(villager);
		static_cast<void>(Locator::livingActionSystem::value().VillagerCallState(stepAction, LivingAction::Index::Top));
		const bool walks = std::find(ecs::living_turn::k_WalkStates.begin(), ecs::living_turn::k_WalkStates.end(), row) !=
		                   ecs::living_turn::k_WalkStates.end();
		EXPECT_EQ(steps, walks ? std::vector<entt::entity> {villager} : std::vector<entt::entity> {}) << "row " << +row;
	}
}

// ---- InitStepsXZ in integers ----------------------------------------------------------------------------------------

struct StepCase
{
	glm::vec2 from;
	glm::vec2 to;
	uint16_t whole;
	uint16_t angle;  ///< GetAngleFromXZ
	glm::ivec2 step; ///< MapCoords units
	uint32_t yAngle; ///< ConvertGameAngleTo3D(angle), its bits
};

TEST_F(LivingTurnTest, InitStepsXZIsTheExesIntegerStep)
{
	// a and the step from the original's tables, (x, z) = ((COS, SIN)[a] * (whole >> 4)) >> 12
	const std::array<StepCase, 8> cases = {{
	    {{100.0f, 130.0f}, {130.0f, 160.0f}, 6550, 256, {4627, 4627}, 0x3F490FDBu},
	    {{0.0f, 0.0f}, {10.0f, 0.0f}, 4600, 0, {4592, 0}, 0x00000000u},
	    {{130.0f, 100.0f}, {100.0f, 100.0f}, 6550, 1024, {-6544, 0}, 0x40490FDBu},
	    {{100.0f, 130.0f}, {100.0f, 100.0f}, 6550, 1536, {0, -6544}, 0x4096CBE4u},
	    {{100.0f, 100.0f}, {90.0f, 115.0f}, 5767, 703, {-3186, 4799}, 0x400A08A3u},
	    {{2000.5f, 1500.25f}, {1987.75f, 1488.0f}, 3277, 1272, {-2364, -2251}, 0x4079C1B2u},
	    {{512.0f, 640.0f}, {530.0f, 601.0f}, 4600, 1676, {1912, -4175}, 0x40A48A7Au},
	    {{2185.72f, 2315.78f}, {2218.0796f, 2368.6238f}, 4600, 334, {2384, 3924}, 0x3F832958u},
	}};
	for (const auto& c : cases)
	{
		Transform transform {};
		transform.position = glm::vec3(c.from.x, 0.0f, c.from.y);
		// the WallHug keeps ToMetres(whole) (SetVillagerSpeed), which WholeSpeed gives back
		WallHug wallHug {c.to, glm::vec2(0.0f), 0.0f, map_coords::ToMetres(static_cast<int32_t>(c.whole))};
		ASSERT_EQ(ecs::WholeSpeed(wallHug), c.whole);
		EXPECT_EQ(gutils::GetAngleFromXZ(c.from, c.to), c.angle);
		ecs::villager::InitStepsXZ(transform, wallHug);
		EXPECT_EQ(wallHug.step.x, map_coords::ToMetres(c.step.x)) << c.angle;
		EXPECT_EQ(wallHug.step.y, map_coords::ToMetres(c.step.y)) << c.angle;
		EXPECT_EQ(std::bit_cast<uint32_t>(wallHug.yAngle), c.yAngle) << c.angle;
		// SetGameAngle: drawn at yAngle + pi / 2
		EXPECT_TRUE(transform.rotation == affine::AngleY(wallHug.yAngle + glm::half_pi<float>())) << c.angle;
	}
}
} // namespace

// ---- the original's function columns (characterisation) -----------------------------------------------------------

namespace
{
/// Each column's equality classes, row by row (0 = no function), taken from the original's table: two rows have
/// the same function exactly when they have the same class
constexpr std::array<uint8_t, 255> k_EntryClasses = {
    0, 0, 0, 0,  1, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0,  0,  3, 0, 0, 0, 0, 0, 0, 0, 4,  0, 0, 0, 1, 5, 0, 0,  0,  0,  0,  0,  0,
    0, 0, 6, 6,  6, 0, 0, 0, 0, 0, 0, 0, 0, 0, 6,  0,  0, 6, 7, 7, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 8, 8,  8,  0,  0,  0,  0,
    0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  0,  0, 0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0,  0,  0,  0,  0,  0,
    0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 9, 9, 9, 0,  0,  0, 0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0,  0,  0,  0,  0,  0,
    0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  0,  0, 0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0,  0,  0,  0,  0,  6,
    0, 0, 0, 6,  6, 0, 0, 0, 0, 0, 0, 0, 0, 0, 10, 11, 0, 0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 12, 12, 12, 13, 12, 14,
    0, 0, 0, 15, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  0,  0, 0, 0, 0, 0, 0, 0, 0, 16, 1, 0, 0, 0, 0, 0, 17, 0,
};
constexpr std::array<uint8_t, 255> k_ExitClasses = {
    0,  1,  1,  0,  2,  3,  4,  4,  4,  4,  5,  6,  4,  0,  0,  7,  8,  0,  9,  4,  4,  4,  4,  0,  10, 4,  4,  0,  2,
    11, 4,  0,  0,  0,  0,  12, 12, 12, 12, 13, 13, 13, 14, 14, 14, 14, 14, 15, 15, 15, 15, 13, 15, 0,  13, 16, 16, 0,
    17, 17, 18, 0,  0,  0,  0,  19, 0,  20, 20, 20, 0,  21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 0,  0,
    0,  0,  0,  22, 23, 23, 23, 23, 23, 23, 23, 23, 23, 12, 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  12, 12, 12, 12, 24, 24, 24, 12, 12, 12, 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  4,  4,  4,  4,  4,
    0,  4,  4,  4,  4,  4,  4,  4,  22, 0,  4,  4,  4,  4,  4,  4,  4,  4,  0,  0,  0,  4,  4,  4,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  13, 0,  0,  0,  13, 13, 23, 23, 23, 22, 4,  4,  4,  0,  25, 26, 27, 28, 28,
    4,  29, 4,  4,  4,  4,  0,  0,  0,  0,  18, 4,  4,  30, 30, 30, 31, 30, 0,  1,  0,  0,  32, 0,  4,  23, 0,  22, 4,
    0,  0,  33, 4,  4,  4,  0,  0,  0,  19, 0,  0,  0,  0,  0,  2,  12, 12, 0,  28, 0,  22, 0,
};
constexpr std::array<uint8_t, 255> k_ValidateClasses = {
    0, 1, 2, 3, 0, 0, 4, 4, 4, 4, 0, 0, 4, 0, 0, 0, 0, 0, 0, 4, 4, 4, 4, 0, 0, 4, 4, 0, 0, 0, 4, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 6, 4, 4, 4, 4, 0, 4, 4,
    4, 4, 4, 4, 4, 0, 1, 4, 4, 4, 4, 4, 4, 4, 4, 0, 0, 0, 4, 6, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 4, 4, 4, 0, 0, 0, 0, 4, 4, 4, 0, 4, 4, 4, 4, 0, 0, 0, 0, 0, 4, 4, 4, 4, 4, 0, 4, 0,
    0, 0, 0, 5, 0, 4, 0, 0, 0, 4, 0, 7, 0, 4, 4, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4, 0, 0, 0,
};
} // namespace

TEST(VillagerOriginalFns, EachColumnKeepsTheOriginalsEqualities)
{
	const auto& table = ecs::villager::k_OriginalStateFns;
	const auto check = [&table](const auto& field, const std::array<uint8_t, 255>& classes, const char* name) {
		for (size_t a = 0; a < table.size(); ++a)
		{
			EXPECT_EQ(field(table.at(a)) == 0, classes.at(a) == 0) << name << " row " << a;
			for (size_t b = 0; b < table.size(); ++b)
			{
				if ((field(table.at(a)) == field(table.at(b))) != (classes.at(a) == classes.at(b)))
				{
					ADD_FAILURE() << name << " rows " << a << " and " << b;
				}
			}
		}
	};
	check([](const auto& row) { return row.entry; }, k_EntryClasses, "entry");
	check([](const auto& row) { return row.exit; }, k_ExitClasses, "exit");
	check([](const auto& row) { return row.validate; }, k_ValidateClasses, "validate");
}
