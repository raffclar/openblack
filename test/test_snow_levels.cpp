/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <algorithm>
#include <chrono>
#include <optional>
#include <span>
#include <vector>

#include <gtest/gtest.h>

#include "3D/SnowCover.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/BuildingDamage.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/HandGrab.h"
#include "ECS/Components/HighDetail.h"
#include "ECS/Components/MapCellResident.h"
#include "ECS/Components/Scaffold.h"
#include "ECS/Components/SnowLevel.h"
#include "ECS/Components/TownDesireFlag.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Registry.h"
#include "ECS/SnowLevels.h"
#include "ECS/Systems/ObjectDrawListSystemInterface.h"

#define LOCATOR_IMPLEMENTATIONS
#include "ECS/Systems/Implementations/SnowSystem.h"

using namespace openblack;
using namespace openblack::ecs::components;
namespace snow_levels = openblack::ecs::snow_levels;
using std::chrono::milliseconds;

namespace
{
/// Snow 120 deep everywhere: a thing that carries all of it shows the level 99
float DeepSnow(glm::vec2 /*unused*/)
{
	return 120.0f;
}

entt::entity Thing(ecs::Registry& registry)
{
	const auto entity = registry.Create();
	registry.Assign<Transform>(
	    entity, Transform {.position = {100.0f, 0.0f, 100.0f}, .rotation = glm::mat3(1.0f), .scale = glm::vec3(1.0f)});
	return entity;
}

/// A draw list that holds the things given to it, all of them drawn
class FakeDrawList final: public ecs::systems::ObjectDrawListSystemInterface
{
public:
	std::vector<entt::entity> list;
	std::optional<object_draw_list::View> view {object_draw_list::View {}};

	void Update(uint32_t /*unused*/, bool /*unused*/) override {}
	void PrepareLand() override {}
	void RequestRebuild(uint32_t /*unused*/) override {}
	[[nodiscard]] object_draw_list::Frame GetFrame() const override { return {}; }
	[[nodiscard]] const std::optional<object_draw_list::View>& GetView() const override { return view; }
	[[nodiscard]] bool Listed(entt::entity entity) const override { return std::ranges::find(list, entity) != list.end(); }
	[[nodiscard]] bool Drawn(const ecs::Registry& /*unused*/, entt::entity entity) const override { return Listed(entity); }
	void NoteFullFrame(entt::entity /*unused*/, bool /*unused*/) override {}
	[[nodiscard]] std::span<const entt::entity> GetList() const override { return list; }
	[[nodiscard]] Summary GetSummary() const override { return {}; }
	void Reset() override {}
};
} // namespace

TEST(SnowCover, ALevelMovesOneStepInAFrameAt60Hz)
{
	EXPECT_EQ(snow_cover::LevelStep(16), 1);
	EXPECT_EQ(snow_cover::LevelStep(17), 1);
	EXPECT_EQ(snow_cover::LevelStep(15), 0);
	EXPECT_EQ(snow_cover::LevelStep(100), 6);
	EXPECT_EQ(snow_cover::EaseLevel(0, 99, 1), 1);
	EXPECT_EQ(snow_cover::EaseLevel(50, 10, 3), 47);
	EXPECT_EQ(snow_cover::EaseLevel(50, 51, 3), 51);
	EXPECT_EQ(snow_cover::EaseLevel(50, 10, 0), 50);
}

TEST(SnowLevels, CreaturesAndBirdsCarryNoSnowAndFieldsAndFlagsCarryLittle)
{
	ecs::Registry registry;
	const auto creature = Thing(registry);
	registry.Assign<Creature>(creature);
	const auto bird = Thing(registry);
	registry.Assign<Animal>(bird).type = AnimalInfo::Pigeon;
	const auto sheep = Thing(registry);
	registry.Assign<Animal>(sheep).type = AnimalInfo::Sheep;
	const auto field = Thing(registry);
	registry.Assign<Field>(field);
	const auto flag = Thing(registry);
	registry.Assign<TownDesireFlag>(flag);

	EXPECT_FALSE(snow_levels::HoldOf(registry, creature).has_value());
	EXPECT_FALSE(snow_levels::HoldOf(registry, bird).has_value());
	EXPECT_EQ(snow_levels::HoldOf(registry, sheep)->cap, 255);
	EXPECT_EQ(snow_levels::HoldOf(registry, field)->rate, 64);
	EXPECT_EQ(snow_levels::HoldOf(registry, field)->cap, 64);
	EXPECT_EQ(snow_levels::HoldOf(registry, flag)->cap, 45);
}

