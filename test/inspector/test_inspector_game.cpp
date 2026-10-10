/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <algorithm>
#include <array>
#include <bitset>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>
#include <vector>

#include <3D/SkyFrame.h>
#include <Creature/CreatureDesires.h>
#include <Creature/CreatureFight.h>
#include <ECS/Components/CreatureFight.h>
#include <ECS/Components/CreatureNeeds.h>
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
			_registry.Assign<Tree>(tree, static_cast<TreeInfo>(i), 2.0f);
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

namespace
{
struct Cell
{
	float height {0.0f};
	int32_t owner {0};
};
struct Holder
{
	std::vector<Cell> cells;
	std::array<Cell, 2> pair {};
	std::unordered_map<int32_t, float> weights;
	std::map<std::string, Cell> named;
	std::vector<std::vector<int32_t>> grid;
	float scale {0.1f};
};
} // namespace

// Containers are written as what they hold, briefly: lists as arrays, maps as their first entries, plain numbers as
// numbers, floats in their shortest form; never as a type's name
TEST(InspectorReflection, ContainersAreWrittenAsWhatTheyHold)
{
	entt::meta_ctx context;
	reflection::Reflect<Cell>(context).Field<&Cell::height>("height").Field<&Cell::owner>("owner");
	reflection::Reflect<Holder>(context)
	    .Field<&Holder::cells>("cells")
	    .Field<&Holder::pair>("pair")
	    .Field<&Holder::weights>("weights")
	    .Field<&Holder::named>("named")
	    .Field<&Holder::grid>("grid")
	    .Field<&Holder::scale>("scale");
	Holder holder;
	holder.cells = {{.height = 1.5f, .owner = 2}};
	holder.pair = {Cell {.height = 0.1f, .owner = 1}, Cell {.height = 2.0f, .owner = 0}};
	holder.weights = {{3, 0.25f}};
	holder.named = {{"a", Cell {.height = 4.0f, .owner = 7}}};
	holder.grid = {{1, 2}, {3}};
	const auto json = reflection::ComponentToJson(context, entt::type_id<Holder>(), &holder);
	EXPECT_EQ(json["cells"], Json::parse(R"([{"height": 1.5, "owner": 2}])"));
	EXPECT_EQ(json["pair"], Json::parse(R"([{"height": 0.1, "owner": 1}, {"height": 2.0, "owner": 0}])"));
	EXPECT_EQ(json["weights"], Json::parse(R"({"3": 0.25})"));
	EXPECT_EQ(json["named"], Json::parse(R"({"a": {"height": 4.0, "owner": 7}})"));
	EXPECT_EQ(json["grid"], Json::parse(R"([[1, 2], [3]])"));
	// A float's shortest form, not its double's digits
	EXPECT_EQ(json["scale"].dump(), "0.1");
	EXPECT_EQ(Dump(json).find('<'), std::string::npos) << Dump(json);

	// Long maps give their size and first entries
	for (int32_t i = 0; i < 40; ++i)
	{
		holder.weights[i] = 1.0f;
	}
	const auto many = reflection::ComponentToJson(context, entt::type_id<Holder>(), &holder)["weights"];
	EXPECT_EQ(many["size"], 40);
	EXPECT_EQ(many["first"].size(), reflection::k_MostElements);
}

TEST(InspectorReflection, ShortTypeNames)
{
	EXPECT_EQ(reflection::ShortTypeName(entt::type_id<Transform>()), "Transform");
	EXPECT_EQ(reflection::ShortTypeName(entt::type_id<Villager>()), "Villager");
}

