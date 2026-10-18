/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <array>
#include <vector>

#include <glm/geometric.hpp>
#include <gtest/gtest.h>

#include "Graphics/Cumulonimbus.h"

namespace cb = openblack::graphics::cumulonimbus;

namespace
{

// A storm over an island, as a forced storm is: its outer radius reaches every corner of the land
cb::StormSite IslandStorm(uint32_t serial, glm::vec2 centre)
{
	return {.serial = serial, .centre = centre, .outerRadius = 3980.0f, .strength = 1.0f, .rain = 1.0f};
}

// A mature cell of a storm
cb::Cell Mature(uint32_t serial, glm::vec2 centre)
{
	return {.serial = serial, .centre = centre, .radius = cb::k_TowerRadius, .height = 1.0f, .fade = 1.0f, .rain = 1.0f};
}

} // namespace

TEST(Cumulonimbus, TowerReachesTheTropopauseFromAFlatBottom)
{
	EXPECT_NEAR(cb::k_Tropopause, 10439.0f, 1.0f);
	EXPECT_GT(cb::k_Base, 900.0f);
	EXPECT_LT(cb::k_Base, 2000.0f);
}

TEST(Cumulonimbus, LargerStormsHoldMoreCells)
{
	EXPECT_EQ(cb::CellCount(0.0f), 1u);
	EXPECT_EQ(cb::CellCount(2000.0f), 1u);
	EXPECT_EQ(cb::CellCount(3980.0f), 2u);
	EXPECT_EQ(cb::CellCount(50000.0f), cb::k_MostCellsInStorm);
}

TEST(Cumulonimbus, CellGrowsThenBreaksUp)
{
	EXPECT_FLOAT_EQ(cb::HeightAt(0.0f), 0.0f);
	EXPECT_FLOAT_EQ(cb::HeightAt(cb::k_GrowthShare), 1.0f);
	EXPECT_FLOAT_EQ(cb::HeightAt(0.9f), 1.0f);
	EXPECT_GT(cb::HeightAt(cb::k_GrowthShare * 0.5f), 0.5f);
	EXPECT_FLOAT_EQ(cb::FadeAt(0.5f), 1.0f);
	EXPECT_FLOAT_EQ(cb::FadeAt(1.0f), 0.0f);
	EXPECT_GT(cb::FadeAt(1.0f - cb::k_DecayShare * 0.5f), 0.0f);
	EXPECT_LT(cb::FadeAt(1.0f - cb::k_DecayShare * 0.5f), 1.0f);
}

TEST(Cumulonimbus, StormAlwaysHasAMatureCell)
{
	const std::array storms {IslandStorm(4u, {2560.0f, 2560.0f})};
	for (float seconds = 0.0f; seconds < 2.0f * cb::k_LifeSeconds; seconds += 3.0f)
	{
		const auto cells = cb::CellsFor(storms, seconds);
		ASSERT_EQ(cells.count, 2u);
		bool mature = false;
		for (const auto& cell : cells.All())
		{
			mature = mature || (cell.height >= cb::k_MatureHeight && cell.fade > 0.3f);
			// The cells stand over the storm, near its middle
			EXPECT_LE(glm::distance(cell.centre, storms[0].centre), cb::k_CellReach * storms[0].outerRadius + 1e-2f);
		}
		EXPECT_TRUE(mature) << seconds;
	}
}

TEST(Cumulonimbus, CellsMoveOnlyBetweenLives)
{
	const std::array storms {IslandStorm(4u, {0.0f, 0.0f})};
	// A cell keeps its place while it lives: across one frame its place changes only when its life starts again
	int jumps = 0;
	auto before = cb::CellsFor(storms, 0.0f);
	for (float seconds = 0.1f; seconds < cb::k_LifeSeconds; seconds += 0.1f)
	{
		const auto now = cb::CellsFor(storms, seconds);
		for (size_t i = 0; i < now.count; ++i)
		{
			if (glm::distance(now.cells.at(i).centre, before.cells.at(i).centre) > 1e-3f)
			{
				++jumps;
				// A cell moves only when it was gone
				EXPECT_LT(before.cells.at(i).fade, 0.05f);
				EXPECT_LT(now.cells.at(i).height, 0.05f);
			}
		}
		before = now;
	}
	EXPECT_EQ(jumps, 2);
}