TEST(SnowLevels, ANewThingStartsWithNoSnowAndTakesItInSteps)
{
	ecs::Registry registry;
	const auto thing = Thing(registry);
	const auto creature = Thing(registry);
	registry.Assign<Creature>(creature);
	const std::vector things {thing, creature};

	EXPECT_EQ(snow_levels::LevelOf(registry, thing), 0);
	snow_levels::Step(registry, things, DeepSnow, 1);
	EXPECT_EQ(snow_levels::LevelOf(registry, thing), 1);
	for (int frame = 0; frame < 200; ++frame)
	{
		snow_levels::Step(registry, things, DeepSnow, 1);
	}
	EXPECT_EQ(snow_levels::LevelOf(registry, thing), 99);
	EXPECT_EQ(snow_levels::LevelOf(registry, creature), 0);
	EXPECT_FALSE(registry.AllOf<SnowLevel>(creature));

	// The snow melts: the thing loses it in steps too
	snow_levels::Step(registry, things, [](glm::vec2) { return 0.0f; }, 2);
	EXPECT_EQ(snow_levels::LevelOf(registry, thing), 97);
}

TEST(SnowLevels, AThingWithoutSnowWhereNoSnowLiesGetsNoLevel)
{
	ecs::Registry registry;
	const auto thing = Thing(registry);
	const std::vector things {thing};
	snow_levels::Step(registry, things, [](glm::vec2) { return 10.0f; }, 1);
	EXPECT_FALSE(registry.AllOf<SnowLevel>(thing));
}

TEST(SnowSystem, OnlyTheThingsTheListDrawsTakeSnowAtMost60TimesASecond)
{
	ecs::Registry registry;
	const auto drawn = Thing(registry);
	const auto undrawn = Thing(registry);
	FakeDrawList list;
	list.list = {drawn};

	ecs::systems::SnowSystem snow;
	// Snow 40 deep around the things: they can show 20 of it
	for (int turn = 0; turn < 200; ++turn)
	{
		Storm storm {};
		storm.effect.snow = 100;
		storm.currentStrength = 1.0f;
		storm.currentPosition = {100.0f, 0.0f, 100.0f};
		storm.currentInnerRadius = 200.0f;
		storm.outerRadius = 400.0f;
		const std::vector storms {storm};
		snow.ProcessTurn(storms);
	}
	ASSERT_GT(snow.GetDepth({100.0f, 100.0f}), 30.0f);

	// A frame of 1 ms real time comes before the next step
	snow.StepObjects(registry, list, milliseconds {1}, milliseconds {1});
	EXPECT_EQ(snow_levels::LevelOf(registry, drawn), 0);
	// A full frame at 60 Hz: one step with all the game time since the last
	snow.StepObjects(registry, list, milliseconds {17}, milliseconds {16});
	EXPECT_EQ(snow_levels::LevelOf(registry, drawn), 1);
	// Many frames of 1 ms: they step no more often than 60 times a second
	for (int frame = 0; frame < 17; ++frame)
	{
		snow.StepObjects(registry, list, milliseconds {1}, milliseconds {1});
	}
	EXPECT_EQ(snow_levels::LevelOf(registry, drawn), 2);
	EXPECT_EQ(snow_levels::LevelOf(registry, undrawn), 0);

	// While the game stops, nothing changes
	snow.StepObjects(registry, list, milliseconds {17}, milliseconds {0});
	EXPECT_EQ(snow_levels::LevelOf(registry, drawn), 2);

	// The thing keeps its level while the list does not draw it
	list.list.clear();
	snow.StepObjects(registry, list, milliseconds {17}, milliseconds {16});
	EXPECT_EQ(snow_levels::LevelOf(registry, drawn), 2);
}

