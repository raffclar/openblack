/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// UseFootpathIfNecessary, on hand-made footpaths and a villager with a fake state table (test_villager_core's): the
// nearest footpath within 40 m is taken; with none, and no town, the walk falls back to SetupMoveToWithHug.

#define LOCATOR_IMPLEMENTATIONS

#include <memory>
#include <optional>

#include <gtest/gtest.h>

#include "3D/MapCoords.h"
#include "Common/RandomNumberManager.h"
#include "ECS/Components/Footpath.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WallHug.h"
#include "ECS/Footpaths.h"
#include "ECS/LivingFootpath.h"
#include "ECS/Registry.h"
#include "ECS/Systems/LivingActionSystemInterface.h"
#include "ECS/Villager/VillagerCore.h"
#include "Enums.h"
#include "GameClock.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "support/TestServices.h"
#include "support/WorldSystems.h"

using namespace openblack;
using namespace openblack::ecs::components;
namespace footpaths = openblack::ecs::footpaths;
namespace map_coords = openblack::map_coords;
namespace villager = openblack::ecs::villager;
using Index = LivingAction::Index;

namespace
{
class TestRng final: public RandomNumberManagerInterface
{
	std::mt19937 _generator {1};
	std::mt19937& Generator() override { return _generator; }
	std::optional<std::reference_wrapper<std::mutex>> LockAccess() override { return std::nullopt; }
};

/// Every state entry / exit succeeds and nothing runs (test_villager_core's FakeStateTable, without its log)
class QuietStateTable final: public ecs::systems::LivingActionSystemInterface
{
public:
	void Update() override {}
	[[nodiscard]] VillagerStates VillagerGetState(const LivingAction& action, Index index) const override
	{
		return static_cast<VillagerStates>(action.states.at(static_cast<size_t>(index)));
	}
	void VillagerSetState(LivingAction&, Index, VillagerStates, bool) const override {}
	uint32_t VillagerCallState(LivingAction&, Index) const override { return 0; }
	uint32_t VillagerCallEntry(LivingAction&, VillagerStates, VillagerStates, VillagerStates) const override { return 1; }
	uint32_t VillagerCallExit(LivingAction&, VillagerStates, VillagerStates) const override { return 1; }
	int VillagerCallOutOfAnimation(LivingAction&, Index) const override { return -1; }
	bool VillagerCallValidate(LivingAction&, Index) const override { return false; }
};

class UseFootpathTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		test::EmplaceMapAndVillagerDefaults();
		auto info = std::make_unique<InfoConstants>();
		info->villager.at(0).life = 1.0f;
		info->villager.at(0).moveState = LivingStates::LivingMoveToPos;
		Locator::infoConstants::reset(info.release());
		Locator::entitiesRegistry::emplace<ecs::Registry>();
		openblack::test::EmplaceWorldSystems();
		Locator::rng::emplace<TestRng>();
		Locator::livingActionSystem::emplace<QuietStateTable>();
		game_clock::SetTurn(100);
	}
	void TearDown() override
	{
		Locator::livingActionSystem::reset();
		Locator::rng::reset();
		Locator::entitiesRegistry::reset();
		openblack::test::ResetWorldSystems();
		test::ResetMapAndVillagerDefaults();
		Locator::infoConstants::reset();
	}
	static ecs::Registry& Reg() { return Locator::entitiesRegistry::value(); }

	/// A footpath along x, one visible node every 10 m from x0 (test_footpaths' Make)
	static entt::entity Make(int x0, int count)
	{
		const auto e = Reg().Create();
		auto& f = Reg().Assign<Footpath>(e);
		for (int i = 0, x = x0; i < count; ++i, x += 10)
		{
			const auto coords = map_coords::FromMetres(glm::vec2(static_cast<float>(x), 0.0f));
			f.nodes.push_back({glm::vec3(static_cast<float>(x), 0.0f, 0.0f), coords, 1, f.nextNodeId++});
		}
		return e;
	}
	static entt::entity Link(std::vector<entt::entity> paths)
	{
		std::vector<Footpath::Id> ids;
		for (const auto p : paths)
		{
			ids.push_back(static_cast<Footpath::Id>(p));
		}
		const auto link = Reg().Create();
		Reg().Assign<FootpathLink>(link, glm::vec3(0.0f), ids);
		return link;
	}
	static entt::entity MakeVillager(float x)
	{
		const auto e = Reg().Create();
		auto& v = Reg().Assign<Villager>(e);
		v.life = 1.0f;
		v.town = entt::null;
		v.abode = entt::null;
		v.food = 1.0f;
		Reg().Assign<LivingAction>(e, static_cast<VillagerStates>(163), static_cast<uint16_t>(0));
		Reg().Assign<Transform>(e, glm::vec3(x, 0.0f, 0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
		// the walk's goal (WallHug), as test_villager_core's SetupMoveToWithHug cases
		Reg().Assign<WallHug>(e, glm::vec2(), glm::vec2(), 0.0f, 0.1f);
		return e;
	}
	static map_coords::MapCoords At(float x) { return map_coords::FromMetres(glm::vec2(x, 0.0f)); }
};

TEST_F(UseFootpathTest, TakesTheNearestFootpathWithinFortyMetres)
{
	const auto near = Make(0, 3);
	const auto far = Make(200, 3);
	const auto link = Link({far, near});
	const auto living = MakeVillager(1.0f);
	// GetNearestPathTo(the link's footpaths, pos, 40) and SetupMoveOnFootpath != 0 -> 1
	EXPECT_EQ(footpaths::UseFootpathIfNecessary(link, living, At(50.0f), 0, entt::null), 1u);
	const auto* fp = Reg().TryGet<LivingFootpath>(living);
	ASSERT_NE(fp, nullptr);
	EXPECT_TRUE(fp->footpath == near);
	EXPECT_NE(fp->node, footpaths::k_NoNode);
	// SetupMobileMoveToPos(the node, 12): the goal is that node, on the footpath at x 0..20
	const auto& hug = Reg().Get<WallHug>(living);
	EXPECT_LE(hug.goal.x, 20.5f);
	EXPECT_NEAR(hug.goal.y, 0.0f, 0.01f);
}

TEST_F(UseFootpathTest, NoFootpathAndNoTownFallsBackToTheHug)
{
	const auto far = Make(500, 3);
	const auto link = Link({far});
	const auto living = MakeVillager(1.0f);
	// nothing within 40 m, no owner and no town -> SetupMoveToWithHug(pos, final): the footpath fields are not
	// touched
	static_cast<void>(footpaths::UseFootpathIfNecessary(link, living, At(50.0f), 0, entt::null));
	const auto* fp = Reg().TryGet<LivingFootpath>(living);
	EXPECT_TRUE(fp == nullptr || fp->footpath == entt::null);
	// the hug's goal is pos itself
	const auto& hug = Reg().Get<WallHug>(living);
	EXPECT_NEAR(hug.goal.x, 50.0f, 0.01f);
	EXPECT_NEAR(hug.goal.y, 0.0f, 0.01f);
}
} // namespace