namespace
{
enum class Colour : uint8_t
{
	Red,
	Green,
	Blue,
	_Count,
};
constexpr std::array<std::string_view, 3> k_ColourNames {"Red", "Green", "Blue"};

struct Leaf
{
	float value {0.0f};
	std::vector<int32_t> marks;
};

struct Orchard
{
	std::array<Leaf, 3> byColour {};
	std::array<float, 3> weights {};
	std::optional<Leaf> spare;
	std::vector<Leaf> leaves;
	std::array<std::array<int32_t, 3>, 2> grid {};
	std::bitset<3> flags;
	std::u16string name {u"Ogré"};
};

/// A value whose data is private, reached through functions
class Box
{
public:
	[[nodiscard]] const std::vector<Leaf>& Items() const { return _items; }
	void Replace(std::vector<Leaf> items) { _items = std::move(items); }

private:
	std::vector<Leaf> _items;
};

std::vector<Leaf> BoxItems(const Box& box)
{
	return box.Items();
}

void SetBoxItems(Box& box, const std::vector<Leaf>& items)
{
	box.Replace(items);
}

struct Crate
{
	Box box;
};

void RegisterOrchard(entt::meta_ctx& context)
{
	reflection::Reflect<Leaf>(context, reflection::ValueOnly {}).Field<&Leaf::value>("value").Field<&Leaf::marks>("marks");
	reflection::Reflect<Orchard>(context)
	    .Field<&Orchard::byColour>("byColour", {k_ColourNames})
	    .Field<&Orchard::weights>("weights", {k_ColourNames})
	    .Field<&Orchard::spare>("spare")
	    .Field<&Orchard::leaves>("leaves")
	    .Field<&Orchard::grid>("grid", {std::span<const std::string_view> {}, k_ColourNames})
	    .Field<&Orchard::flags>("flags")
	    .Field<&Orchard::name>("name");
	reflection::Reflect<Box>(context, reflection::ValueOnly {}).Property<&BoxItems, &SetBoxItems>("items");
	reflection::Reflect<Crate>(context).Field<&Crate::box>("box");
}

std::string Set(const entt::meta_ctx& context, Orchard& orchard, std::string_view path, const std::string& json)
{
	return reflection::SetField(context, entt::type_id<Orchard>(), &orchard, path, Json::parse(json));
}

std::optional<Json> Read(const entt::meta_ctx& context, const Orchard& orchard, std::string_view path)
{
	return reflection::ReadField(context, entt::type_id<Orchard>(), &orchard, path);
}
} // namespace

// A list indexed by an enumeration is written by its names, and its elements are read and set by name or by number
TEST(InspectorReflection, ListsIndexedByAnEnumerationGoByItsNames)
{
	entt::meta_ctx context;
	RegisterOrchard(context);
	Orchard orchard;
	orchard.weights = {0.5f, 1.0f, 2.0f};

	const auto json = reflection::ComponentToJson(context, entt::type_id<Orchard>(), &orchard);
	EXPECT_EQ(json["weights"], Json::parse(R"({"Red": 0.5, "Green": 1.0, "Blue": 2.0})"));
	EXPECT_EQ(json["byColour"]["Green"], Json::parse(R"({"value": 0.0, "marks": []})"));
	// Lists within lists: by number, then by name
	EXPECT_EQ(json["grid"][1], Json::parse(R"({"Red": 0, "Green": 0, "Blue": 0})"));

	EXPECT_EQ(Set(context, orchard, "byColour.Green.value", "2.5"), "");
	EXPECT_FLOAT_EQ(orchard.byColour[1].value, 2.5f);
	EXPECT_EQ(Read(context, orchard, "byColour.Green.value"), Json(2.5));
	EXPECT_EQ(Set(context, orchard, "weights.2", "4"), "");
	EXPECT_FLOAT_EQ(orchard.weights[2], 4.0f);
	EXPECT_EQ(Read(context, orchard, "weights.Blue"), Json(4.0));
	EXPECT_EQ(Set(context, orchard, "grid.1.Blue", "9"), "");
	EXPECT_EQ(orchard.grid[1][2], 9);
	EXPECT_EQ(Read(context, orchard, "grid.1"), Json::parse(R"({"Red": 0, "Green": 0, "Blue": 9})"));

	// No such element: the names it has are given
	EXPECT_NE(Set(context, orchard, "weights.Purple", "1").find("Red, Green, Blue"), std::string::npos);
	EXPECT_FALSE(Set(context, orchard, "weights.3", "1").empty());
	EXPECT_FALSE(Set(context, orchard, "weights.Red", "\"heavy\"").empty());
	EXPECT_FALSE(Read(context, orchard, "weights.Purple").has_value());
	// A whole list from an array of its size
	EXPECT_EQ(Set(context, orchard, "weights", "[1, 2, 3]"), "");
	EXPECT_EQ(orchard.weights, (std::array<float, 3> {1.0f, 2.0f, 3.0f}));
	EXPECT_FALSE(Set(context, orchard, "weights", "[1, 2]").empty());
}