TEST(Cumulonimbus, NoCellsWithoutStorms)
{
	EXPECT_EQ(cb::CellsFor({}, 10.0f).count, 0u);
	const std::array calm {cb::StormSite {.serial = 1u, .outerRadius = 3000.0f, .strength = 0.0f}};
	EXPECT_EQ(cb::CellsFor(calm, 10.0f).count, 0u);
	// A storm that has nearly calmed down has no cells left
	const std::array faint {
	    cb::StormSite {.serial = 1u, .outerRadius = 3000.0f, .strength = cb::k_FaintestStorm * 0.5f, .rain = 1.0f}};
	EXPECT_EQ(cb::CellsFor(faint, 10.0f).count, 0u);
}

TEST(Cumulonimbus, OnlyStormsWithRainHaveCells)
{
	const std::array dry {cb::StormSite {.serial = 1u, .outerRadius = 3000.0f, .strength = 1.0f, .rain = 0.0f}};
	EXPECT_EQ(cb::CellsFor(dry, 10.0f).count, 0u);
	const std::array showers {cb::StormSite {.serial = 1u, .outerRadius = 2000.0f, .strength = 1.0f, .rain = 0.25f}};
	const auto cells = cb::CellsFor(showers, 10.0f);
	ASSERT_EQ(cells.count, 1u);
	EXPECT_FLOAT_EQ(cells.cells.at(0).height, 0.75f);
}

TEST(Cumulonimbus, StrongestStormsTakeTheCells)
{
	const std::array storms {
	    cb::StormSite {.serial = 1u, .centre = {0.0f, 0.0f}, .outerRadius = 9000.0f, .strength = 0.3f, .rain = 1.0f},
	    cb::StormSite {.serial = 2u, .centre = {50000.0f, 0.0f}, .outerRadius = 9000.0f, .strength = 0.9f, .rain = 1.0f},
	};
	const auto cells = cb::CellsFor(storms, 0.0f);
	ASSERT_EQ(cells.count, cb::k_MaxCells);
	for (size_t i = 0; i < cb::k_MostCellsInStorm; ++i)
	{
		EXPECT_EQ(cells.cells.at(i).serial, 2u);
	}
	EXPECT_EQ(cells.cells.at(cb::k_MaxCells - 1).serial, 1u);
}

TEST(Cumulonimbus, SmallStormKeepsOneCellAtItsMiddle)
{
	const std::array storms {
	    cb::StormSite {.serial = 9u, .centre = {10.0f, 20.0f}, .outerRadius = 1500.0f, .strength = 0.5f, .rain = 0.8f}};
	for (float seconds : {0.0f, 100.0f, 1000.0f})
	{
		const auto cells = cb::CellsFor(storms, seconds);
		ASSERT_EQ(cells.count, 1u);
		EXPECT_EQ(cells.cells.at(0).centre, glm::vec2(10.0f, 20.0f));
		EXPECT_FLOAT_EQ(cells.cells.at(0).height, 0.75f);
		EXPECT_FLOAT_EQ(cells.cells.at(0).fade, 1.0f);
	}
}

TEST(Cumulonimbus, StrikeComesDownUnderTheCoreOfACell)
{
	const std::vector cells {Mature(1u, {0.0f, 0.0f}), Mature(1u, {6000.0f, 0.0f}), Mature(2u, {-9000.0f, 0.0f})};
	for (uint32_t i = 0; i < 200; ++i)
	{
		const std::array randoms {cb::RandomOf(i, 0), cb::RandomOf(i, 1), cb::RandomOf(i, 2)};
		const auto place = cb::StrikePlace(cells, 1u, {300.0f, 100.0f}, randoms);
		ASSERT_TRUE(place.has_value());
		// Under the core of one of its own storm's cells, never in the clear air between them
		const float toFirst = glm::distance(*place, cells[0].centre);
		const float toSecond = glm::distance(*place, cells[1].centre);
		EXPECT_LE(std::min(toFirst, toSecond), cb::k_CoreShare * cb::k_TowerRadius + 1e-2f);
		EXPECT_GT(glm::distance(*place, cells[2].centre), cb::k_TowerRadius);
	}
}

TEST(Cumulonimbus, NearerCellsStrikeMoreOften)
{
	const std::vector cells {Mature(1u, {0.0f, 0.0f}), Mature(1u, {16000.0f, 0.0f})};
	int near = 0;
	for (uint32_t i = 0; i < 1000; ++i)
	{
		const std::array randoms {cb::RandomOf(i, 0), cb::RandomOf(i, 1), cb::RandomOf(i, 2)};
		const auto place = cb::StrikePlace(cells, 1u, {0.0f, 0.0f}, randoms);
		near += glm::length(*place) < cb::k_TowerRadius ? 1 : 0;
	}
	// The far cell is two reaches away: one chance in five against the near cell's one
	EXPECT_GT(near, 780);
	EXPECT_LT(near, 880);
}

