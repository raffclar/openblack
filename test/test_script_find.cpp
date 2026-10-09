/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <map>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "ECS/Components/Abode.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/Ball.h"
#include "ECS/Components/Feature.h"
#include "ECS/Components/Flowers.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Tree.h"
#include "ECS/Registry.h"
#include "ECS/ScriptFind.h"

using namespace openblack;
using namespace openblack::ecs;
using ScriptType = openblack::script::ObjectType;

namespace
{
/// Things laid in cells, as a fake of the map's cells
struct FakeCells
{
	std::map<std::pair<int, int>, std::vector<script_find::Candidate>> cells;
	mutable std::vector<glm::ivec2> visited;

	void Put(entt::entity entity, glm::vec2 metres)
	{
		const auto at = map_coords::FromMetres(metres);
		const auto cell = map_coords::Cell(at);
		cells[{cell.x, cell.y}].push_back({.entity = entity, .at = at});
	}

	[[nodiscard]] script_find::CellCandidates Lookup() const
	{
		return [this](glm::ivec2 cell) {
			visited.push_back(cell);
			const auto found = cells.find({cell.x, cell.y});
			return found != cells.end() ? found->second : std::vector<script_find::Candidate> {};
		};
	}
};

constexpr auto k_A = static_cast<entt::entity>(1);
constexpr auto k_B = static_cast<entt::entity>(2);
constexpr auto k_C = static_cast<entt::entity>(3);
} // namespace

TEST(ScriptFind, MatchesTypeAndSubtype)
{
	const script_find::Kind abode {.type = ScriptType::Abode, .subtype = 7};
	EXPECT_TRUE(script_find::Matches(abode, ScriptType::Abode, 7));
	EXPECT_TRUE(script_find::Matches(abode, ScriptType::Abode, script_find::k_AnySubtype));
	EXPECT_FALSE(script_find::Matches(abode, ScriptType::Abode, 8));
	EXPECT_FALSE(script_find::Matches(abode, ScriptType::Feature, script_find::k_AnySubtype));
}

TEST(ScriptFind, NoSubtypeMatchesOnlyAny)
{
	const script_find::Kind ball {.type = ScriptType::Ball};
	EXPECT_TRUE(script_find::Matches(ball, ScriptType::Ball, script_find::k_AnySubtype));
	EXPECT_FALSE(script_find::Matches(ball, ScriptType::Ball, 0));
}

TEST(ScriptFind, CellsAroundSpanTheReach)
{
	// 1 metre about (25, 25) stays in cell 2
	auto range = script_find::CellsAround(map_coords::FromMetres({25.0f, 25.0f}), 1.0f);
	EXPECT_EQ(range.low, glm::ivec2(2, 2));
	EXPECT_EQ(range.high, glm::ivec2(2, 2));
	// 1 metre about (20.5, 39.5) reaches cells 1 and 3
	range = script_find::CellsAround(map_coords::FromMetres({20.5f, 39.5f}), 1.0f);
	EXPECT_EQ(range.low, glm::ivec2(1, 3));
	EXPECT_EQ(range.high, glm::ivec2(2, 4));
	// 50 metres about (100, 100)
	range = script_find::CellsAround(map_coords::FromMetres({100.0f, 100.0f}), 50.0f);
	EXPECT_EQ(range.low, glm::ivec2(5, 5));
	EXPECT_EQ(range.high, glm::ivec2(15, 15));
}

TEST(ScriptFind, CellsLookedAtXByXThenZ)
{
	FakeCells cells;
	const auto found = script_find::FindNearest(map_coords::FromMetres({20.5f, 20.5f}), 1.0f, cells.Lookup());
	EXPECT_TRUE(found == entt::null);
	const std::vector<glm::ivec2> expected {{1, 1}, {1, 2}, {2, 1}, {2, 2}};
	EXPECT_EQ(cells.visited, expected);
}

TEST(ScriptFind, CellsOffTheMapPassedOver)
{
	FakeCells cells;
	const auto found = script_find::FindNearest(map_coords::FromMetres({0.5f, 0.5f}), 1.0f, cells.Lookup());
	EXPECT_TRUE(found == entt::null);
	const std::vector<glm::ivec2> expected {{0, 0}};
	EXPECT_EQ(cells.visited, expected);
}