// Sets of bits are arrays of truths, and text of 16-bit units is written as UTF-8
TEST(InspectorReflection, BitsAndWideTextAreWrittenAsWhatTheyHold)
{
	entt::meta_ctx context;
	RegisterOrchard(context);
	Orchard orchard;
	orchard.flags.set(1);
	const auto json = reflection::ComponentToJson(context, entt::type_id<Orchard>(), &orchard);
	EXPECT_EQ(json["flags"], Json::parse("[false, true, false]"));
	EXPECT_EQ(json["name"].get<std::string>(), std::string("Ogr\xC3\xA9"));
	EXPECT_EQ(Set(context, orchard, "flags", "[true, false, true]"), "");
	EXPECT_TRUE(orchard.flags.test(0));
	EXPECT_FALSE(orchard.flags.test(1));
	EXPECT_FALSE(Set(context, orchard, "flags", "[true]").empty());
}

// An option is written as what it holds or null, made when a field inside it is set, and emptied by null; lists of
// values with fields are set from arrays of objects, taking the array's size
TEST(InspectorReflection, OptionsAndListsOfValuesAreSetThroughTheirElements)
{
	entt::meta_ctx context;
	RegisterOrchard(context);
	Orchard orchard;

	EXPECT_EQ(reflection::ComponentToJson(context, entt::type_id<Orchard>(), &orchard)["spare"], Json(nullptr));
	EXPECT_FALSE(Read(context, orchard, "spare.value").has_value());
	EXPECT_EQ(Set(context, orchard, "spare.value", "3"), "");
	ASSERT_TRUE(orchard.spare.has_value());
	EXPECT_FLOAT_EQ(orchard.spare->value, 3.0f);
	EXPECT_EQ(reflection::ComponentToJson(context, entt::type_id<Orchard>(), &orchard)["spare"],
	          Json::parse(R"({"value": 3.0, "marks": []})"));
	EXPECT_EQ(Set(context, orchard, "spare", "null"), "");
	EXPECT_FALSE(orchard.spare.has_value());

	EXPECT_EQ(Set(context, orchard, "leaves", R"([{"value": 1}, {"value": 2, "marks": [4, 5]}])"), "");
	ASSERT_EQ(orchard.leaves.size(), 2u);
	EXPECT_EQ(orchard.leaves[1].marks, (std::vector<int32_t> {4, 5}));
	EXPECT_EQ(Read(context, orchard, "leaves.1.marks.0"), Json(4));
	EXPECT_EQ(Set(context, orchard, "leaves.1.marks.0", "7"), "");
	EXPECT_EQ(orchard.leaves[1].marks[0], 7);
	EXPECT_FALSE(Set(context, orchard, "leaves.2.value", "1").empty());
	EXPECT_EQ(Set(context, orchard, "leaves", "[]"), "");
	EXPECT_TRUE(orchard.leaves.empty());
}