TEST(Cumulonimbus, YoungStormStrikesFromItsTallestCell)
{
	auto low = Mature(1u, {0.0f, 0.0f});
	low.height = 0.2f;
	auto taller = Mature(1u, {5000.0f, 0.0f});
	taller.height = 0.4f;
	const std::vector cells {low, taller};
	const auto place = cb::StrikePlace(cells, 1u, {0.0f, 0.0f}, {0.5f, 0.5f, 0.0f});
	ASSERT_TRUE(place.has_value());
	EXPECT_NEAR(glm::distance(*place, taller.centre), 0.0f, 1e-3f);
}

TEST(Cumulonimbus, StormWithoutCellsHasNoStrikePlace)
{
	const std::vector cells {Mature(1u, {0.0f, 0.0f})};
	EXPECT_FALSE(cb::StrikePlace(cells, 2u, {0.0f, 0.0f}, {0.5f, 0.5f, 0.5f}).has_value());
	EXPECT_FALSE(cb::StrikePlace({}, 1u, {0.0f, 0.0f}, {0.5f, 0.5f, 0.5f}).has_value());
}

TEST(Cumulonimbus, PackPutsTheCellsAndTheWindForTheShaders)
{
	const std::vector cells {Mature(1u, {10.0f, 20.0f})};
	const auto packed = cb::Pack(cells, {0.0f, 30.0f});
	EXPECT_EQ(packed.at(0), glm::vec4(10.0f, 20.0f, cb::k_TowerRadius, 1.0f));
	EXPECT_EQ(packed.at(cb::k_MaxCells), glm::vec4(0.0f, 1.0f, 1.0f, 1.0f));
	EXPECT_EQ(packed.at(2 * cb::k_MaxCells), cb::TowerBound(cells[0], {0.0f, 1.0f}));
	EXPECT_EQ(packed.at(3 * cb::k_MaxCells), cb::AnvilBound(cells[0], {0.0f, 1.0f}));
	EXPECT_EQ(packed.at(1), glm::vec4(0.0f));
	EXPECT_EQ(packed.at(cb::k_MaxCells + 1), glm::vec4(0.0f));
	EXPECT_EQ(packed.at(3 * cb::k_MaxCells + 1), glm::vec4(0.0f));
}

TEST(Cumulonimbus, BoundsHoldTheTowerAndTheAnvil)
{
	const auto cell = Mature(1u, {0.0f, 0.0f});
	const glm::vec2 down {1.0f, 0.0f};
	const auto tower = cb::TowerBound(cell, down);
	// The leaning top and the widest bulge of the tower are inside its cylinder, and the dome under its top
	const glm::vec2 top = cell.centre + down * (cb::k_Lean * cell.radius);
	EXPECT_LT(glm::distance(top, glm::vec2(tower)) + 1.37f * cell.radius, tower.z);
	EXPECT_LT(glm::distance(cell.centre, glm::vec2(tower)) + 1.37f * cell.radius, tower.z);
	EXPECT_FLOAT_EQ(tower.w, cb::k_Tropopause + cb::k_DomeHeight);
	const auto anvil = cb::AnvilBound(cell, down);
	// The upwind edge and the downwind end of the anvil are inside its cylinder
	const glm::vec2 upwind = top - down * (cb::k_AnvilUpwind * cell.radius);
	const glm::vec2 downwind = top + down * (cb::k_AnvilDownwind * cell.radius);
	EXPECT_LE(glm::distance(upwind, glm::vec2(anvil)), anvil.z);
	EXPECT_LE(glm::distance(downwind, glm::vec2(anvil)), anvil.z);
	EXPECT_LT(anvil.w, cb::k_Tropopause - cb::k_AnvilDepth);
}

TEST(Cumulonimbus, LowTowerHasNoAnvil)
{
	auto cell = Mature(1u, {0.0f, 0.0f});
	cell.height = 0.6f;
	EXPECT_FLOAT_EQ(cb::AnvilGrowth(cell.height), 0.0f);
	EXPECT_FLOAT_EQ(cb::AnvilBound(cell, {1.0f, 0.0f}).z, 0.0f);
	EXPECT_FLOAT_EQ(cb::AnvilGrowth(1.0f), 1.0f);
}
