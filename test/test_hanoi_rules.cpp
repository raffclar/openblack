/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <entt/entity/registry.hpp>
#include <entt/meta/context.hpp>
#include <gtest/gtest.h>

#include "ECS/Components/PuzzleGame.h"
#include "ECS/HanoiRules.h"
#include "ECS/Reflection/ComponentReflection.h"
#include "ECS/Reflection/WorldArchive.h"
#include "ScriptHeaders/ScriptCreateRules.h"

using openblack::MobileObjectInfo;
using openblack::map_coords::MapCoords;
namespace hanoi = openblack::ecs::hanoi;

namespace
{
constexpr auto k_Base = MobileObjectInfo::HanoiPuzzleBase;
constexpr auto k_Largest = MobileObjectInfo::HanoiPuzzlePart1;
constexpr auto k_Second = MobileObjectInfo::HanoiPuzzlePart2;
constexpr auto k_Third = MobileObjectInfo::HanoiPuzzlePart3;
constexpr auto k_Smallest = MobileObjectInfo::HanoiPuzzlePart4;

/// Pieces named by numbers: bases 1 to 3, rings 11 to 14 from the largest, 0 for no piece
hanoi::Stacks<int> StartStacks()
{
	return {.bases = {1, 2, 3}, .rings = {11, 12, 13, 14}, .below = {1, 11, 12, 13}};
}
} // namespace

// A ring rests on a base or on a larger ring, never on a smaller one
TEST(HanoiRules, ARingRestsOnABaseOrALargerRing)
{
	for (const auto ring : hanoi::k_RingKinds)
	{
		EXPECT_TRUE(hanoi::MayRestOn(ring, k_Base));
	}
	EXPECT_TRUE(hanoi::MayRestOn(k_Second, k_Largest));
	EXPECT_TRUE(hanoi::MayRestOn(k_Smallest, k_Largest));
	EXPECT_TRUE(hanoi::MayRestOn(k_Smallest, k_Third));
	EXPECT_FALSE(hanoi::MayRestOn(k_Largest, k_Second));
	EXPECT_FALSE(hanoi::MayRestOn(k_Second, k_Third));
	EXPECT_FALSE(hanoi::MayRestOn(k_Third, k_Smallest));
	EXPECT_FALSE(hanoi::MayRestOn(k_Largest, k_Smallest));
	// Nothing rests on a thing that is not a piece of the puzzle
	EXPECT_FALSE(hanoi::MayRestOn(k_Smallest, MobileObjectInfo::Ball));
}

// A ring of the same size as the top ring may rest on it, as the game allows
TEST(HanoiRules, ARingOfTheSameSizeMayRestOnTheTop)
{
	EXPECT_TRUE(hanoi::MayRestOn(k_Largest, k_Largest));
	EXPECT_TRUE(hanoi::MayRestOn(k_Smallest, k_Smallest));
}

// Each piece puts a ring that rests on it higher by its own height
TEST(HanoiRules, TheHeightsOfThePieces)
{
	EXPECT_EQ(hanoi::HeightOf(k_Base), 0.0f);
	EXPECT_EQ(hanoi::HeightOf(k_Largest), 4.0f);
	EXPECT_EQ(hanoi::HeightOf(k_Second), 3.0f);
	EXPECT_EQ(hanoi::HeightOf(k_Third), 2.0f);
	EXPECT_EQ(hanoi::HeightOf(k_Smallest), 1.0f);
}

// The rings start stacked on the first base, each above the one below by the height of that one
TEST(HanoiRules, TheRingsStartStackedOnTheFirstBase)
{
	const MapCoords base {.x = 100, .z = 200, .altitude = 0.0f};
	const auto places = hanoi::StartRingPlaces(base);
	EXPECT_EQ(places.at(0), (MapCoords {.x = 100, .z = 200, .altitude = 0.0f}));
	EXPECT_EQ(places.at(1).altitude, 4.0f);
	EXPECT_EQ(places.at(2).altitude, 7.0f);
	EXPECT_EQ(places.at(3).altitude, 9.0f);
	EXPECT_EQ(places.at(3).x, 100);
	EXPECT_EQ(places.at(3).z, 200);
}

