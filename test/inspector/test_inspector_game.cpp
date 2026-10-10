/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <algorithm>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <variant>
#include <vector>

#include <3D/SkyFrame.h>
#include <ECS/Components/CreatureObjectAction.h>
#include <ECS/Components/Transform.h>
#include <ECS/Components/Tree.h>
#include <ECS/Components/Villager.h>
#include <ECS/Registry.h>
#include <Graphics/Moon.h>
#include <Inspector.h>
#include <Inspector/ComponentReflection.h>
#include <Inspector/RegistryProviders.h>
#include <Inspector/RunControl.h>
#include <Inspector/WorldProviders.h>
#include <entt/meta/context.hpp>
#include <gtest/gtest.h>

using namespace openblack;
using namespace openblack::inspector;
using namespace openblack::ecs::components;

namespace
{

Json Ask(const Inspector& inspector, const std::string& line)
{
	const auto decoded = DecodeRequest(line);
	EXPECT_TRUE(std::holds_alternative<Request>(decoded));
	const auto answer = inspector.Answer(std::get<Request>(decoded));
	EXPECT_TRUE(answer.Ok()) << answer.error;
	return answer.value;
}

std::set<std::string> Keys(const Json& object)
{
	std::set<std::string> keys;
	for (const auto& [key, unused] : object.items())
	{
		keys.insert(key);
	}
	return keys;
}

/// A sky at a fixed hour and date, and a camera at a fixed place
SkySources FakeSky(float hour)
{
	return {
	    // 2026-01-01
	    .moon = [hour]() -> std::optional<ecs::components::Moon> { return sky_frame::MoonAt(hour, 1767225600); },
	    .cameraOrigin = []() -> std::optional<glm::vec3> { return glm::vec3(100.0f, 50.0f, 200.0f); },
	};
}

ParticleEffectInfo FakeEffect(uint32_t id, std::string file, glm::vec3 origin, size_t atoms)
{
	return {
	    .id = id,
	    .file = std::move(file),
	    .origin = origin,
	    .age = 1.5f,
	    .atoms = atoms,
	    .collections = 1,
	    .closing = false,
	    .ownedBySpell = false,
	    .path = {},
	    .targets = 0,
	    .secondsLeft = std::nullopt,
	    .unportedClasses = {},
	};
}

/// A registry of a few trees along the x axis and a villager among them
class InspectorRegistry: public ::testing::Test
{
protected:
	void SetUp() override
	{
		reflection::RegisterComponents(_reflection);
		for (int i = 0; i < 5; ++i)
		{
			const auto tree = _registry.Create();
			_registry.Assign<Transform>(tree, glm::vec3(i * 10.0f, 0.0f, 0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
			_registry.Assign<Tree>(tree, static_cast<TreeInfo>(i), 2.0f, 1u);
			_trees.push_back(tree);
		}
		_villager = _registry.Create();
		_registry.Assign<Transform>(_villager, glm::vec3(21.0f, 0.0f, 0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
		auto& villager = _registry.Assign<Villager>(_villager);
		villager.lifeStage = Villager::LifeStage::Adult;
		villager.sex = Villager::Sex::FEMALE;

		const RegistrySources sources {.registry = [this]() -> const ecs::Registry* { return &_registry; },
		                               .info = []() -> const InfoConstants* { return nullptr; }};
		_inspector.Add(std::make_unique<RegistryProvider>(sources, _reflection));
		_inspector.Add(std::make_unique<ObjectsProvider>(sources));
	}

	ecs::Registry _registry;
	entt::meta_ctx _reflection;
	Inspector _inspector;
	std::vector<entt::entity> _trees;
	entt::entity _villager {entt::null};
};

/// A clock and a list of scenarios, with no game behind them
class FakeRunTarget final: public RunTargetInterface
{
public:
	[[nodiscard]] bool IsPaused() const override { return paused; }
	void SetPaused(bool value) override { paused = value; }
	[[nodiscard]] uint32_t GetTurn() const override { return turn; }
	[[nodiscard]] float GetSpeed() const override { return speed; }
	void SetSpeed(float value) override { speed = value; }
	void SetFixedFrameTime(std::optional<uint32_t> milliseconds) override { fixedMs = milliseconds; }
	[[nodiscard]] std::optional<uint32_t> GetFixedFrameTime() const override { return fixedMs; }
	bool LoadScenario(std::string_view id) override
	{
		if (id != "idle.one")
		{
			return false;
		}
		loaded = id;
		return true;
	}
	[[nodiscard]] std::vector<ScenarioSummary> Scenarios() const override
	{
		return {{.id = "idle.one", .name = "One", .facet = "Idle", .description = "A creature idles"}};
	}
	[[nodiscard]] uint32_t GetSeed() const override { return seed; }
	void SetSeed(uint32_t value, std::optional<int64_t> pinned) override
	{
		seed = value;
		date = pinned;
		ticks = 0;
	}
	[[nodiscard]] std::optional<int64_t> GetPinnedDate() const override { return date; }
	[[nodiscard]] uint32_t GetTicks() const override { return ticks; }

	uint32_t seed {12345};
	std::optional<int64_t> date;
	uint32_t ticks {999};
	bool paused {true};
	uint32_t turn {10};
	float speed {1.0f};
	std::optional<uint32_t> fixedMs;
	std::string loaded;
};

} // namespace

// The user's example: the moon's position returns just the moon and its state, nothing else of the sky
TEST(InspectorMoon, MoonQueryIsJustTheMoon)
{
	Inspector inspector;
	inspector.Add(std::make_unique<SkyProvider>(FakeSky(0.0f)));

	const auto moon = Ask(inspector, R"({"query": "sky.moon"})");
	EXPECT_EQ(Keys(moon), (std::set<std::string> {"position", "phase", "cycle_day", "visible"}));
	EXPECT_EQ(moon["visible"], true);

	// At midnight it is up, beside the camera at its place for the hour
	const auto placement = graphics::moon::Place(0.0f);
	ASSERT_TRUE(placement.has_value());
	const glm::vec3 expected = glm::vec3(100.0f, 50.0f, 200.0f) + placement->offset;
	EXPECT_FLOAT_EQ(moon["position"][0].get<float>(), expected.x);
	EXPECT_FLOAT_EQ(moon["position"][1].get<float>(), expected.y);
	EXPECT_FLOAT_EQ(moon["position"][2].get<float>(), expected.z);
	EXPECT_FLOAT_EQ(moon["phase"].get<float>(), graphics::moon::Phase(1767225600));
	EXPECT_GE(moon["cycle_day"].get<int>(), 0);
	EXPECT_LE(moon["cycle_day"].get<int>(), 29);
}

TEST(InspectorMoon, AtNoonTheMoonIsDown)
{
	const auto moon = MoonState(12.0f, 1767225600, glm::vec3(0.0f));
	EXPECT_EQ(moon["visible"], false);
	EXPECT_TRUE(moon["position"].is_null());
}

TEST(InspectorMoon, CycleDayCountsFromTheNewMoon)
{
	// A day's seconds apart, the days since the new moon go up by one, or wrap round to 0
	const int64_t day = 86400;
	const int64_t start = 1767225600;
	const auto first = MoonState(0.0f, start, glm::vec3(0.0f))["cycle_day"].get<int>();
	const auto next = MoonState(0.0f, start + day, glm::vec3(0.0f))["cycle_day"].get<int>();
	EXPECT_TRUE(next == first + 1 || next == 0) << first << " then " << next;
}

// The user's example: a splash at a position within a radius, and only the splashes there with their state
TEST(InspectorSplash, SplashAtAPositionWithinARadius)
{
	std::vector<water_rings::Ring> rings(2);
	rings[0].position = glm::vec3(10.0f, 0.0f, 10.0f);
	rings[0].age = 200;
	rings[1].position = glm::vec3(500.0f, 0.0f, 500.0f);
	const std::vector<ParticleEffectInfo> effects {
	    FakeEffect(7, "SF_SplashBig", glm::vec3(12.0f, 1.0f, 10.0f), 40),
	    FakeEffect(8, "SF_Smoke", glm::vec3(10.0f, 0.0f, 10.0f), 90),
	    FakeEffect(9, "SF_Splash", glm::vec3(300.0f, 0.0f, 0.0f), 10),
	};
	Inspector inspector;
	inspector.Add(std::make_unique<ParticlesProvider>(ParticleSources {
	    .rings = [&rings]() { return std::span<const water_rings::Ring>(rings); },
	    .effects = [&effects]() { return effects; },
	}));

	const auto found = Ask(inspector, R"({"query": "particles.splash", "near": [10, 0, 10], "radius": 5})");
	ASSERT_EQ(found["total"], 2);
	// The ring right at the point first, then the splash effect two units off; not the smoke, nor the far splashes
	const auto& ring = found["items"][0];
	EXPECT_EQ(ring["kind"], "ring");
	EXPECT_EQ(ring["age_ms"], 200);
	EXPECT_EQ(ring["life_ms"], water_rings::k_Life);
	EXPECT_DOUBLE_EQ(ring["distance"].get<double>(), 0.0);
	const auto& effect = found["items"][1];
	EXPECT_EQ(effect["kind"], "effect");
	EXPECT_EQ(effect["id"], 7);
	EXPECT_EQ(effect["particles"], 40);
	EXPECT_EQ(effect["closing"], false);

	// The splash query needs a point to search around
	const auto decoded = DecodeRequest(R"({"query": "particles.splash"})");
	EXPECT_FALSE(inspector.Answer(std::get<Request>(decoded)).Ok());
}

// The user's example: objects of a type within a radius, nearest first, with only their id, label, place and distance
TEST_F(InspectorRegistry, ObjectsOfATypeWithinARadiusNearestFirst)
{
	const auto found = Ask(_inspector, R"({"query": "objects.find", "params": {"component": "Tree"},
	                                      "near": [22, 0, 0], "radius": 15})");
	// Trees at 10, 20 and 30 are within 15 of 22: 20 first, then 30, then 10. The villager at 21 is no tree.
	ASSERT_EQ(found["total"], 3);
	const auto& items = found["items"];
	EXPECT_EQ(items[0]["id"], ToId(_trees[2]));
	EXPECT_EQ(items[1]["id"], ToId(_trees[3]));
	EXPECT_EQ(items[2]["id"], ToId(_trees[1]));
	EXPECT_EQ(Keys(items[0]), (std::set<std::string> {"id", "kind", "label", "position", "distance"}));
	EXPECT_EQ(items[0]["kind"], "Tree");
	EXPECT_DOUBLE_EQ(items[0]["distance"].get<double>(), 2.0);

	const auto villagers = Ask(_inspector, R"({"query": "objects.find", "params": {"kind": "villager"},
	                                          "near": [0, 0], "radius": 100})");
	ASSERT_EQ(villagers["total"], 1);
	EXPECT_EQ(villagers["items"][0]["label"], "Villager: Adult FEMALE, number 0");
}

TEST_F(InspectorRegistry, EntitiesAreIdsAndLabelsByDefault)
{
	const auto all = Ask(_inspector, R"({"query": "ecs.entities", "limit": 2})");
	EXPECT_EQ(all["total"], 6);
	EXPECT_EQ(all["items"].size(), 2u);
	EXPECT_FALSE(all["items"][0].contains("components"));

	const auto count = Ask(_inspector, R"({"query": "ecs.entities", "params": {"component": ["Tree", "Transform"]},
	                                      "count": true})");
	EXPECT_EQ(count, Json({{"count", 5}}));
}

TEST_F(InspectorRegistry, ComponentsAndFieldsOnRequest)
{
	const auto id = std::to_string(ToId(_trees[1]));
	const auto plain = Ask(_inspector, R"({"query": "ecs.entity", "params": {"id": )" + id + "}}");
	EXPECT_FALSE(plain.contains("data"));
	EXPECT_NE(std::find(plain["components"].begin(), plain["components"].end(), "Tree"), plain["components"].end());

	const auto tree =
	    Ask(_inspector, R"({"query": "ecs.entity", "params": {"id": )" + id + R"(, "components": ["Tree", "Transform"]}})");
	EXPECT_EQ(tree["data"]["Tree"]["type"], 1);
	EXPECT_FLOAT_EQ(tree["data"]["Tree"]["maxSize"].get<float>(), 2.0f);
	EXPECT_EQ(tree["data"]["Transform"]["position"], Json({10.0f, 0.0f, 0.0f}));

	// Only the fields asked for
	const auto field = Ask(_inspector, R"({"query": "ecs.entity", "params": {"id": )" + id +
	                                       R"(, "components": "all"}, "fields": ["data.Tree.maxSize"]})");
	EXPECT_EQ(Keys(field), std::set<std::string> {"data"});
	EXPECT_EQ(Keys(field["data"]), std::set<std::string> {"Tree"});

	const auto components = Ask(_inspector, R"({"query": "ecs.components", "where": [{"field": "name", "value": "Tree"}]})");
	ASSERT_EQ(components["total"], 1);
	EXPECT_EQ(components["items"][0]["count"], 5);
	EXPECT_EQ(components["items"][0]["reflected"], true);
}

// What still points at an entity, through any component field that holds entities, even after the entity has gone
TEST_F(InspectorRegistry, ReferencesToAnEntityAreFoundByField)
{
	const auto holder = _trees[1];
	const auto held = _trees[2];
	_registry.Assign<ecs::components::HeldByCreature>(_villager, ecs::components::HeldByCreature {.creature = holder});
	const auto id = std::to_string(ToId(holder));

	const auto found = Ask(_inspector, R"({"query": "ecs.references", "params": {"id": )" + id + "}}");
	ASSERT_EQ(found["total"], 1);
	EXPECT_EQ(found["items"][0]["id"], ToId(_villager));
	EXPECT_EQ(found["items"][0]["component"], "HeldByCreature");
	EXPECT_EQ(found["items"][0]["field"], "creature");

	_registry.Destroy(holder);
	EXPECT_EQ(Ask(_inspector, R"({"query": "ecs.references", "params": {"id": )" + id + "}}")["total"], 1);
	_registry.Remove<ecs::components::HeldByCreature>(_villager);
	EXPECT_EQ(Ask(_inspector, R"({"query": "ecs.references", "params": {"id": )" + id + "}}")["total"], 0);
	EXPECT_EQ(Ask(_inspector, R"({"query": "ecs.references", "params": {"id": )" + std::to_string(ToId(held)) + "}}")["total"],
	          0);
}

// Two runs that end with everything in the same place hash the same; anything moved changes the hash
TEST_F(InspectorRegistry, HashOfWhereEverythingIs)
{
	const auto first = Ask(_inspector, R"({"query": "ecs.hash"})");
	EXPECT_EQ(first["entities"], 6);
	EXPECT_EQ(Ask(_inspector, R"({"query": "ecs.hash"})")["hash"], first["hash"]);

	_registry.Get<Transform>(_trees[1]).position.x += 0.001f;
	const auto moved = Ask(_inspector, R"({"query": "ecs.hash"})");
	EXPECT_NE(moved["hash"], first["hash"]);
	_registry.Get<Transform>(_trees[1]).position.x -= 0.001f;

	// A component's fields count when named
	const auto plain = Ask(_inspector, R"({"query": "ecs.hash", "params": {"component": "Tree"}})");
	_registry.Get<Tree>(_trees[1]).maxSize = 3.0f;
	EXPECT_NE(Ask(_inspector, R"({"query": "ecs.hash", "params": {"component": "Tree"}})")["hash"], plain["hash"]);
}

// Entities with a component excluded are left out, as the hand is when comparing runs
TEST_F(InspectorRegistry, HashLeavesOutExcludedEntities)
{
	const auto without = Ask(_inspector, R"({"query": "ecs.hash", "params": {"exclude": ["Villager"]}})");
	EXPECT_EQ(without["entities"], 5);
	_registry.Get<Transform>(_villager).position.x += 1.0f;
	EXPECT_EQ(Ask(_inspector, R"({"query": "ecs.hash", "params": {"exclude": ["Villager"]}})")["hash"], without["hash"]);
	const auto decoded = DecodeRequest(R"({"query": "ecs.hash", "params": {"exclude": ["Nope"]}})");
	EXPECT_FALSE(_inspector.Answer(std::get<Request>(decoded)).Ok());
}

TEST_F(InspectorRegistry, UnknownComponentsAndEntitiesAreExplained)
{
	auto decoded = DecodeRequest(R"({"query": "ecs.entities", "params": {"component": "Nope"}})");
	EXPECT_FALSE(_inspector.Answer(std::get<Request>(decoded)).Ok());
	decoded = DecodeRequest(R"({"query": "ecs.entity", "params": {"id": 123456}})");
	EXPECT_FALSE(_inspector.Answer(std::get<Request>(decoded)).Ok());
}

TEST(InspectorReflection, ShortTypeNames)
{
	EXPECT_EQ(reflection::ShortTypeName(entt::type_id<Transform>()), "Transform");
	EXPECT_EQ(reflection::ShortTypeName(entt::type_id<Villager>()), "Villager");
}

TEST(InspectorRunControl, StepsFramesThenPauses)
{
	RunControl control;
	control.StepFrames(2);
	EXPECT_EQ(control.Frame(true, 0), false);
	EXPECT_EQ(control.Frame(false, 0), std::nullopt);
	EXPECT_EQ(control.Frame(false, 0), true);
	EXPECT_FALSE(control.Stepping());
	EXPECT_EQ(control.Frame(true, 0), std::nullopt);
}

TEST(InspectorRunControl, StepsTurnsThenPauses)
{
	RunControl control;
	control.StepTurns(3, 10);
	EXPECT_EQ(control.Frame(true, 10), false);
	EXPECT_EQ(control.Frame(false, 11), std::nullopt);
	EXPECT_EQ(control.Frame(false, 12), std::nullopt);
	EXPECT_EQ(control.Frame(false, 13), true);
	EXPECT_FALSE(control.Stepping());
}

TEST(InspectorRunControl, GameQueriesDriveTheClock)
{
	FakeRunTarget target;
	Inspector inspector;
	auto provider = std::make_unique<GameProvider>(target);
	auto* game = provider.get();
	inspector.Add(std::move(provider));

	EXPECT_EQ(Ask(inspector, R"({"query": "game.resume"})")["paused"], false);
	EXPECT_EQ(Ask(inspector, R"({"query": "game.pause"})")["paused"], true);

	const auto stepping = Ask(inspector, R"({"query": "game.step", "params": {"turns": 2}})");
	EXPECT_EQ(stepping["stepping"]["until_turn"], 12);
	game->Frame();
	EXPECT_FALSE(target.paused);
	target.turn = 12;
	game->Frame();
	EXPECT_TRUE(target.paused);
	EXPECT_TRUE(Ask(inspector, R"({"query": "game.state"})")["stepping"].is_null());

	EXPECT_FLOAT_EQ(Ask(inspector, R"({"query": "game.speed", "params": {"speed": 2}})")["speed"].get<float>(), 2.0f);
	EXPECT_EQ(Ask(inspector, R"({"query": "game.scenario", "params": {"id": "idle.one"}})")["loading"], "idle.one");
	EXPECT_EQ(target.loaded, "idle.one");
	EXPECT_EQ(Ask(inspector, R"({"query": "game.scenarios"})")["items"][0]["id"], "idle.one");

	auto decoded = DecodeRequest(R"({"query": "game.step", "params": {"frames": 1, "turns": 1}})");
	EXPECT_FALSE(inspector.Answer(std::get<Request>(decoded)).Ok());
	decoded = DecodeRequest(R"({"query": "game.scenario", "params": {"id": "nope"}})");
	EXPECT_FALSE(inspector.Answer(std::get<Request>(decoded)).Ok());
}

// A step with a fixed frame time holds the frames to it while stepping, then gives the wall clock back
TEST(InspectorRunControl, StepWithAFixedFrameTimeGivesItBackAfter)
{
	FakeRunTarget target;
	Inspector inspector;
	auto provider = std::make_unique<GameProvider>(target);
	auto* game = provider.get();
	inspector.Add(std::move(provider));

	const auto stepping = Ask(inspector, R"({"query": "game.step", "params": {"frames": 2, "fixed_ms": 16}})");
	EXPECT_EQ(stepping["fixed_ms"], 16);
	game->Frame();
	game->Frame();
	EXPECT_EQ(target.fixedMs, 16u);
	game->Frame();
	EXPECT_TRUE(target.paused);
	EXPECT_FALSE(target.fixedMs.has_value());

	// Set for good, it stays after steps
	EXPECT_EQ(Ask(inspector, R"({"query": "game.frame_time", "params": {"ms": 10}})")["fixed_ms"], 10);
	Ask(inspector, R"({"query": "game.step", "params": {"frames": 1, "fixed_ms": 20}})");
	game->Frame();
	game->Frame();
	EXPECT_EQ(target.fixedMs, 10u);
	EXPECT_TRUE(Ask(inspector, R"({"query": "game.frame_time", "params": {"ms": 0}})")["fixed_ms"].is_null());

	const auto decoded = DecodeRequest(R"({"query": "game.step", "params": {"frames": 1, "fixed_ms": 0}})");
	EXPECT_FALSE(inspector.Answer(std::get<Request>(decoded)).Ok());
}

// Once a step has run, the frame time it ran with is gone back to what it was: game.state keeps the last step, with
// the fixed frame time it ran with and where it started and ended, so that tools waiting for it can report it
TEST(InspectorRunControl, TheLastStepIsReportedWithItsFixedFrameTime)
{
	FakeRunTarget target;
	Inspector inspector;
	auto provider = std::make_unique<GameProvider>(target);
	auto* game = provider.get();
	inspector.Add(std::move(provider));
	EXPECT_TRUE(Ask(inspector, R"({"query": "game.state"})")["last_step"].is_null());
	EXPECT_EQ(Ask(inspector, R"({"query": "game.state"})")["ready"], true);

	const auto stepping = Ask(inspector, R"({"query": "game.step", "params": {"frames": 2, "fixed_ms": 16}})");
	EXPECT_EQ(stepping["stepping"]["fixed_ms"], 16);
	EXPECT_EQ(stepping["last_step"]["done"], false);
	game->Frame();
	target.turn = 11;
	game->Frame();
	game->Frame();
	const auto state = Ask(inspector, R"({"query": "game.state"})");
	EXPECT_TRUE(state["stepping"].is_null());
	EXPECT_TRUE(state["fixed_ms"].is_null());
	const auto& last = state["last_step"];
	EXPECT_EQ(last["done"], true);
	EXPECT_EQ(last["frames"], 2);
	EXPECT_EQ(last["fixed_ms"], 16);
	EXPECT_EQ(last["from_frame"], 0);
	EXPECT_EQ(last["to_frame"], 3);
	EXPECT_EQ(last["from_turn"], 10);
	EXPECT_EQ(last["to_turn"], 11);

	// A step without a fixed time reports the frame time set for good, or none
	Ask(inspector, R"({"query": "game.step", "params": {"turns": 1}})");
	EXPECT_TRUE(Ask(inspector, R"({"query": "game.state"})")["last_step"]["fixed_ms"].is_null());
	Ask(inspector, R"({"query": "game.frame_time", "params": {"ms": 10}})");
	Ask(inspector, R"({"query": "game.step", "params": {"frames": 1}})");
	EXPECT_EQ(Ask(inspector, R"({"query": "game.state"})")["last_step"]["fixed_ms"], 10);
}

// game.seed reads the run's seed; given one, every random number starts again from it and the date is pinned
TEST(InspectorRunControl, SeedStartsTheRunAgainFromASeed)
{
	FakeRunTarget target;
	Inspector inspector;
	inspector.Add(std::make_unique<GameProvider>(target));

	const auto now = Ask(inspector, R"({"query": "game.seed"})");
	EXPECT_EQ(now["seed"], 12345);
	EXPECT_TRUE(now["date"].is_null());
	EXPECT_EQ(now["ticks"], 999);

	const auto seeded = Ask(inspector, R"({"query": "game.seed", "params": {"seed": 42}})");
	EXPECT_EQ(seeded["seed"], 42);
	EXPECT_EQ(seeded["date"], GameProvider::k_SeededDate);
	EXPECT_EQ(seeded["ticks"], 0);

	EXPECT_EQ(Ask(inspector, R"({"query": "game.seed", "params": {"seed": 7, "date": 1000}})")["date"], 1000);
	const auto wall = Ask(inspector, R"({"query": "game.seed", "params": {"seed": 7, "wall_clock": true}})");
	EXPECT_TRUE(wall["date"].is_null());
	EXPECT_EQ(target.seed, 7u);

	for (const auto* refused :
	     {R"({"query": "game.seed", "params": {"seed": -1}})", R"({"query": "game.seed", "params": {"seed": 1.5}})",
	      R"({"query": "game.seed", "params": {"seed": 4294967296}})", R"({"query": "game.seed", "params": {"date": 5}})",
	      R"({"query": "game.seed", "params": {"seed": 1, "date": 5, "wall_clock": true}})"})
	{
		const auto decoded = DecodeRequest(refused);
		EXPECT_FALSE(inspector.Answer(std::get<Request>(decoded)).Ok()) << refused;
	}
	EXPECT_EQ(target.seed, 7u);
}

// In a seeded run each land loaded starts paused, before any of its frames, so that the turns after are stepped exactly
TEST(InspectorRunControl, ASeededRunStartsEachLoadPaused)
{
	FakeRunTarget target;
	Inspector inspector;
	auto provider = std::make_unique<GameProvider>(target);
	auto* game = provider.get();
	inspector.Add(std::move(provider));

	target.paused = false;
	game->Loaded();
	EXPECT_FALSE(target.paused);

	EXPECT_EQ(Ask(inspector, R"({"query": "game.seed", "params": {"seed": 3}})")["pause_on_load"], true);
	game->Loaded();
	EXPECT_TRUE(target.paused);

	Ask(inspector, R"({"query": "game.seed", "params": {"seed": 3, "pause_on_load": false}})");
	target.paused = false;
	game->Loaded();
	EXPECT_FALSE(target.paused);
	const auto decoded = DecodeRequest(R"({"query": "game.seed", "params": {"pause_on_load": true}})");
	EXPECT_FALSE(inspector.Answer(std::get<Request>(decoded)).Ok());
}