TEST(ScriptFind, NearestWinsAndFirstMetWinsATie)
{
	FakeCells cells;
	cells.Put(k_A, {26.0f, 25.0f});
	cells.Put(k_B, {24.0f, 25.0f});
	cells.Put(k_C, {25.5f, 25.0f});
	const auto from = map_coords::FromMetres({25.0f, 25.0f});
	EXPECT_TRUE(script_find::FindNearest(from, 1.0f, cells.Lookup()) == k_C);

	FakeCells tie;
	tie.Put(k_A, {26.0f, 25.0f});
	tie.Put(k_B, {24.0f, 25.0f});
	EXPECT_TRUE(script_find::FindNearest(from, 1.0f, tie.Lookup()) == k_A);
}

TEST(ScriptFind, AThingFartherThanTheReachInALookedAtCellCounts)
{
	// A plain "get ... at" looks through the cells within a metre, but anything in them counts
	FakeCells cells;
	cells.Put(k_A, {29.0f, 29.0f});
	EXPECT_TRUE(script_find::FindNearest(map_coords::FromMetres({21.0f, 21.0f}), 1.0f, cells.Lookup()) == k_A);
}

TEST(ScriptFind, TownStrictlyWithinTheReach)
{
	const auto from = map_coords::FromMetres({100.0f, 100.0f});
	const std::vector<script_find::Candidate> towns {
	    {.entity = k_A, .at = map_coords::FromMetres({110.0f, 100.0f})},
	    {.entity = k_B, .at = map_coords::FromMetres({95.0f, 100.0f})},
	};
	EXPECT_TRUE(script_find::FindNearestTown(from, 10.0f, towns) == k_B);
	EXPECT_TRUE(script_find::FindNearestTown(from, 5.0f, towns) == entt::null);
	EXPECT_TRUE(script_find::FindNearestTown(from, 5.5f, towns) == k_B);
}

TEST(ScriptFind, KindsOfThings)
{
	Registry registry;
	const auto abode = registry.Create();
	registry.Assign<components::Abode>(abode).info = AbodeInfo::CelticStoragePit;
	const auto feature = registry.Create();
	registry.Assign<components::Feature>(feature, FeatureInfo {3});
	const auto flowers = registry.Create();
	registry.Assign<components::Flowers>(flowers);
	const auto still = registry.Create();
	registry.Assign<components::MobileStatic>(still, MobileStaticInfo {5});
	const auto rock = registry.Create();
	registry.Assign<components::MobileStatic>(rock, MobileStaticInfo {25});
	registry.Assign<components::Rock>(rock);
	const auto sheep = registry.Create();
	registry.Assign<components::Animal>(sheep).type = AnimalInfo::Sheep;
	const auto crow = registry.Create();
	registry.Assign<components::Animal>(crow).type = AnimalInfo::Crow;
	const auto pot = registry.Create();
	registry.Assign<components::Pot>(pot).type = PotInfo {10};
	const auto ball = registry.Create();
	registry.Assign<components::Ball>(ball);
	const auto nothing = registry.Create();

	const auto expect = [&registry](entt::entity entity, ScriptType type, std::optional<uint32_t> subtype) {
		const auto kind = script_find::KindOf(registry, entity);
		ASSERT_TRUE(kind.has_value());
		EXPECT_EQ(kind->type, type);
		EXPECT_EQ(kind->subtype, subtype);
	};
	expect(abode, ScriptType::Abode, static_cast<uint32_t>(AbodeInfo::CelticStoragePit));
	expect(feature, ScriptType::Feature, 3u);
	expect(flowers, ScriptType::Feature, std::nullopt);
	expect(still, ScriptType::MobileStatic, 5u);
	expect(rock, ScriptType::Rock, 25u);
	expect(sheep, ScriptType::Animal, static_cast<uint32_t>(AnimalInfo::Sheep));
	expect(crow, ScriptType::Bird, static_cast<uint32_t>(AnimalInfo::Crow));
	expect(pot, ScriptType::Store, 10u);
	expect(ball, ScriptType::Ball, std::nullopt);
	EXPECT_FALSE(script_find::KindOf(registry, nothing).has_value());
}