TEST(SnowSystem, CoverLaysTheSnowEverywhere)
{
	ecs::systems::SnowSystem snow;
	const auto revision = snow.GetRevision();
	snow.Cover(300.0f);
	EXPECT_FLOAT_EQ(snow.GetDepth({100.0f, 100.0f}), snow_cover::k_MaxDepth);
	EXPECT_NE(snow.GetRevision(), revision);
}

TEST(SnowCover, TheBrokenModelTakesTheWholeMetresOfTheSnowLessTwenty)
{
	EXPECT_EQ(snow_cover::BrokenModelLevel(0.0f), 0);
	EXPECT_EQ(snow_cover::BrokenModelLevel(20.9f), 0);
	EXPECT_EQ(snow_cover::BrokenModelLevel(25.7f), 5);
	EXPECT_EQ(snow_cover::BrokenModelLevel(120.0f), 100);
	EXPECT_EQ(snow_cover::BrokenModelLevel(274.5f), 254);
	EXPECT_EQ(snow_cover::BrokenModelLevel(400.0f), 255);
	// It shows snow from 6, one more than a whole model
	EXPECT_EQ(snow_cover::BrokenModelShownLevel(5), 0);
	EXPECT_EQ(snow_cover::BrokenModelShownLevel(6), 6);
}

TEST(SnowLevels, ATreeDrawnOutOfTheLandCarriesLessSnowFromThenOn)
{
	ecs::Registry registry;
	const auto held = Thing(registry);
	registry.Assign<Tree>(held);
	registry.Assign<InHand>(held);
	const auto planted = Thing(registry);
	registry.Assign<Tree>(planted);
	registry.Assign<MapCellResident>(planted);
	// A tree that the hand holds but that is still in the cells of the map is not out of the land
	registry.Assign<InHand>(planted);

	snow_levels::NoteTreesOutOfLand(registry);
	const auto low = snow_levels::HoldOf(registry, held);
	ASSERT_TRUE(low.has_value());
	EXPECT_EQ(low->rate, snow_cover::k_OutOfLandTreeRate);
	EXPECT_EQ(low->cap, snow_cover::k_OutOfLandTreeCap);
	EXPECT_EQ(snow_levels::HoldOf(registry, planted)->cap, snow_cover::k_ObjectRate);

	// Back in the land, it keeps the lower rate: in snow 120 deep it shows 50, not 99
	registry.Remove<InHand>(held);
	registry.Assign<MapCellResident>(held);
	snow_levels::NoteTreesOutOfLand(registry);
	const std::vector things {held};
	for (int step = 0; step < 100; ++step)
	{
		snow_levels::Step(registry, things, DeepSnow, 1);
	}
	EXPECT_EQ(snow_levels::LevelOf(registry, held), 50);
	// In the deepest snow it shows no more than the lower cap
	for (int step = 0; step < 100; ++step)
	{
		snow_levels::Step(registry, things, [](glm::vec2) { return snow_cover::k_MaxDepth; }, 1);
	}
	EXPECT_EQ(snow_levels::LevelOf(registry, held), snow_cover::k_OutOfLandTreeCap);
}

