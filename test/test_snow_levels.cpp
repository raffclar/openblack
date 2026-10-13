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
#include "ECS/Components/Creature.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/SnowLevel.h"
#include "ECS/Components/TownDesireFlag.h"
#include "ECS/Components/Transform.h"
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

	void Update(uint32_t /*unused*/) override {}
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