// The script's half turn becomes the game angle 1024 through two float products
TEST(HanoiRules, TheHalfTurnOfTheBeachTemple)
{
	const float radians = openblack::script::create_rules::AngleFromDegrees(180.0f);
	EXPECT_EQ(hanoi::GameAngleOf(radians), 1024);
	EXPECT_EQ(hanoi::GameAngleOf(0.0f), 0);
	// Not masked: a negative angle stays negative
	EXPECT_LT(hanoi::GameAngleOf(-0.5f), 0);
}

// The bases stand along the puzzle's angle, two steps apart, the first on the puzzle's place
TEST(HanoiRules, TheBasesStandInARow)
{
	const MapCoords puzzle {.x = 0x01000000, .z = 0x02000000, .altitude = 0.0f};
	// The half turn: the cosine is -65536 and the sine 0, so the row goes towards -z, 20 metres a base
	const auto half = hanoi::Spaced(hanoi::StepsOf(1024, 1.0f));
	EXPECT_EQ(half.cosine, -131072);
	EXPECT_EQ(half.sine, 0);
	EXPECT_EQ(hanoi::BasePlace(puzzle, half, 0), puzzle);
	EXPECT_EQ(hanoi::BasePlace(puzzle, half, 1), (MapCoords {.x = 0x01000000, .z = 0x02000000 - 131072}));
	EXPECT_EQ(hanoi::BasePlace(puzzle, half, 2), (MapCoords {.x = 0x01000000, .z = 0x02000000 - 262144}));
	// A quarter turn: the row goes towards +x
	const auto quarter = hanoi::Spaced(hanoi::StepsOf(512, 1.0f));
	EXPECT_EQ(hanoi::BasePlace(puzzle, quarter, 1).x, 0x01000000 + 131072);
	EXPECT_EQ(hanoi::BasePlace(puzzle, quarter, 1).z, 0x02000000);
}

// The size scales the steps, truncated after a float product
TEST(HanoiRules, TheSizeScalesTheSteps)
{
	const auto steps = hanoi::StepsOf(1, 1.5f);
	// The table holds 65535 and 201 for the first step of the circle
	EXPECT_EQ(steps.cosine, 98302);
	EXPECT_EQ(steps.sine, 301);
}

// Solved: the four rings stacked in order, the largest on the last base
TEST(HanoiRules, SolvedOnTheLastBase)
{
	auto stacks = StartStacks();
	EXPECT_FALSE(hanoi::Solved(stacks));
	stacks.below.front() = 3;
	EXPECT_TRUE(hanoi::Solved(stacks));
	// On the second base it is not solved
	stacks.below.front() = 2;
	EXPECT_FALSE(hanoi::Solved(stacks));
	// A ring out of order is not solved
	stacks.below = {3, 11, 11, 13};
	EXPECT_FALSE(hanoi::Solved(stacks));
}

// The largest ring on a thing that is not one of the first two bases also counts, as the game tests only those two
TEST(HanoiRules, SolvedOnAnyThingButTheFirstTwoBases)
{
	auto stacks = StartStacks();
	stacks.below.front() = 99;
	EXPECT_TRUE(hanoi::Solved(stacks));
}

// The hand may take only an uncovered ring of a calm, unsolved puzzle
TEST(HanoiRules, WhenTheHandMayTakeAPiece)
{
	const hanoi::PickUpFacts free {.hasPuzzle = true, .calm = 5};
	EXPECT_FALSE(hanoi::CannotBePickedUp(free));
	auto facts = free;
	facts.base = true;
	EXPECT_TRUE(hanoi::CannotBePickedUp(facts));
	facts = free;
	facts.covered = true;
	EXPECT_TRUE(hanoi::CannotBePickedUp(facts));
	facts = free;
	facts.hasPuzzle = false;
	EXPECT_TRUE(hanoi::CannotBePickedUp(facts));
	facts = free;
	facts.calm = 4;
	EXPECT_TRUE(hanoi::CannotBePickedUp(facts));
	facts = free;
	facts.fixed = true;
	EXPECT_TRUE(hanoi::CannotBePickedUp(facts));
}