TEST(SnowLevels, ABrokenBuildingKeepsItsSnowAndItsBrokenModelEasesFromIt)
{
	ecs::Registry registry;
	const auto building = Thing(registry);
	registry.Assign<SnowLevel>(building, SnowLevel {.level = 40});
	snow_levels::FollowBreak(registry, building, true);
	EXPECT_EQ(registry.Get<SnowLevel>(building).broken, std::optional<int32_t>(40));
	EXPECT_EQ(snow_levels::ShownLevelOf(registry, building), 40);

	// The broken model takes all of the snow 120 deep, with no cap: 100
	const std::vector buildings {building};
	for (int step = 0; step < 100; ++step)
	{
		snow_levels::StepBroken(registry, buildings, DeepSnow, 1);
	}
	EXPECT_EQ(registry.Get<SnowLevel>(building).broken, std::optional<int32_t>(100));
	EXPECT_EQ(snow_levels::LevelOf(registry, building), 40);
	EXPECT_EQ(snow_levels::ShownLevelOf(registry, building), 100);

	// A second note of the same break changes nothing
	snow_levels::FollowBreak(registry, building, true);
	EXPECT_EQ(registry.Get<SnowLevel>(building).broken, std::optional<int32_t>(100));

	// Whole again, it shows its own snow
	snow_levels::FollowBreak(registry, building, false);
	EXPECT_FALSE(registry.Get<SnowLevel>(building).broken.has_value());
	EXPECT_EQ(snow_levels::ShownLevelOf(registry, building), 40);

	// A building without snow that breaks gives none to its broken model, which shows none below 6
	const auto bare = Thing(registry);
	snow_levels::FollowBreak(registry, bare, true);
	EXPECT_EQ(registry.Get<SnowLevel>(bare).broken, std::optional<int32_t>(0));
	registry.Get<SnowLevel>(bare).broken = 5;
	EXPECT_EQ(snow_levels::ShownLevelOf(registry, bare), 0);
}

TEST(SnowLevels, ABuildingThatBreaksAgainAfterARepairStartsItsNewBrokenModelWithItsOwnSnow)
{
	ecs::Registry registry;
	const auto building = Thing(registry);
	registry.Assign<SnowLevel>(building, SnowLevel {.level = 40});
	snow_levels::FollowBreak(registry, building, true, 1);
	const std::vector buildings {building};
	for (int step = 0; step < 100; ++step)
	{
		snow_levels::StepBroken(registry, buildings, DeepSnow, 1);
	}
	EXPECT_EQ(registry.Get<SnowLevel>(building).broken, std::optional<int32_t>(100));
	// The same broken model keeps its snow
	snow_levels::FollowBreak(registry, building, true, 1);
	EXPECT_EQ(registry.Get<SnowLevel>(building).broken, std::optional<int32_t>(100));
	// Repaired part of the way and broken again, the building has a new broken model before any frame saw it whole. The
	// new broken model starts with the snow of the building.
	snow_levels::FollowBreak(registry, building, true, 2);
	EXPECT_EQ(registry.Get<SnowLevel>(building).broken, std::optional<int32_t>(40));
	EXPECT_EQ(registry.Get<SnowLevel>(building).brokenModel, 2u);
}

TEST(SnowLevels, AVillagerInHighDetailShowsNoSnowAndKeepsItsLevel)
{
	ecs::Registry registry;
	const auto villager = Thing(registry);
	registry.Assign<SnowLevel>(villager, SnowLevel {.level = 120});
	EXPECT_EQ(snow_levels::ShownLevelOf(registry, villager), 120);

	registry.Assign<HighDetail>(villager);
	EXPECT_EQ(snow_levels::ShownLevelOf(registry, villager), 0);
	EXPECT_EQ(snow_levels::LevelOf(registry, villager), 120);

	// Drawn as usual again, it shows its snow again
	registry.Remove<HighDetail>(villager);
	EXPECT_EQ(snow_levels::ShownLevelOf(registry, villager), 120);
}