// A value whose data is private is read through its getter, and set by changing what it gives and setting it back
TEST(InspectorReflection, PropertiesAreReadAndSetBackWhole)
{
	entt::meta_ctx context;
	RegisterOrchard(context);
	Crate crate;
	const auto type = entt::type_id<Crate>();

	EXPECT_EQ(reflection::SetField(context, type, &crate, "box.items", Json::parse(R"([{"value": 5}])")), "");
	ASSERT_EQ(crate.box.Items().size(), 1u);
	EXPECT_EQ(reflection::ReadField(context, type, &crate, "box.items.0.value"), Json(5.0));
	EXPECT_EQ(reflection::SetField(context, type, &crate, "box.items.0.value", Json(6)), "");
	EXPECT_FLOAT_EQ(crate.box.Items()[0].value, 6.0f);
	EXPECT_EQ(reflection::ComponentToJson(context, type, &crate)["box"]["items"],
	          Json::parse(R"([{"value": 6.0, "marks": []}])"));
}

// The game's components: a creature's body needs, a fight's fighter (its health, control and queue of moves) and the
// desires by name are registered with the values they hold, never written as a type's name
TEST(InspectorReflection, TheGamesValuesAreRegisteredWithTheirComponents)
{
	entt::meta_ctx context;
	reflection::RegisterComponents(context);

	CreatureNeeds needs;
	const auto needsType = entt::type_id<CreatureNeeds>();
	EXPECT_EQ(reflection::SetField(context, needsType, &needs, "needs.life", Json(0.25)), "");
	EXPECT_FLOAT_EQ(needs.needs.life, 0.25f);
	EXPECT_EQ(reflection::ComponentToJson(context, needsType, &needs)["needs"]["life"], Json(0.25));

	CreatureFighting fighting;
	const auto fightingType = entt::type_id<CreatureFighting>();
	EXPECT_EQ(reflection::SetField(context, fightingType, &fighting, "fighter.health", Json(0.5)), "");
	EXPECT_EQ(reflection::SetField(context, fightingType, &fighting, "fighter.control",
	                               Json(static_cast<int>(creature_fight::Control::Player))),
	          "");
	EXPECT_EQ(fighting.fighter.control, creature_fight::Control::Player);
	EXPECT_EQ(reflection::SetField(context, fightingType, &fighting, "fighter.queue.moves",
	                               Json::parse(R"([{"move": {"kind": 3}, "chargeMs": 0}, {"move": {"kind": 1}}])")),
	          "");
	ASSERT_EQ(fighting.fighter.queue.Size(), 2u);
	EXPECT_EQ(fighting.fighter.queue.Moves()[0].move.kind, creature_fight::Move::Kind::Block);
	// The blow left without a charge waits for the button to be let go
	EXPECT_TRUE(fighting.fighter.queue.HasWaiting());
	const auto json = reflection::ComponentToJson(context, fightingType, &fighting);
	EXPECT_EQ(json["fighter"]["health"], Json(0.5));
	EXPECT_EQ(json["fighter"]["queue"]["waiting"], Json(true));
	EXPECT_EQ(Dump(json).find('<'), std::string::npos) << Dump(json);
	EXPECT_EQ(reflection::ReadField(context, fightingType, &fighting, "fighter.queue.moves.0.move.kind"), Json(3));
	EXPECT_EQ(reflection::ReadField(context, fightingType, &fighting, "fighter.queue.moves.1.chargeMs"), Json(nullptr));

	creature_desires::Desires desires;
	const auto desiresType = entt::type_id<creature_desires::Desires>();
	EXPECT_EQ(reflection::SetField(context, desiresType, &desires, "desires.Hunger.value", Json(0.75)), "");
	EXPECT_FLOAT_EQ(desires[creature_desires::Desire::Hunger].value, 0.75f);
	EXPECT_EQ(reflection::ReadField(context, desiresType, &desires, "desires.Hunger.value"), Json(0.75));
	EXPECT_TRUE(reflection::ComponentToJson(context, desiresType, &desires)["desires"].contains("Hunger"));
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