// The count of calm turns goes down to -5 while a ring is off its place, and up to 5 while all are calm
TEST(HanoiRules, TheCountOfCalmTurns)
{
	EXPECT_EQ(hanoi::CalmAfterRestlessTurn(5), 4);
	EXPECT_EQ(hanoi::CalmAfterRestlessTurn(-4), -5);
	EXPECT_EQ(hanoi::CalmAfterRestlessTurn(-5), -5);
	EXPECT_EQ(hanoi::CalmAfterCalmTurn(4), 5);
	EXPECT_EQ(hanoi::CalmAfterCalmTurn(5), 5);
	EXPECT_EQ(hanoi::CalmAfterCalmTurn(-5), -4);
}

// Of the rings that the puzzle makes at the start, only the largest casts its shadow into the land
TEST(HanoiRules, OnlyTheLargestStartRingCastsALandShadow)
{
	EXPECT_TRUE(hanoi::StartRingCastsLandShadow(0));
	EXPECT_FALSE(hanoi::StartRingCastsLandShadow(1));
	EXPECT_FALSE(hanoi::StartRingCastsLandShadow(2));
	EXPECT_FALSE(hanoi::StartRingCastsLandShadow(3));
}

// A save keeps the puzzle, its links to its pieces and the links between the pieces
TEST(HanoiRules, ASaveKeepsThePuzzleAndItsPieces)
{
	using openblack::ecs::components::HanoiBlock;
	using openblack::ecs::components::PuzzleGame;
	namespace reflection = openblack::ecs::reflection;
	entt::meta_ctx context;
	reflection::RegisterComponents(context);
	entt::registry registry;
	const auto puzzle = registry.create();
	const auto base = registry.create();
	const auto ring = registry.create();
	PuzzleGame game {.type = openblack::script::PuzzleGameType::Hanoi, .angle = 1024, .calmTurns = -2, .order = 3};
	game.bases.front() = base;
	game.rings.front() = ring;
	game.kept.at(hanoi::k_Bases) = HanoiBlock {.puzzle = puzzle, .below = base};
	registry.emplace<PuzzleGame>(puzzle, game);
	registry.emplace<HanoiBlock>(base, HanoiBlock {.puzzle = puzzle, .above = ring});
	registry.emplace<HanoiBlock>(ring, HanoiBlock {.puzzle = puzzle, .home = {.altitude = 4.0f}, .below = base});

	reflection::WorldSaveReport report;
	const auto bytes = reflection::SaveWorld(registry, context, &report);
	EXPECT_TRUE(report.unsavedValues.empty());
	entt::registry loaded;
	ASSERT_TRUE(reflection::LoadWorld(loaded, context, bytes, reflection::WorldLoadMode::Exact).has_value());
	const auto& loadedGame = loaded.get<PuzzleGame>(puzzle);
	EXPECT_EQ(loadedGame.bases.front(), base);
	EXPECT_EQ(loadedGame.rings.front(), ring);
	EXPECT_EQ(loadedGame.calmTurns, -2);
	EXPECT_EQ(loadedGame.angle, 1024);
	EXPECT_EQ(loadedGame.kept.at(hanoi::k_Bases).below, base);
	EXPECT_EQ(loaded.get<HanoiBlock>(base).above, ring);
	EXPECT_EQ(loaded.get<HanoiBlock>(ring).below, base);
	EXPECT_EQ(loaded.get<HanoiBlock>(ring).home.altitude, 4.0f);
}