TEST(SnowLevels, APieceKeepsTheSnowOfTheBrokenModelThatItBrokeOff)
{
	ecs::Registry registry;
	const auto building = Thing(registry);
	registry.Assign<SnowLevel>(building, SnowLevel {.level = 30, .broken = 70});
	const auto piece = Thing(registry);
	snow_levels::TakePieceSnow(registry, piece, building);
	EXPECT_EQ(snow_levels::LevelOf(registry, piece), 70);
	EXPECT_TRUE(registry.Get<SnowLevel>(piece).piece);

	// It takes the snow one time only
	registry.Get<SnowLevel>(building).broken = 90;
	snow_levels::TakePieceSnow(registry, piece, building);
	EXPECT_EQ(snow_levels::LevelOf(registry, piece), 70);

	// Before the frame notes the break, the broken model has the snow of the building
	const auto whole = Thing(registry);
	registry.Assign<SnowLevel>(whole, SnowLevel {.level = 12});
	const auto early = Thing(registry);
	snow_levels::TakePieceSnow(registry, early, whole);
	EXPECT_EQ(snow_levels::LevelOf(registry, early), 12);

	// A piece without a building takes no snow, and a piece shows none below 6
	const auto orphan = Thing(registry);
	snow_levels::TakePieceSnow(registry, orphan, entt::null);
	EXPECT_EQ(snow_levels::LevelOf(registry, orphan), 0);
	registry.Get<SnowLevel>(orphan).level = 5;
	EXPECT_EQ(snow_levels::ShownLevelOf(registry, orphan), 0);
}

TEST(SnowSystem, ABrokenBuildingInTheListEasesOnlyItsBrokenModel)
{
	ecs::Registry registry;
	const auto building = Thing(registry);
	registry.Assign<SnowLevel>(building, SnowLevel {.level = 3});
	registry.Assign<BuildingDamage>(building);
	const auto piece = Thing(registry);
	registry.Assign<BuildingPiece>(piece, BuildingPiece {.parent = building});
	FakeDrawList list;
	list.list = {building};

	ecs::systems::SnowSystem snow;
	snow.Cover(120.0f);
	snow.StepObjects(registry, list, milliseconds {17}, milliseconds {16});
	EXPECT_EQ(snow_levels::LevelOf(registry, building), 3);
	EXPECT_EQ(registry.Get<SnowLevel>(building).broken, std::optional<int32_t>(4));
	// The piece took the snow of the broken model before the step
	EXPECT_EQ(snow_levels::LevelOf(registry, piece), 3);

	// Whole again, the building loses the snow of its broken model, and its own snow eases again
	registry.Remove<BuildingDamage>(building);
	snow.StepObjects(registry, list, milliseconds {17}, milliseconds {16});
	EXPECT_FALSE(registry.Get<SnowLevel>(building).broken.has_value());
	EXPECT_EQ(snow_levels::LevelOf(registry, building), 4);
}

TEST(SnowLevels, APhantomBuildingCarriesSnowInAHandUntilASiteShowsItAndNoneFromThen)
{
	ecs::Registry registry;
	const auto scaffold = Thing(registry);
	registry.Assign<InHand>(scaffold);
	const auto phantom = Thing(registry);
	registry.Assign<ScaffoldGhost>(phantom, ScaffoldGhost {.scaffold = scaffold});
	// A phantom that shows nothing gets no snow
	snow_levels::StepScaffoldPhantoms(registry, DeepSnow, 1);
	EXPECT_FALSE(registry.AllOf<SnowLevel>(phantom));
	// Shown in the hand it moves towards all of the snow under it, with no cap
	registry.Get<ScaffoldGhost>(phantom).model = Mesh {};
	for (int step = 0; step < 30; ++step)
	{
		snow_levels::StepScaffoldPhantoms(registry, DeepSnow, 1);
	}
	EXPECT_EQ(snow_levels::ShownLevelOf(registry, phantom), 30);
	for (int step = 0; step < 100; ++step)
	{
		snow_levels::StepScaffoldPhantoms(registry, DeepSnow, 1);
	}
	EXPECT_EQ(snow_levels::ShownLevelOf(registry, phantom), 99);
	// A site shows it: it loses its snow, even in a frame with no time
	registry.Remove<InHand>(scaffold);
	snow_levels::StepScaffoldPhantoms(registry, DeepSnow, 0);
	EXPECT_EQ(snow_levels::ShownLevelOf(registry, phantom), 0);
	// From then on it carries none, also in a hand
	registry.Assign<InHand>(scaffold);
	snow_levels::StepScaffoldPhantoms(registry, DeepSnow, 1);
	EXPECT_EQ(snow_levels::ShownLevelOf(registry, phantom), 0);
}
